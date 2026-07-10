using System;
using System.IO;
using System.IO.Ports;
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
    /// Handles image upload to ESP32 via serial with flow control.
    /// Thin wrapper around ChunkedSerialUploader (IMG_* protocol) that adds
    /// PNG/JPG -> RGB565A8 conversion.
    /// </summary>
    public class ImageUploader
    {
        private readonly ChunkedSerialUploader _uploader;

        public event EventHandler<UploadProgressEventArgs> ProgressChanged;
        public event EventHandler<string> LogMessage;

        /// <param name="port">Open serial port</param>
        /// <param name="writeLock">Shared lock guarding ALL writes to this port
        /// (data loop, commands, uploads) - prevents interleaved lines.</param>
        public ImageUploader(SerialPort port, object writeLock = null)
        {
            _uploader = new ChunkedSerialUploader(port, writeLock, "IMG");
            _uploader.ProgressChanged += (s, e) => ProgressChanged?.Invoke(this, e);
            _uploader.LogMessage += (s, m) => LogMessage?.Invoke(this, m);
        }

        /// <summary>
        /// Uploads an image file to the specified slot on the ESP32.
        /// </summary>
        public async Task<bool> UploadImageAsync(string imagePath, ImageSlot slot, CancellationToken ct = default)
        {
            if (!File.Exists(imagePath))
            {
                Log("Error: File not found: " + imagePath);
                return false;
            }

            try
            {
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
        public Task<bool> UploadDataAsync(byte[] data, uint crc32, ImageSlot slot, CancellationToken ct = default)
        {
            string beginCommand = $"IMG_BEGIN:{(int)slot}:{data?.Length ?? 0}";
            return _uploader.UploadAsync(beginCommand, data, crc32, ct);
        }

        private void Log(string message)
        {
            Console.WriteLine($"[ImageUploader] {message}");
            System.Diagnostics.Debug.WriteLine($"[ImageUploader] {message}");
            LogMessage?.Invoke(this, message);
        }
    }
}
