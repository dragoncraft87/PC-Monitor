using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Management;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;
using LibreHardwareMonitor.Hardware;
using NvAPIWrapper;
using NvAPIWrapper.GPU;

namespace PCMonitorClient
{
    public sealed class UpdateVisitor : IVisitor
    {
        public void VisitComputer(IComputer computer) { computer.Traverse(this); }

        public void VisitHardware(IHardware hardware)
        {
            hardware.Update();
            foreach (var sub in hardware.SubHardware)
                sub.Accept(this);
        }

        public void VisitSensor(ISensor sensor) { }
        public void VisitParameter(IParameter parameter) { }
    }

    public sealed class HardwareCollector : IDisposable
    {
        private Computer _computer;
        private readonly UpdateVisitor _visitor = new UpdateVisitor();
        private bool _nvApiAvailable;
        private PhysicalGPU _gpu;

        // Network perf counters
        private PerformanceCounter _netBytesRecv;
        private PerformanceCounter _netBytesSent;

        // Lite Mode: WMI-based CPU fallback
        private PerformanceCounter _cpuCounter;

        /// <summary>
        /// True if running without admin rights (limited sensor access).
        /// </summary>
        public bool IsLiteMode { get; private set; }

        /// <summary>
        /// Human-readable status message for UI.
        /// </summary>
        public string InitStatus { get; private set; }

        public HardwareCollector()
        {
            Console.WriteLine("  [System]   Process Architecture: " + (Environment.Is64BitProcess ? "x64" : "x86"));
            Console.WriteLine("  [System]   Admin: " + (IsAdmin() ? "YES" : "NO"));

            IsLiteMode = false;
            InitStatus = "";

            // --- Try NvAPI first (works without admin for basic GPU info) ---
            InitNvApi();

            // --- Try LibreHardwareMonitor (requires admin for Ring0) ---
            if (!InitLibreHardwareMonitor())
            {
                // Ring0 failed - switch to Lite Mode
                IsLiteMode = true;
                InitStatus = "Lite Mode (no admin rights)";
                Console.WriteLine("  [LHM]      Switching to LITE MODE");

                // Initialize WMI-based CPU counter as fallback
                InitLiteModeFallbacks();
            }
            else
            {
                InitStatus = "Full Mode (admin)";
            }

            // --- Network PerformanceCounters (works without admin) ---
            InitNetwork();
        }

        // ========================================================================
        //  Initialization
        // ========================================================================

        private void InitNvApi()
        {
            try
            {
                NVIDIA.Initialize();
                var gpus = PhysicalGPU.GetPhysicalGPUs();
                if (gpus.Length > 0)
                {
                    _gpu = gpus[0];
                    _nvApiAvailable = true;
                    Console.WriteLine("  [NvAPI]    OK - " + _gpu.FullName);
                }
            }
            catch (Exception ex)
            {
                _nvApiAvailable = false;
                Console.WriteLine("  [NvAPI]    FAIL - " + ex.Message);
            }
        }

        private bool InitLibreHardwareMonitor()
        {
            try
            {
                _computer = new Computer
                {
                    IsCpuEnabled = true,
                    IsMemoryEnabled = true,
                    IsMotherboardEnabled = true,
                    IsControllerEnabled = true,
                    IsGpuEnabled = !_nvApiAvailable,
                    IsStorageEnabled = false,
                    IsNetworkEnabled = false
                };

                // This is where Ring0 driver loads - may fail without admin
                _computer.Open();
                Console.WriteLine("  [LHM]      Computer opened (Ring0 driver loaded)");

                // Wait for Ring0 driver to finish scanning buses
                Console.WriteLine("  [LHM]      Waiting 2s for bus scan...");
                Thread.Sleep(2000);

                // First update pass
                _computer.Accept(_visitor);

                // Second update pass — some sensors need two reads
                Thread.Sleep(500);
                _computer.Accept(_visitor);

                // Debug: dump hardware nodes
                Console.WriteLine("  [LHM]      --- Hardware Dump ---");
                foreach (var hw in _computer.Hardware)
                {
                    DumpHardwareTemps(hw, 0);
                }
                Console.WriteLine("  [LHM]      --- End Dump ---");

                return true;
            }
            catch (UnauthorizedAccessException ex)
            {
                Console.WriteLine("  [LHM]      FAIL (UnauthorizedAccess) - " + ex.Message);
                SafeCloseComputer();
                return false;
            }
            catch (Exception ex)
            {
                // Ring0 driver failed to load (common without admin)
                Console.WriteLine("  [LHM]      FAIL - " + ex.Message);
                SafeCloseComputer();
                return false;
            }
        }

        private void SafeCloseComputer()
        {
            if (_computer != null)
            {
                try { _computer.Close(); } catch { }
                _computer = null;
            }
        }

        private void InitLiteModeFallbacks()
        {
            // WMI-based CPU usage counter (works without admin)
            try
            {
                _cpuCounter = new PerformanceCounter("Processor", "% Processor Time", "_Total");
                _cpuCounter.NextValue(); // First call returns 0
                Console.WriteLine("  [Lite]     CPU PerformanceCounter initialized");
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [Lite]     CPU counter FAIL - " + ex.Message);
            }

            // Initialize a minimal Computer for RAM only (usually works without Ring0)
            try
            {
                _computer = new Computer
                {
                    IsCpuEnabled = false,
                    IsMemoryEnabled = true,
                    IsMotherboardEnabled = false,
                    IsControllerEnabled = false,
                    IsGpuEnabled = false,
                    IsStorageEnabled = false,
                    IsNetworkEnabled = false
                };
                _computer.Open();
                _computer.Accept(_visitor);
                Console.WriteLine("  [Lite]     RAM monitoring OK");
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [Lite]     RAM monitoring FAIL - " + ex.Message);
                SafeCloseComputer();
            }
        }

        private static void DumpHardwareTemps(IHardware hw, int depth)
        {
            var indent = new string(' ', depth * 4);

            int sensorCount = hw.Sensors.Length;
            int subCount = hw.SubHardware.Length;
            Console.WriteLine("  " + indent + "[" + hw.HardwareType + "] " + hw.Name + "  (sensors=" + sensorCount + ", sub=" + subCount + ")");

            foreach (var sensor in hw.Sensors)
            {
                if (sensor.SensorType == SensorType.Temperature)
                    Console.WriteLine("  " + indent + "  -> Temp: \"" + sensor.Name + "\" = " + sensor.Value);
            }

            if (hw.HardwareType == HardwareType.Cpu)
            {
                foreach (var sensor in hw.Sensors)
                {
                    if (sensor.SensorType == SensorType.Load)
                        Console.WriteLine("  " + indent + "  -> Load: \"" + sensor.Name + "\" = " + sensor.Value);
                }
            }

            foreach (var sub in hw.SubHardware)
                DumpHardwareTemps(sub, depth + 1);

            if (hw.HardwareType == HardwareType.Motherboard && subCount == 0)
                Console.WriteLine("  " + indent + "  !! WARNING: No SubHardware (SuperIO/EC chip not detected)");
        }

        // ========================================================================
        //  Recursive Sensor Helper
        // ========================================================================

        private static IEnumerable<ISensor> GetAllSensorsRecursive(IHardware hardware)
        {
            foreach (var sensor in hardware.Sensors)
                yield return sensor;

            foreach (var sub in hardware.SubHardware)
            {
                foreach (var sensor in GetAllSensorsRecursive(sub))
                    yield return sensor;
            }
        }

        // ========================================================================
        //  Network
        // ========================================================================

        private void InitNetwork()
        {
            try
            {
                var instanceName = GetBestNetworkInstanceName();
                if (instanceName == null)
                {
                    Console.WriteLine("  [Network]  No active adapter found");
                    return;
                }

                _netBytesRecv = new PerformanceCounter("Network Interface", "Bytes Received/sec", instanceName);
                _netBytesSent = new PerformanceCounter("Network Interface", "Bytes Sent/sec", instanceName);
                _netBytesRecv.NextValue();
                _netBytesSent.NextValue();
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [Network]  FAIL - " + ex.Message);
            }
        }

        private static string GetBestNetworkInstanceName()
        {
            var interfaces = NetworkInterface.GetAllNetworkInterfaces()
                .Where(n => n.OperationalStatus == OperationalStatus.Up
                         && n.NetworkInterfaceType != NetworkInterfaceType.Loopback
                         && n.GetIPProperties().UnicastAddresses
                             .Any(a => a.Address.AddressFamily == AddressFamily.InterNetwork))
                .ToList();

            if (interfaces.Count == 0) return null;

            var best = interfaces.FirstOrDefault(n => n.NetworkInterfaceType == NetworkInterfaceType.Ethernet)
                    ?? interfaces.FirstOrDefault(n => n.NetworkInterfaceType == NetworkInterfaceType.Wireless80211)
                    ?? interfaces[0];

            Console.WriteLine("  [Network]  Selected: " + best.Description + " (" + best.NetworkInterfaceType + ")");

            return best.Description
                .Replace('(', '[')
                .Replace(')', ']')
                .Replace('/', '_');
        }

        // ========================================================================
        //  GetStats (main collection loop)
        // ========================================================================

        public SystemStats GetStats()
        {
            var stats = new SystemStats();

            if (IsLiteMode)
            {
                CollectCpuLite(stats);
                CollectRamLite(stats);
                CollectGpuLite(stats);
            }
            else
            {
                if (_computer != null)
                    _computer.Accept(_visitor);

                CollectCpu(stats);
                CollectRam(stats);
                CollectGpu(stats);
            }

            CollectNetwork(stats);

            return stats;
        }

        // ========================================================================
        //  CPU (Full Mode)
        // ========================================================================

        private void CollectCpu(SystemStats s)
        {
            if (_computer == null) return;

            try
            {
                foreach (var hw in _computer.Hardware)
                {
                    if (hw.HardwareType != HardwareType.Cpu) continue;

                    var allSensors = GetAllSensorsRecursive(hw).ToList();

                    // --- Load ---
                    foreach (var sensor in allSensors)
                    {
                        if (sensor.SensorType == SensorType.Load && sensor.Name == "CPU Total")
                        {
                            if (sensor.Value.HasValue)
                                s.CpuLoad = sensor.Value.Value;
                            break;
                        }
                    }

                    // --- Temp ---
                    float rawTemp = FindBestTempFromSensors(allSensors,
                        "Package", "Core Max", "Core Average", "Tctl");

                    if (rawTemp <= 0f)
                        rawTemp = FindMotherboardCpuTemp();

                    if (rawTemp > 0f)
                        s.CpuTemp = rawTemp;

                    break;
                }
            }
            catch { /* sensor read failed */ }
        }

        // ========================================================================
        //  CPU (Lite Mode - WMI/PerformanceCounter)
        // ========================================================================

        private void CollectCpuLite(SystemStats s)
        {
            // CPU Load via PerformanceCounter
            try
            {
                if (_cpuCounter != null)
                {
                    s.CpuLoad = _cpuCounter.NextValue();
                }
            }
            catch { }

            // CPU Temp - not available in Lite Mode
            s.CpuTemp = -1f;
        }

        private static float FindBestTempFromSensors(List<ISensor> sensors, params string[] priorities)
        {
            var tempSensors = sensors
                .Where(s => s.SensorType == SensorType.Temperature)
                .ToList();

            if (tempSensors.Count == 0) return 0f;

            foreach (var keyword in priorities)
            {
                var match = tempSensors.FirstOrDefault(
                    s => s.Name.IndexOf(keyword, StringComparison.OrdinalIgnoreCase) >= 0);
                if (match != null)
                    return match.Value ?? 0f;
            }

            foreach (var sensor in tempSensors)
            {
                float val = sensor.Value ?? 0f;
                if (val > 0f) return val;
            }

            return tempSensors[0].Value ?? 0f;
        }

        private float FindMotherboardCpuTemp()
        {
            if (_computer == null) return 0f;

            foreach (var hw in _computer.Hardware)
            {
                if (hw.HardwareType != HardwareType.Motherboard) continue;

                var allSensors = GetAllSensorsRecursive(hw).ToList();
                float val = FindBestTempFromSensors(allSensors, "CPU", "Socket");
                if (val > 0f) return val;
            }
            return 0f;
        }

        // ========================================================================
        //  RAM (Full Mode)
        // ========================================================================

        private void CollectRam(SystemStats s)
        {
            if (_computer == null) return;

            try
            {
                foreach (var hw in _computer.Hardware)
                {
                    if (hw.HardwareType != HardwareType.Memory) continue;

                    float used = -1f, available = -1f;
                    foreach (var sensor in hw.Sensors)
                    {
                        if (sensor.SensorType == SensorType.Data && sensor.Name == "Memory Used")
                            used = sensor.Value ?? -1f;
                        if (sensor.SensorType == SensorType.Data && sensor.Name == "Memory Available")
                            available = sensor.Value ?? -1f;
                    }

                    if (used > 0f && available >= 0f)
                    {
                        s.RamUsedGb = used;
                        s.RamTotalGb = used + available;
                    }
                    break;
                }
            }
            catch { /* sensor read failed */ }
        }

        // ========================================================================
        //  RAM (Lite Mode - WMI)
        // ========================================================================

        private void CollectRamLite(SystemStats s)
        {
            // Try LibreHardwareMonitor first (sometimes works without Ring0)
            if (_computer != null)
            {
                try
                {
                    _computer.Accept(_visitor);
                    CollectRam(s);
                    if (s.RamUsedGb > 0f) return;
                }
                catch { }
            }

            // Fallback: WMI
            try
            {
                using (var searcher = new ManagementObjectSearcher("SELECT TotalVisibleMemorySize, FreePhysicalMemory FROM Win32_OperatingSystem"))
                {
                    foreach (ManagementObject obj in searcher.Get())
                    {
                        float totalKb = Convert.ToSingle(obj["TotalVisibleMemorySize"]);
                        float freeKb = Convert.ToSingle(obj["FreePhysicalMemory"]);
                        s.RamTotalGb = totalKb / (1024f * 1024f);
                        s.RamUsedGb = (totalKb - freeKb) / (1024f * 1024f);
                        break;
                    }
                }
            }
            catch { }
        }

        // ========================================================================
        //  GPU (Full Mode)
        // ========================================================================

        private void CollectGpu(SystemStats s)
        {
            if (_nvApiAvailable && _gpu != null)
            {
                try
                {
                    CollectGpuNvApi(s);
                    return;
                }
                catch { /* NvAPI read failed, fall through to LHM */ }
            }

            CollectGpuLhm(s);
        }

        private void CollectGpuNvApi(SystemStats s)
        {
            var gpuUsage = _gpu.UsageInformation.GPU;
            s.GpuLoad = gpuUsage.Percentage;

            var thermals = _gpu.ThermalInformation.ThermalSensors;
            foreach (var t in thermals)
            {
                s.GpuTemp = t.CurrentTemperature;
                break;
            }

            var mem = _gpu.MemoryInformation;
            float totalKb = mem.DedicatedVideoMemoryInkB;
            float currentAvailKb = mem.CurrentAvailableDedicatedVideoMemoryInkB;
            s.GpuVramTotal = totalKb / (1024f * 1024f);
            s.GpuVramUsed = (totalKb - currentAvailKb) / (1024f * 1024f);
        }

        private void CollectGpuLhm(SystemStats s)
        {
            if (_computer == null) return;

            try
            {
                foreach (var hw in _computer.Hardware)
                {
                    if (hw.HardwareType != HardwareType.GpuNvidia
                        && hw.HardwareType != HardwareType.GpuAmd
                        && hw.HardwareType != HardwareType.GpuIntel)
                        continue;

                    foreach (var sensor in GetAllSensorsRecursive(hw))
                    {
                        if (sensor.SensorType == SensorType.Load && sensor.Name == "GPU Core" && sensor.Value.HasValue)
                            s.GpuLoad = sensor.Value.Value;
                        if (sensor.SensorType == SensorType.Temperature && sensor.Name == "GPU Core" && sensor.Value.HasValue && sensor.Value.Value > 0f)
                            s.GpuTemp = sensor.Value.Value;
                        if (sensor.SensorType == SensorType.SmallData && sensor.Name == "GPU Memory Used" && sensor.Value.HasValue)
                            s.GpuVramUsed = sensor.Value.Value / 1024f;
                        if (sensor.SensorType == SensorType.SmallData && sensor.Name == "GPU Memory Total" && sensor.Value.HasValue)
                            s.GpuVramTotal = sensor.Value.Value / 1024f;
                    }
                    break;
                }
            }
            catch { /* sensor read failed */ }
        }

        // ========================================================================
        //  GPU (Lite Mode - NvAPI only, no Ring0 needed)
        // ========================================================================

        private void CollectGpuLite(SystemStats s)
        {
            // NvAPI works without admin for NVIDIA cards
            if (_nvApiAvailable && _gpu != null)
            {
                try
                {
                    CollectGpuNvApi(s);
                    return;
                }
                catch { }
            }

            // No GPU data available in Lite Mode without NvAPI
            s.GpuLoad = -1f;
            s.GpuTemp = -1f;
            s.GpuVramUsed = -1f;
            s.GpuVramTotal = -1f;
        }

        // ========================================================================
        //  Network
        // ========================================================================

        private void CollectNetwork(SystemStats s)
        {
            try
            {
                if (_netBytesRecv == null || _netBytesSent == null) return;

                s.NetDown = _netBytesRecv.NextValue() / (1024f * 1024f);
                s.NetUp = _netBytesSent.NextValue() / (1024f * 1024f);
            }
            catch { /* counter read failed */ }
        }

        // ========================================================================
        //  Dispose
        // ========================================================================

        public void Dispose()
        {
            SafeCloseComputer();
            if (_netBytesRecv != null) { try { _netBytesRecv.Dispose(); } catch { } }
            if (_netBytesSent != null) { try { _netBytesSent.Dispose(); } catch { } }
            if (_cpuCounter != null) { try { _cpuCounter.Dispose(); } catch { } }
        }

        // ========================================================================
        //  Admin Check
        // ========================================================================

        public static bool IsAdmin()
        {
            using (var identity = System.Security.Principal.WindowsIdentity.GetCurrent())
            {
                return new System.Security.Principal.WindowsPrincipal(identity)
                       .IsInRole(System.Security.Principal.WindowsBuiltInRole.Administrator);
            }
        }
    }
}
