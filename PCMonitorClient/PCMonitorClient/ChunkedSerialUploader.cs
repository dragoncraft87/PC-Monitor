using System;
using System.IO.Ports;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace PCMonitorClient
{
    /// <summary>
    /// Progress information for chunked uploads (images, firmware).
    /// </summary>
    public class UploadProgressEventArgs : EventArgs
    {
        public int BytesSent { get; set; }
        public int TotalBytes { get; set; }
        public int ChunksSent { get; set; }
        public int TotalChunks { get; set; }
        public string Status { get; set; }
        public double PercentComplete => TotalBytes > 0 ? (double)BytesSent / TotalBytes * 100.0 : 0;
    }

    /// <summary>
    /// Generic chunked hex upload over serial, shared by image (IMG_*) and
    /// firmware (FW_*) transfers.
    ///
    /// Protocol (PREFIX = "IMG" or "FW"):
    /// 1. Client: [begin command, e.g. IMG_BEGIN:slot:size or FW_BEGIN:size]
    /// 2. ESP:    PREFIX_OK:BEGIN
    /// 3. Client: PREFIX_DATA:[offset]:[hex]   (CHUNK_SIZE byte chunks)
    /// 4. ESP:    PREFIX_OK:DATA:[received]    or PREFIX_ERR:OFFSET:[expected]
    /// 5. Client: PREFIX_END:[CRC32-hex]
    /// 6. ESP:    PREFIX_OK:COMPLETE           or PREFIX_ERR:...
    ///
    /// Reliability:
    /// - All port WRITES go through a shared lock so they cannot interleave
    ///   with the stats data loop or user commands (corrupted lines).
    /// - Lost-ACK recovery: on PREFIX_ERR:OFFSET:[expected] the client resyncs
    ///   its send position to the offset the ESP reports, instead of failing.
    /// </summary>
    public class ChunkedSerialUploader
    {
        private const int CHUNK_SIZE = 1024;           // 1024 bytes = 2048 hex chars, fits ESP 4096 line buffer
        private const int RESPONSE_TIMEOUT_MS = 5000;  // Max wait for ESP response
        private const int MAX_RETRIES = 3;             // Retries per chunk (timeout case)

        private readonly SerialPort _port;
        private readonly object _writeLock;
        private readonly string _prefix;    // "IMG" or "FW"

        public event EventHandler<UploadProgressEventArgs> ProgressChanged;
        public event EventHandler<string> LogMessage;

        public ChunkedSerialUploader(SerialPort port, object writeLock, string prefix)
        {
            _port = port ?? throw new ArgumentNullException(nameof(port));
            _writeLock = writeLock ?? new object();
            _prefix = prefix;
        }

        /// <summary>
        /// Uploads data using the chunked protocol.
        /// </summary>
        /// <param name="beginCommand">Full begin command incl. newline-free args, e.g. "IMG_BEGIN:2:172816"</param>
        /// <param name="data">Payload bytes</param>
        /// <param name="crc32">CRC32 of payload (sent with END)</param>
        /// <param name="ct">Cancellation token</param>
        public async Task<bool> UploadAsync(string beginCommand, byte[] data, uint crc32, CancellationToken ct = default)
        {
            if (data == null || data.Length == 0)
            {
                Log("Error: No data to upload");
                return false;
            }

            if (!_port.IsOpen)
            {
                Log("Error: Serial port not open");
                return false;
            }

            int totalBytes = data.Length;
            int totalChunks = (totalBytes + CHUNK_SIZE - 1) / CHUNK_SIZE;
            string okBegin = _prefix + "_OK:BEGIN";
            string okData = _prefix + "_OK:DATA";
            string errOffsetPrefix = _prefix + "_ERR:OFFSET:";
            string errAny = _prefix + "_ERR";

            Log($"Starting upload: Size={totalBytes} bytes, Chunks={totalChunks}");

            try
            {
                // --- Phase 1: BEGIN ---
                Log("TX: " + beginCommand);
                DiscardStaleInput();

                string response = await SendAndAwaitLineAsync(beginCommand + "\n", new[] { okBegin }, errAny, ct);
                if (response == null || !response.Contains(okBegin))
                {
                    Log($"Error: {_prefix}_BEGIN not acknowledged" + (response != null ? $" (got: {response})" : ""));
                    SendAbort();
                    return false;
                }

                // --- Phase 2: DATA chunks with lost-ACK resync ---
                int bytesSent = 0;
                int chunksSent = 0;
                int timeoutRetries = 0;
                int resyncStalls = 0;   // consecutive resyncs without forward progress

                while (bytesSent < totalBytes)
                {
                    ct.ThrowIfCancellationRequested();

                    int chunkSize = Math.Min(CHUNK_SIZE, totalBytes - bytesSent);
                    string dataCmd = $"{_prefix}_DATA:{bytesSent}:{BytesToHex(data, bytesSent, chunkSize)}\n";

                    if (chunksSent % 20 == 0 || bytesSent + chunkSize >= totalBytes)
                    {
                        Log($"TX Chunk {chunksSent + 1}/{totalChunks}: offset={bytesSent}, size={chunkSize}");
                    }

                    response = await SendAndAwaitLineAsync(dataCmd, new[] { okData }, errAny, ct);

                    if (response != null && response.Contains(okData))
                    {
                        // ACK received - advance
                        bytesSent += chunkSize;
                        chunksSent++;
                        timeoutRetries = 0;
                        resyncStalls = 0;
                        ReportProgress(bytesSent, totalBytes, chunksSent, totalChunks, "Uploading...");
                        continue;
                    }

                    // RESYNC: ESP tells us which offset it expects. Happens when
                    // an ACK got lost (ESP already has the chunk) or a chunk got
                    // corrupted (ESP still waits at the old offset).
                    int errIdx = response != null ? response.IndexOf(errOffsetPrefix, StringComparison.Ordinal) : -1;
                    if (errIdx >= 0)
                    {
                        string offsetStr = response.Substring(errIdx + errOffsetPrefix.Length).Trim();
                        if (int.TryParse(offsetStr, out int expectedOffset)
                            && expectedOffset >= 0 && expectedOffset <= totalBytes)
                        {
                            // Guard against a stuck peer: resyncing to the same
                            // offset repeatedly means we make no progress.
                            resyncStalls = (expectedOffset == bytesSent) ? resyncStalls + 1 : 0;
                            if (resyncStalls >= MAX_RETRIES)
                            {
                                Log($"FATAL: Resync stalled at offset {bytesSent} - aborting upload");
                                SendAbort();
                                return false;
                            }

                            Log($"Resync: ESP expects offset {expectedOffset} (we were at {bytesSent})");
                            bytesSent = expectedOffset;
                            chunksSent = expectedOffset / CHUNK_SIZE;
                            timeoutRetries = 0;
                            continue;
                        }
                    }

                    if (response != null)
                    {
                        // Any other PREFIX_ERR is fatal (SIZE, NOMEM, WRITE, ...)
                        Log($"FATAL: ESP error: {response} - aborting upload");
                        SendAbort();
                        return false;
                    }

                    // Timeout - retry same chunk a few times
                    timeoutRetries++;
                    if (timeoutRetries >= MAX_RETRIES)
                    {
                        Log($"FATAL: Chunk at offset {bytesSent} timed out {MAX_RETRIES} times - aborting upload");
                        SendAbort();
                        return false;
                    }
                    Log($"No response for chunk at offset {bytesSent}, retrying ({timeoutRetries}/{MAX_RETRIES})...");
                    await Task.Delay(100, ct);
                }

                // --- Phase 3: END ---
                string endCmd = $"{_prefix}_END:{crc32:X8}";
                Log("TX: " + endCmd);

                response = await SendAndAwaitLineAsync(endCmd + "\n",
                    new[] { _prefix + "_OK:END", _prefix + "_OK:COMPLETE" }, errAny, ct);

                if (response == null || response.Contains(errAny))
                {
                    Log($"Error: {_prefix}_END not acknowledged" + (response != null ? $" (got: {response})" : " (possible CRC mismatch)"));
                    return false;
                }

                ReportProgress(totalBytes, totalBytes, totalChunks, totalChunks, "Complete!");
                Log("Upload complete!");
                return true;
            }
            catch (OperationCanceledException)
            {
                Log("Upload cancelled");
                SendAbort();
                return false;
            }
            catch (Exception ex)
            {
                Log("Upload error: " + ex.Message);
                SendAbort();
                return false;
            }
        }

        /// <summary>
        /// Thread-safe write via shared port lock.
        /// </summary>
        private void WritePort(string text)
        {
            lock (_writeLock)
            {
                _port.Write(text);
                _port.BaseStream.Flush();
            }
        }

        /// <summary>
        /// Discards stale RX data (ESP log lines etc.). Only used before BEGIN -
        /// discarding mid-transfer could eat a late ACK we need for resync.
        /// </summary>
        private void DiscardStaleInput()
        {
            try { _port.DiscardInBuffer(); } catch { }
        }

        /// <summary>
        /// Sends a command and waits for a line containing one of the success
        /// tokens or the error token. Returns the matched line, or null on timeout.
        /// Unrelated lines (ESP logs) are ignored.
        /// </summary>
        private async Task<string> SendAndAwaitLineAsync(string command, string[] successTokens, string errorToken, CancellationToken ct)
        {
            try
            {
                WritePort(command);

                DateTime timeout = DateTime.Now.AddMilliseconds(RESPONSE_TIMEOUT_MS);
                StringBuilder lineBuffer = new StringBuilder();

                while (DateTime.Now < timeout)
                {
                    ct.ThrowIfCancellationRequested();

                    if (_port.BytesToRead > 0)
                    {
                        char c = (char)_port.ReadByte();
                        if (c == '\n' || c == '\r')
                        {
                            if (lineBuffer.Length > 0)
                            {
                                string line = lineBuffer.ToString();
                                lineBuffer.Clear();

                                foreach (string token in successTokens)
                                {
                                    if (line.Contains(token))
                                    {
                                        return line;
                                    }
                                }

                                if (line.Contains(errorToken))
                                {
                                    Log("ESP Error: " + line);
                                    return line;
                                }

                                // Unrelated line (ESP log output) - ignore
                            }
                        }
                        else
                        {
                            lineBuffer.Append(c);
                        }
                    }
                    else
                    {
                        await Task.Delay(10, ct);
                    }
                }

                Log($"Timeout waiting for: {string.Join(" or ", successTokens)}");
                return null;
            }
            catch (OperationCanceledException)
            {
                throw;
            }
            catch (Exception ex)
            {
                Log("Communication error: " + ex.Message);
                return null;
            }
        }

        /// <summary>
        /// Sends abort command to reset ESP upload state.
        /// </summary>
        public void SendAbort()
        {
            try
            {
                WritePort(_prefix + "_ABORT\n");
            }
            catch { }
        }

        private static string BytesToHex(byte[] data, int offset, int length)
        {
            var sb = new StringBuilder(length * 2);
            for (int i = 0; i < length; i++)
            {
                sb.Append(data[offset + i].ToString("X2"));
            }
            return sb.ToString();
        }

        private void ReportProgress(int bytesSent, int totalBytes, int chunksSent, int totalChunks, string status)
        {
            ProgressChanged?.Invoke(this, new UploadProgressEventArgs
            {
                BytesSent = bytesSent,
                TotalBytes = totalBytes,
                ChunksSent = chunksSent,
                TotalChunks = totalChunks,
                Status = status
            });
        }

        private void Log(string message)
        {
            Console.WriteLine($"[{_prefix}-Upload] {message}");
            System.Diagnostics.Debug.WriteLine($"[{_prefix}-Upload] {message}");
            LogMessage?.Invoke(this, message);
        }
    }
}
