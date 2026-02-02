using System;
using System.IO;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

// ImageSharp 2.1.13 - Latest version compatible with .NET Framework 4.7.2
// Note: ImageSharp 3.x requires .NET 6+ and is NOT compatible!

namespace PCMonitorClient
{
    /// <summary>
    /// Converts images to RGB565A8 format for ESP32 LVGL displays.
    /// RGB565A8 = 16-bit color (RGB565) + 8-bit alpha channel (separate plane)
    /// Total: 3 bytes per pixel (2 for color, 1 for alpha)
    ///
    /// Output format: 16-byte SCARAB header + pixel data
    /// </summary>
    public static class ImageConverter
    {
        public const int TARGET_WIDTH = 240;
        public const int TARGET_HEIGHT = 240;

        // SCARAB image header constants (must match ESP32 scarab_img_header_t)
        private const uint SCARAB_IMG_MAGIC = 0x53434152;  // "SCAR" in little-endian
        private const int SCARAB_HEADER_SIZE = 16;
        private const byte SCARAB_FORMAT_RGB565A8 = 1;     // 0=RGB565, 1=RGB565A8
        private const byte SCARAB_VERSION = 1;

        /// <summary>
        /// Result of image conversion
        /// </summary>
        public class ConversionResult
        {
            /// <summary>RGB565 color data (2 bytes per pixel, row-major)</summary>
            public byte[] ColorData { get; set; }

            /// <summary>Alpha channel data (1 byte per pixel, row-major)</summary>
            public byte[] AlphaData { get; set; }

            /// <summary>Combined data: 16-byte SCARAB header + ColorData + AlphaData</summary>
            public byte[] CombinedData { get; set; }

            /// <summary>Image width (always 240)</summary>
            public int Width { get; set; }

            /// <summary>Image height (always 240)</summary>
            public int Height { get; set; }

            /// <summary>CRC32 checksum of CombinedData</summary>
            public uint Crc32 { get; set; }
        }

        /// <summary>
        /// Converts an image file to RGB565A8 format.
        /// Image is resized to 240x240, maintaining aspect ratio with transparent padding.
        /// </summary>
        /// <param name="imagePath">Path to source image (PNG, JPG, etc.)</param>
        /// <returns>Conversion result with RGB565A8 data</returns>
        public static ConversionResult ConvertToRgb565A8(string imagePath)
        {
            using (var image = Image.Load<Rgba32>(imagePath))
            {
                return ConvertImage(image);
            }
        }

        /// <summary>
        /// Converts an image from a stream to RGB565A8 format.
        /// </summary>
        public static ConversionResult ConvertToRgb565A8(Stream stream)
        {
            using (var image = Image.Load<Rgba32>(stream))
            {
                return ConvertImage(image);
            }
        }

        /// <summary>
        /// Converts an image from byte array to RGB565A8 format.
        /// </summary>
        public static ConversionResult ConvertToRgb565A8(byte[] imageData)
        {
            using (var ms = new MemoryStream(imageData))
            using (var image = Image.Load<Rgba32>(ms))
            {
                return ConvertImage(image);
            }
        }

        private static ConversionResult ConvertImage(Image<Rgba32> sourceImage)
        {
            int srcWidth = sourceImage.Width;
            int srcHeight = sourceImage.Height;

            // NO-UPSCALE LOGIC: If image is smaller than 240x240, do NOT upscale
            // Only downscale if larger than target
            bool needsResize = (srcWidth > TARGET_WIDTH || srcHeight > TARGET_HEIGHT);

            int newWidth, newHeight;
            if (needsResize)
            {
                // Calculate downscale dimensions (fit within 240x240, maintain aspect ratio)
                float scale = Math.Min((float)TARGET_WIDTH / srcWidth, (float)TARGET_HEIGHT / srcHeight);
                newWidth = (int)(srcWidth * scale);
                newHeight = (int)(srcHeight * scale);
            }
            else
            {
                // Keep original size (no upscaling)
                newWidth = srcWidth;
                newHeight = srcHeight;
            }

            // Create 240x240 canvas with TRANSPARENT background (v2.3)
            // ESP32 renders transparent areas over gui_settings.ss_bg_color
            using (var canvas = new Image<Rgba32>(TARGET_WIDTH, TARGET_HEIGHT, new Rgba32(0, 0, 0, 0)))
            {
                // Only resize if needed (downscaling)
                if (needsResize)
                {
                    sourceImage.Mutate(x => x.Resize(newWidth, newHeight));
                }

                // Center on canvas
                int offsetX = (TARGET_WIDTH - newWidth) / 2;
                int offsetY = (TARGET_HEIGHT - newHeight) / 2;

                // Draw image onto canvas (centered)
                canvas.Mutate(x => x.DrawImage(sourceImage, new Point(offsetX, offsetY), 1f));

                // Convert to RGB565A8
                return ExtractRgb565A8(canvas);
            }
        }

        /// <summary>
        /// Extracts RGB565A8 data in LVGL v9 PLANAR format.
        ///
        /// LVGL v9 RGB565A8 (LV_COLOR_FORMAT_RGB565A8 = 20 / 0x14) layout:
        /// ┌─────────────────────────────────────────────────────────┐
        /// │ Block 1: RGB565 color data (W × H × 2 bytes)            │
        /// │   - Row 0: Pixel[0,0], Pixel[1,0], ... Pixel[W-1,0]     │
        /// │   - Row 1: Pixel[0,1], Pixel[1,1], ... Pixel[W-1,1]     │
        /// │   - ...                                                  │
        /// │   - Each pixel: 2 bytes Little-Endian (low, high)       │
        /// ├─────────────────────────────────────────────────────────┤
        /// │ Block 2: Alpha data (W × H × 1 byte)                    │
        /// │   - Row 0: Alpha[0,0], Alpha[1,0], ... Alpha[W-1,0]     │
        /// │   - Row 1: Alpha[0,1], Alpha[1,1], ... Alpha[W-1,1]     │
        /// │   - ...                                                  │
        /// │   - Each alpha: 1 byte (0-255)                          │
        /// └─────────────────────────────────────────────────────────┘
        ///
        /// IMPORTANT: This is PLANAR format (NOT interleaved!)
        /// - Color and alpha are stored in separate contiguous blocks
        /// - Stride for LVGL = width × 2 (RGB565 row width)
        /// </summary>
        private static ConversionResult ExtractRgb565A8(Image<Rgba32> image)
        {
            int pixelCount = TARGET_WIDTH * TARGET_HEIGHT;  // 57,600 pixels

            // Block 1: RGB565 color data - 2 bytes per pixel (115,200 bytes total)
            byte[] rgbData = new byte[pixelCount * 2];

            // Block 2: Alpha channel data - 1 byte per pixel (57,600 bytes total)
            byte[] alphaData = new byte[pixelCount];

            int rgbIdx = 0;
            int alphaIdx = 0;

            // Iterate row by row, column by column (row-major order)
            for (int y = 0; y < TARGET_HEIGHT; y++)
            {
                for (int x = 0; x < TARGET_WIDTH; x++)
                {
                    Rgba32 pixel = image[x, y];

                    // Convert RGB888 to RGB565 (5-6-5 bits)
                    ushort rgb565 = ToRgb565(pixel.R, pixel.G, pixel.B);

                    // Store RGB565 in Little-Endian byte order (ESP32 native)
                    rgbData[rgbIdx++] = (byte)(rgb565 & 0xFF);         // Low byte first
                    rgbData[rgbIdx++] = (byte)((rgb565 >> 8) & 0xFF);  // High byte second

                    // Store alpha channel (0-255)
                    alphaData[alphaIdx++] = pixel.A;
                }
            }

            // Total pixel data: RGB block + Alpha block = 172,800 bytes
            int pixelDataSize = rgbData.Length + alphaData.Length;

            // Final output: 16-byte SCARAB header + pixel data
            byte[] combinedData = new byte[SCARAB_HEADER_SIZE + pixelDataSize];

            // ═══════════════════════════════════════════════════════════
            // Build 16-byte SCARAB header (matches ESP32 scarab_img_header_t)
            // ═══════════════════════════════════════════════════════════
            int offset = 0;

            // [0-3] magic: uint32 - "SCAR" = 0x53434152 (Little-Endian)
            combinedData[offset++] = 0x52;  // 'R'
            combinedData[offset++] = 0x41;  // 'A'
            combinedData[offset++] = 0x43;  // 'C'
            combinedData[offset++] = 0x53;  // 'S'

            // [4-5] width: uint16 (Little-Endian)
            combinedData[offset++] = (byte)(TARGET_WIDTH & 0xFF);
            combinedData[offset++] = (byte)((TARGET_WIDTH >> 8) & 0xFF);

            // [6-7] height: uint16 (Little-Endian)
            combinedData[offset++] = (byte)(TARGET_HEIGHT & 0xFF);
            combinedData[offset++] = (byte)((TARGET_HEIGHT >> 8) & 0xFF);

            // [8] format: uint8 - 1 = SCARAB_FMT_RGB565A8
            combinedData[offset++] = SCARAB_FORMAT_RGB565A8;

            // [9] version: uint8
            combinedData[offset++] = SCARAB_VERSION;

            // [10-11] reserved: uint16 - padding for 4-byte alignment
            combinedData[offset++] = 0;
            combinedData[offset++] = 0;

            // [12-15] data_size: uint32 - pixel data size (Little-Endian)
            combinedData[offset++] = (byte)(pixelDataSize & 0xFF);
            combinedData[offset++] = (byte)((pixelDataSize >> 8) & 0xFF);
            combinedData[offset++] = (byte)((pixelDataSize >> 16) & 0xFF);
            combinedData[offset++] = (byte)((pixelDataSize >> 24) & 0xFF);

            // ═══════════════════════════════════════════════════════════
            // Append pixel data in PLANAR format: RGB block, then Alpha block
            // ═══════════════════════════════════════════════════════════
            Buffer.BlockCopy(rgbData, 0, combinedData, SCARAB_HEADER_SIZE, rgbData.Length);
            Buffer.BlockCopy(alphaData, 0, combinedData, SCARAB_HEADER_SIZE + rgbData.Length, alphaData.Length);

            return new ConversionResult
            {
                ColorData = rgbData,
                AlphaData = alphaData,
                CombinedData = combinedData,
                Width = TARGET_WIDTH,
                Height = TARGET_HEIGHT,
                Crc32 = ComputeCrc32(combinedData)
            };
        }

        /// <summary>
        /// Converts 8-bit RGB components to RGB565 format.
        /// </summary>
        private static ushort ToRgb565(byte r, byte g, byte b)
        {
            // RGB565: RRRRR GGGGGG BBBBB
            // Red:   5 bits (bits 15-11)
            // Green: 6 bits (bits 10-5)
            // Blue:  5 bits (bits 4-0)
            ushort r5 = (ushort)((r >> 3) & 0x1F);
            ushort g6 = (ushort)((g >> 2) & 0x3F);
            ushort b5 = (ushort)((b >> 3) & 0x1F);

            return (ushort)((r5 << 11) | (g6 << 5) | b5);
        }

        /// <summary>
        /// Computes CRC32 checksum of byte array.
        /// </summary>
        public static uint ComputeCrc32(byte[] data)
        {
            uint crc = 0xFFFFFFFF;

            foreach (byte b in data)
            {
                crc ^= b;
                for (int i = 0; i < 8; i++)
                {
                    crc = (crc >> 1) ^ (0xEDB88320 & ~((crc & 1) - 1));
                }
            }

            return ~crc;
        }

        /// <summary>
        /// Validates that a file is a supported image format.
        /// </summary>
        public static bool IsSupportedImage(string filePath)
        {
            try
            {
                string ext = Path.GetExtension(filePath).ToLowerInvariant();
                return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                       ext == ".gif" || ext == ".bmp" || ext == ".webp";
            }
            catch
            {
                return false;
            }
        }
    }
}
