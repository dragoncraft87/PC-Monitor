using System;
using System.IO;
using System.IO.Ports;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace PCMonitorClient
{
    /// <summary>
    /// Upload slot identifiers matching ESP32 screensaver slots.
    /// </summary>
    public enum ImageSlot
    {
        CPU = 0,
        GPU = 1,
        RAM = 2,
        NET = 3
    }

    /// <summary>
    /// Progress information for image upload.
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
    /// Handles image upload to ESP32 via serial with flow control.
    ///
    /// Protocol:
    /// 1. Client sends: IMG_BEGIN:[Slot]:[Size]
    /// 2. ESP responds:  IMG_OK:BEGIN
    /// 3. Client sends: IMG_DATA:[Offset]:[HexData] (512 byte chunks)
    /// 4. ESP responds:  IMG_OK:DATA after each chunk
    /// 5. Client sends: IMG_END:[CRC32]
    /// 6. ESP responds:  IMG_OK:END or IMG_ERR:CRC
    /// </summary>
    public class ImageUploader
    {
        private const int CHUNK_SIZE = 512;         // Bytes per chunk (512 = 1024 hex chars + ~20 overhead)
        private const int RESPONSE_TIMEOUT_MS = 5000;  // Max wait for ESP response
        private const int MAX_RETRIES = 3;          // Retries per chunk

        private readonly SerialPort _port;

        public event EventHandler<UploadProgressEventArgs> ProgressChanged;
        public event EventHandler<string> LogMessage;

        public ImageUploader(SerialPort port)
        {
            _port = port ?? throw new ArgumentNullException(nameof(port));
        }

        /// <summary>
        /// Uploads an image to the specified slot on the ESP32.
        /// </summary>
        /// <param name="imagePath">Path to source image file</param>
        /// <param name="slot">Target screensaver slot</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>True if upload succeeded</returns>
        public async Task<bool> UploadImageAsync(string imagePath, ImageSlot slot, CancellationToken ct = default)
        {
            if (!File.Exists(imagePath))
            {
                Log("Error: File not found: " + imagePath);
                return false;
            }

            try
            {
                // Convert image to RGB565A8
                Log("Converting image to RGB565A8...");
                var result = ImageConverter.ConvertToRgb565A8(imagePath);
                Log($"Converted: {result.CombinedData.Length} bytes, CRC32: {result.Crc32:X8}");

                return await UploadDataAsync(result.CombinedData, result.Crc32, slot, ct);
            }
            catch (Exception ex)
            {
                Log("Conversion error: " + ex.Message);
                return false;
            }
        }

        /// <summary>
        /// Uploads pre-converted RGB565A8 data to the ESP32.
        /// </summary>
        public async Task<bool> UploadDataAsync(byte[] data, uint crc32, ImageSlot slot, CancellationToken ct = default)
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

            Log($"Starting upload: Slot={slot}, Size={totalBytes} bytes, Chunks={totalChunks}");

            try
            {
                // --- Phase 1: IMG_BEGIN ---
                string beginCmd = $"IMG_BEGIN:{(int)slot}:{totalBytes}\n";
                Log("TX: " + beginCmd.Trim());

                if (!await SendAndWaitForResponseAsync(beginCmd, "IMG_OK:BEGIN", ct))
                {
                    Log("Error: IMG_BEGIN not acknowledged");
                    return false;
                }

                // --- Phase 2: IMG_DATA chunks ---
                int bytesSent = 0;
                int chunksSent = 0;

                while (bytesSent < totalBytes)
                {
                    ct.ThrowIfCancellationRequested();

                    int chunkSize = Math.Min(CHUNK_SIZE, totalBytes - bytesSent);
                    string hexData = BytesToHex(data, bytesSent, chunkSize);

                    string dataCmd = $"IMG_DATA:{bytesSent}:{hexData}\n";

                    // Log every 20th chunk or last chunk for progress visibility
                    if (chunksSent % 20 == 0 || bytesSent + chunkSize >= totalBytes)
                    {
                        Log($"TX Chunk {chunksSent + 1}/{totalChunks}: offset={bytesSent}, size={chunkSize}");
                    }

                    bool chunkSuccess = false;
                    for (int retry = 0; retry < MAX_RETRIES && !chunkSuccess; retry++)
                    {
                        if (retry > 0)
                            Log($"Retrying chunk at offset {bytesSent} (attempt {retry + 1}/{MAX_RETRIES})");

                        if (await SendAndWaitForResponseAsync(dataCmd, "IMG_OK:DATA", ct))
                        {
                            chunkSuccess = true;
                        }
                        else
                        {
                            Log($"No ACK for chunk at offset {bytesSent}, will retry...");
                            await Task.Delay(100, ct);
                        }
                    }

                    if (!chunkSuccess)
                    {
                        Log($"FATAL: Chunk at offset {bytesSent} failed after {MAX_RETRIES} retries - aborting upload");
                        SendAbort();
                        return false;
                    }

                    bytesSent += chunkSize;
                    chunksSent++;

                    ReportProgress(bytesSent, totalBytes, chunksSent, totalChunks, "Uploading...");

                    // Small delay to let ESP process (prevents buffer overrun)
                    await Task.Delay(5, ct);
                }

                // --- Phase 3: IMG_END ---
                string endCmd = $"IMG_END:{crc32:X8}\n";
                Log("TX: " + endCmd.Trim());

                // ESP32 responds with either IMG_OK:END or IMG_OK:COMPLETE:N
                if (!await SendAndWaitForResponseAsync(endCmd, new[] { "IMG_OK:END", "IMG_OK:COMPLETE" }, ct))
                {
                    // Check for CRC error
                    Log("Error: IMG_END not acknowledged (possible CRC mismatch)");
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
        /// Sends command and waits for expected response.
        /// </summary>
        private async Task<bool> SendAndWaitForResponseAsync(string command, string expectedResponse, CancellationToken ct)
        {
            return await SendAndWaitForResponseAsync(command, new[] { expectedResponse }, ct);
        }

        /// <summary>
        /// Sends command and waits for any of the expected responses.
        /// </summary>
        private async Task<bool> SendAndWaitForResponseAsync(string command, string[] expectedResponses, CancellationToken ct)
        {
            try
            {
                _port.DiscardInBuffer();
                _port.Write(command);
                _port.BaseStream.Flush();

                // Wait for response with timeout
                DateTime timeout = DateTime.Now.AddMilliseconds(RESPONSE_TIMEOUT_MS);
                StringBuilder lineBuffer = new StringBuilder();
                int waitLoops = 0;

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
                                string response = lineBuffer.ToString();

                                // Check if response matches any expected response
                                foreach (string expected in expectedResponses)
                                {
                                    if (response.Contains(expected))
                                    {
                                        Log($"RX: {response} (matched: {expected})");
                                        return true;
                                    }
                                }

                                if (response.Contains("IMG_ERR"))
                                {
                                    Log("ESP Error: " + response);
                                    return false;
                                }
                                else
                                {
                                    // Log unexpected responses for debugging
                                    Log($"RX (ignored): {response}");
                                }
                                lineBuffer.Clear();
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
                        waitLoops++;

                        // Log every second of waiting
                        if (waitLoops % 100 == 0)
                        {
                            Log($"Waiting for response... ({waitLoops * 10}ms elapsed)");
                        }
                    }
                }

                // Log what was in the buffer when timeout occurred
                if (lineBuffer.Length > 0)
                {
                    Log($"Timeout with partial data in buffer: '{lineBuffer}'");
                }
                else
                {
                    Log($"Timeout waiting for: {string.Join(" or ", expectedResponses)} (no data received)");
                }
                return false;
            }
            catch (Exception ex)
            {
                Log("Communication error: " + ex.Message);
                return false;
            }
        }

        /// <summary>
        /// Sends abort command to reset ESP upload state.
        /// </summary>
        private void SendAbort()
        {
            try
            {
                _port.Write("IMG_ABORT\n");
                _port.BaseStream.Flush();
            }
            catch { }
        }

        /// <summary>
        /// Converts byte array segment to hexadecimal string.
        /// </summary>
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
            // Debug to console as fallback
            Console.WriteLine($"[ImageUploader] {message}");
            System.Diagnostics.Debug.WriteLine($"[ImageUploader] {message}");

            LogMessage?.Invoke(this, message);
        }
    }
}
