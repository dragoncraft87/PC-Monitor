using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
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
        private readonly Computer _computer;
        private readonly UpdateVisitor _visitor = new UpdateVisitor();
        private readonly bool _nvApiAvailable;
        private PhysicalGPU _gpu;

        // Network perf counters
        private PerformanceCounter _netBytesRecv;
        private PerformanceCounter _netBytesSent;

        public HardwareCollector()
        {
            Console.WriteLine("  [System]   Process Architecture: " + (Environment.Is64BitProcess ? "x64" : "x86"));

            // --- NvAPI ---
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

            // --- LibreHardwareMonitor ---
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
            _computer.Open();
            Console.WriteLine("  [LHM]      Computer opened (Ring0 driver loaded)");

            // Wait for Ring0 driver to finish scanning buses (SMBus, LPC, etc.)
            Console.WriteLine("  [LHM]      Waiting 2s for bus scan...");
            Thread.Sleep(2000);

            // First update pass
            _computer.Accept(_visitor);

            // Second update pass — some sensors (SuperIO) need two reads to populate
            Thread.Sleep(500);
            _computer.Accept(_visitor);

            // Debug: dump ALL hardware nodes + temperature sensors recursively
            Console.WriteLine("  [LHM]      --- Full Hardware & Temperature Sensor Dump ---");
            foreach (var hw in _computer.Hardware)
            {
                DumpHardwareTemps(hw, 0);
            }
            Console.WriteLine("  [LHM]      --- End Sensor Dump ---");

            // --- Network PerformanceCounters ---
            InitNetwork();
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

            _computer.Accept(_visitor);

            CollectCpu(stats);
            CollectRam(stats);
            CollectGpu(stats);
            CollectNetwork(stats);

            return stats;
        }

        // ========================================================================
        //  CPU
        // ========================================================================

        private void CollectCpu(SystemStats s)
        {
            try
            {
                foreach (var hw in _computer.Hardware)
                {
                    if (hw.HardwareType != HardwareType.Cpu) continue;

                    var allSensors = GetAllSensorsRecursive(hw).ToList();

                    // --- Load: 0 is valid (idle), null means sensor missing ---
                    foreach (var sensor in allSensors)
                    {
                        if (sensor.SensorType == SensorType.Load && sensor.Name == "CPU Total")
                        {
                            if (sensor.Value.HasValue)
                                s.CpuLoad = sensor.Value.Value;
                            // else stays -1
                            break;
                        }
                    }

                    // --- Temp: 0 means sensor returned nothing useful -> keep -1 ---
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
        //  RAM
        // ========================================================================

        private void CollectRam(SystemStats s)
        {
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
        //  GPU
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
            _computer.Close();
            if (_netBytesRecv != null) _netBytesRecv.Dispose();
            if (_netBytesSent != null) _netBytesSent.Dispose();
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
