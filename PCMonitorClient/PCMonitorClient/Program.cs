using System;
using System.IO.Ports;
using System.Linq;
using System.Management;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;

namespace PCMonitorClient
{
    internal static class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Desert-Spec Monitor v1.0");
            Console.WriteLine("========================");

            // --- Hardware Collector ---
            Console.WriteLine("Initializing hardware collector...");
            using (var collector = new HardwareCollector())
            {
                Console.WriteLine("Hardware collector ready.");

                // --- Serial Port Discovery ---
                string portName = (args.Length > 0) ? args[0] : null;

                while (true)
                {
                    if (portName == null)
                    {
                        portName = FindEsp32Port();
                    }

                    if (portName == null)
                    {
                        Console.WriteLine("[Serial] No COM port found. Retrying in 3s...");
                        Thread.Sleep(3000);
                        continue;
                    }

                    Console.WriteLine("[Serial] Connecting to " + portName + " @ 115200...");

                    try
                    {
                        using (var port = new SerialPort(portName, 115200))
                        {
                            port.WriteTimeout = 2000;
                            port.ReadTimeout = 1000;
                            port.DtrEnable = true;
                            port.Open();
                            Console.WriteLine("[Serial] Connected!");

                            // Give ESP32 time to reset after DTR
                            Thread.Sleep(2000);

                            RunLoop(collector, port);
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine("[Serial] Error: " + ex.Message);
                        Console.WriteLine("[Serial] Reconnecting in 2s...");
                        Thread.Sleep(2000);
                    }

                    // If we had a CLI argument, keep retrying the same port
                    // If auto-detected, re-scan next iteration
                    if (args.Length == 0)
                        portName = null;
                }
            }
        }

        // ====================================================================
        //  Main data loop
        // ====================================================================

        private static void RunLoop(HardwareCollector collector, SerialPort port)
        {
            // Detect network type and speed once
            string netType = "LAN";
            string netSpeed = "1000 Mbps";
            DetectNetworkInfo(out netType, out netSpeed);

            while (true)
            {
                var s = collector.GetStats();

                // Exact protocol format from pc_monitor.py:
                // CPU:{cpu},CPUT:{cput:.1f},GPU:{gpu},GPUT:{gput:.1f},VRAM:{vu:.1f}/{vt:.1f},RAM:{ru:.1f}/{rt:.1f},NET:{type},SPEED:{speed},DOWN:{down:.1f},UP:{up:.1f}\n
                string data = string.Format(
                    System.Globalization.CultureInfo.InvariantCulture,
                    "CPU:{0:0},CPUT:{1:0.0},GPU:{2:0},GPUT:{3:0.0},VRAM:{4:0.0}/{5:0.0},RAM:{6:0.0}/{7:0.0},NET:{8},SPEED:{9},DOWN:{10:0.0},UP:{11:0.0}\n",
                    (int)s.CpuLoad,
                    s.CpuTemp,
                    (int)s.GpuLoad,
                    s.GpuTemp,
                    s.GpuVramUsed,
                    s.GpuVramTotal,
                    s.RamUsedGb,
                    s.RamTotalGb,
                    netType,
                    netSpeed,
                    s.NetDown,
                    s.NetUp);

                port.Write(data);
                port.BaseStream.Flush();

                // Console output (strip newline for display)
                Console.WriteLine("[TX] " + data.TrimEnd());

                Thread.Sleep(1000);
            }
        }

        // ====================================================================
        //  ESP32 Port Discovery (WMI)
        // ====================================================================

        private static string FindEsp32Port()
        {
            // Try WMI first: look for USB Serial / JTAG devices
            try
            {
                using (var searcher = new ManagementObjectSearcher(
                    "SELECT * FROM Win32_PnPEntity WHERE Caption LIKE '%(COM%'"))
                {
                    foreach (var obj in searcher.Get())
                    {
                        string caption = (obj["Caption"] ?? "").ToString();
                        string upper = caption.ToUpperInvariant();

                        // ESP32 shows up as "USB Serial Device", "USB-SERIAL CH340",
                        // "Silicon Labs CP210x", "USB JTAG/serial debug unit", etc.
                        if (upper.Contains("USB") || upper.Contains("SERIAL") || upper.Contains("JTAG") || upper.Contains("CP210"))
                        {
                            // Extract COM port name from caption like "USB Serial Device (COM5)"
                            int start = caption.IndexOf("(COM");
                            if (start >= 0)
                            {
                                int end = caption.IndexOf(")", start);
                                if (end > start)
                                {
                                    string port = caption.Substring(start + 1, end - start - 1);
                                    Console.WriteLine("[Serial] WMI found: " + caption + " -> " + port);
                                    return port;
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("[Serial] WMI query failed: " + ex.Message);
            }

            // Fallback: take the last (highest) COM port
            var ports = SerialPort.GetPortNames();
            if (ports.Length > 0)
            {
                var sorted = ports.OrderBy(p => p).ToArray();
                string last = sorted[sorted.Length - 1];
                Console.WriteLine("[Serial] Fallback: using " + last + " (highest port)");
                return last;
            }

            return null;
        }

        // ====================================================================
        //  Network Info (type + speed)
        // ====================================================================

        private static void DetectNetworkInfo(out string netType, out string netSpeed)
        {
            netType = "LAN";
            netSpeed = "1000 Mbps";

            try
            {
                var iface = NetworkInterface.GetAllNetworkInterfaces()
                    .Where(n => n.OperationalStatus == OperationalStatus.Up
                             && n.NetworkInterfaceType != NetworkInterfaceType.Loopback
                             && n.GetIPProperties().UnicastAddresses
                                 .Any(a => a.Address.AddressFamily == AddressFamily.InterNetwork))
                    .FirstOrDefault();

                if (iface == null) return;

                if (iface.NetworkInterfaceType == NetworkInterfaceType.Wireless80211)
                    netType = "WLAN";
                else
                    netType = "LAN";

                long speedMbps = iface.Speed / 1_000_000;
                netSpeed = speedMbps + " Mbps";
            }
            catch { /* keep defaults */ }
        }
    }
}
