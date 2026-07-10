using System;
using System.IO;
using System.IO.Ports;
using System.Threading;
using System.Threading.Tasks;

namespace PCMonitorClient
{
    /// <summary>
    /// Pushes a new firmware image (.bin from 'idf.py build') to the ESP32
    /// over the existing USB serial link using the FW_* protocol.
    ///
    /// The ESP writes the image to its inactive OTA slot, verifies CRC32 and
    /// the ESP-IDF image signature, switches the boot partition and reboots.
    /// The device disconnects after FW_OK:COMPLETE - the client's reconnect
    /// loop picks it up again automatically (~5-10s later).
    ///
    /// Requires firmware >= 2.4 (OTA partition table). Devices still on the
    /// old factory-only partition table need one final cable flash.
    /// </summary>
    public class FirmwareUploader
    {
        /// <summary>ESP-IDF app image magic byte (first byte of every valid .bin)</summary>
        private const byte ESP_IMAGE_MAGIC = 0xE9;

        /// <summary>Plausibility limits for firmware size (OTA slot is 4MB)</summary>
        private const int FW_MIN_SIZE = 0x10000;        // 64 KB
        private const int FW_MAX_SIZE = 4 * 1024 * 1024;

        private readonly ChunkedSerialUploader _uploader;

        public event EventHandler<UploadProgressEventArgs> ProgressChanged;
        public event EventHandler<string> LogMessage;

        /// <param name="port">Open serial port</param>
        /// <param name="writeLock">Shared lock guarding ALL writes to this port</param>
        public FirmwareUploader(SerialPort port, object writeLock = null)
        {
            _uploader = new ChunkedSerialUploader(port, writeLock, "FW");
            _uploader.ProgressChanged += (s, e) => ProgressChanged?.Invoke(this, e);
            _uploader.LogMessage += (s, m) => LogMessage?.Invoke(this, m);
        }

        /// <summary>
        /// Validates that a file looks like an ESP-IDF app image.
        /// Returns null if OK, otherwise a human-readable error.
        /// </summary>
        public static string ValidateFirmwareFile(string path)
        {
            try
            {
                if (!File.Exists(path))
                    return "File not found.";

                var info = new FileInfo(path);
                if (info.Length < FW_MIN_SIZE)
                    return $"File too small ({info.Length} bytes) - not a firmware image.";
                if (info.Length > FW_MAX_SIZE)
                    return $"File too large ({info.Length} bytes) - exceeds 4MB OTA slot.";

                using (var fs = File.OpenRead(path))
                {
                    int first = fs.ReadByte();
                    if (first != ESP_IMAGE_MAGIC)
                        return $"Not an ESP-IDF app image (magic byte 0x{first:X2}, expected 0xE9). " +
                               "Select the app .bin from 'idf.py build' (e.g. pc-monitor-poc.bin), " +
                               "not bootloader.bin, partition-table.bin or storage.bin.";
                }

                return null;
            }
            catch (Exception ex)
            {
                return "Cannot read file: " + ex.Message;
            }
        }

        /// <summary>
        /// Uploads a firmware .bin file. On success the ESP reboots into the
        /// new firmware and the serial connection drops (expected!).
        /// </summary>
        public async Task<bool> UploadFirmwareAsync(string binPath, CancellationToken ct = default)
        {
            string error = ValidateFirmwareFile(binPath);
            if (error != null)
            {
                Log("Error: " + error);
                return false;
            }

            byte[] data = File.ReadAllBytes(binPath);
            uint crc32 = ImageConverter.ComputeCrc32(data);

            Log($"Firmware image: {Path.GetFileName(binPath)}, {data.Length:N0} bytes, CRC32: {crc32:X8}");

            bool success = await _uploader.UploadAsync($"FW_BEGIN:{data.Length}", data, crc32, ct);

            if (success)
            {
                Log("Firmware accepted - device is verifying and rebooting now.");
            }
            return success;
        }

        private void Log(string message)
        {
            Console.WriteLine($"[FirmwareUploader] {message}");
            System.Diagnostics.Debug.WriteLine($"[FirmwareUploader] {message}");
            LogMessage?.Invoke(this, message);
        }
    }
}
