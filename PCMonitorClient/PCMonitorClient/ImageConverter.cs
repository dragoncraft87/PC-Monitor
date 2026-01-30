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
    /// </summary>
    public static class ImageConverter
    {
        public const int TARGET_WIDTH = 240;
        public const int TARGET_HEIGHT = 240;

        /// <summary>
        /// Result of image conversion
        /// </summary>
        public class ConversionResult
        {
            /// <summary>RGB565 color data (2 bytes per pixel, row-major)</summary>
            public byte[] ColorData { get; set; }

            /// <summary>Alpha channel data (1 byte per pixel, row-major)</summary>
            public byte[] AlphaData { get; set; }

            /// <summary>Combined RGB565A8 data (ColorData + AlphaData)</summary>
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
            // Calculate resize dimensions (fit within 240x240, maintain aspect ratio)
            int srcWidth = sourceImage.Width;
            int srcHeight = sourceImage.Height;

            float scale = Math.Min((float)TARGET_WIDTH / srcWidth, (float)TARGET_HEIGHT / srcHeight);
            int newWidth = (int)(srcWidth * scale);
            int newHeight = (int)(srcHeight * scale);

            // Create 240x240 canvas with transparent background
            using (var canvas = new Image<Rgba32>(TARGET_WIDTH, TARGET_HEIGHT, new Rgba32(0, 0, 0, 0)))
            {
                // Resize source image
                sourceImage.Mutate(x => x.Resize(newWidth, newHeight));

                // Center on canvas
                int offsetX = (TARGET_WIDTH - newWidth) / 2;
                int offsetY = (TARGET_HEIGHT - newHeight) / 2;

                // Draw resized image onto canvas
                canvas.Mutate(x => x.DrawImage(sourceImage, new Point(offsetX, offsetY), 1f));

                // Convert to RGB565A8
                return ExtractRgb565A8(canvas);
            }
        }

        private static ConversionResult ExtractRgb565A8(Image<Rgba32> image)
        {
            int pixelCount = TARGET_WIDTH * TARGET_HEIGHT;

            // RGB565: 2 bytes per pixel (57,600 bytes total)
            byte[] colorData = new byte[pixelCount * 2];

            // Alpha: 1 byte per pixel (57,600 bytes total)
            byte[] alphaData = new byte[pixelCount];

            int colorIdx = 0;
            int alphaIdx = 0;

            for (int y = 0; y < TARGET_HEIGHT; y++)
            {
                for (int x = 0; x < TARGET_WIDTH; x++)
                {
                    Rgba32 pixel = image[x, y];

                    // Convert to RGB565 (5 bits red, 6 bits green, 5 bits blue)
                    ushort rgb565 = ToRgb565(pixel.R, pixel.G, pixel.B);

                    // Store as little-endian (ESP32 is little-endian)
                    colorData[colorIdx++] = (byte)(rgb565 & 0xFF);
                    colorData[colorIdx++] = (byte)((rgb565 >> 8) & 0xFF);

                    // Store alpha
                    alphaData[alphaIdx++] = pixel.A;
                }
            }

            // Combine: ColorData followed by AlphaData (172,800 bytes total)
            byte[] combinedData = new byte[colorData.Length + alphaData.Length];
            Buffer.BlockCopy(colorData, 0, combinedData, 0, colorData.Length);
            Buffer.BlockCopy(alphaData, 0, combinedData, colorData.Length, alphaData.Length);

            return new ConversionResult
            {
                ColorData = colorData,
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
