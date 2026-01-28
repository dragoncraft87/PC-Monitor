using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using LibreHardwareMonitor.Hardware;
using NvAPIWrapper;
using NvAPIWrapper.GPU;

namespace PCMonitorClient;

public sealed class UpdateVisitor : IVisitor
{
    public void VisitComputer(IComputer computer) => computer.Traverse(this);

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
    private readonly UpdateVisitor _visitor = new();
    private readonly bool _nvApiAvailable;
    private PhysicalGPU? _gpu;

    // Network perf counters
    private PerformanceCounter? _netBytesRecv;
    private PerformanceCounter? _netBytesSent;

    public HardwareCollector()
    {
        Console.WriteLine($"  [System]   Process Architecture: {(Environment.Is64BitProcess ? "x64" : "x86")}");

        // --- NvAPI ---
        try
        {
            NVIDIA.Initialize();
            var gpus = PhysicalGPU.GetPhysicalGPUs();
            if (gpus.Length > 0)
            {
                _gpu = gpus[0];
                _nvApiAvailable = true;
                Console.WriteLine($"  [NvAPI]    OK - {_gpu.FullName}");
            }
        }
        catch (Exception ex)
        {
            _nvApiAvailable = false;
            Console.WriteLine($"  [NvAPI]    FAIL - {ex.Message}");
        }

        // --- LibreHardwareMonitor ---
        _computer = new Computer
        {
            IsCpuEnabled = true,
            IsMemoryEnabled = true,
            IsMotherboardEnabled = true,
            IsControllerEnabled = true,
            IsGpuEnabled = !_nvApiAvailable, // false when NvAPI works, true as fallback
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
            DumpHardwareTemps(hw, depth: 0);
        }
        Console.WriteLine("  [LHM]      --- End Sensor Dump ---");

        // --- Network PerformanceCounters ---
        InitNetwork();
    }

    /// <summary>
    /// Recursively dump all temperature sensors for debug output.
    /// </summary>
    private static void DumpHardwareTemps(IHardware hw, int depth)
    {
        var indent = new string(' ', depth * 4);

        // Always show this hardware node (even if it has no sensors)
        int sensorCount = hw.Sensors.Length;
        int subCount = hw.SubHardware.Length;
        Console.WriteLine($"  {indent}[{hw.HardwareType}] {hw.Name}  (sensors={sensorCount}, sub={subCount})");

        // Show temperature sensors
        foreach (var sensor in hw.Sensors)
        {
            if (sensor.SensorType == SensorType.Temperature)
                Console.WriteLine($"  {indent}  -> Temp: \"{sensor.Name}\" = {sensor.Value}");
        }

        // Show load sensors for CPU nodes
        if (hw.HardwareType == HardwareType.Cpu)
        {
            foreach (var sensor in hw.Sensors)
            {
                if (sensor.SensorType == SensorType.Load)
                    Console.WriteLine($"  {indent}  -> Load: \"{sensor.Name}\" = {sensor.Value}");
            }
        }

        // Always recurse into sub-hardware (show nodes even if empty)
        foreach (var sub in hw.SubHardware)
            DumpHardwareTemps(sub, depth + 1);

        // Explicitly note if no sub-hardware was found (useful for Motherboard diagnosis)
        if (hw.HardwareType == HardwareType.Motherboard && subCount == 0)
            Console.WriteLine($"  {indent}  !! WARNING: No SubHardware (SuperIO/EC chip not detected)");
    }

    // ========================================================================
    //  Recursive Sensor Helper
    // ========================================================================

    /// <summary>
    /// Recursively collects ALL sensors from a hardware node and all its sub-hardware.
    /// This is critical for platforms like Skylake-X / X299 where temp sensors
    /// live deep inside SubHardware (e.g. SuperIO chips under Motherboard).
    /// </summary>
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
            Console.WriteLine($"  [Network]  FAIL - {ex.Message}");
        }
    }

    private static string? GetBestNetworkInstanceName()
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

        Console.WriteLine($"  [Network]  Selected: {best.Description} ({best.NetworkInterfaceType})");

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

        // Single visitor pass — updates ALL hardware + sub-hardware recursively
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

                // Deep-search ALL sensors (including sub-hardware)
                var allSensors = GetAllSensorsRecursive(hw).ToList();

                // --- Load ---
                foreach (var sensor in allSensors)
                {
                    if (sensor.SensorType == SensorType.Load && sensor.Name == "CPU Total")
                    {
                        s.CpuLoad = sensor.Value ?? 0f;
                        break;
                    }
                }

                // --- Temperature (prioritized deep search) ---
                s.CpuTemp = FindBestTempFromSensors(allSensors,
                    "Package", "Core Max", "Core Average", "Tctl");

                // Fallback: search Motherboard tree for CPU-related temp
                if (s.CpuTemp == 0f)
                    s.CpuTemp = FindMotherboardCpuTemp();

                break; // first CPU only
            }
        }
        catch { /* sensor read failed */ }
    }

    /// <summary>
    /// Given a flat list of sensors, find the best temperature sensor by name priority.
    /// Checks each priority keyword in order; falls back to first available temp.
    /// </summary>
    private static float FindBestTempFromSensors(List<ISensor> sensors, params string[] priorities)
    {
        var tempSensors = sensors
            .Where(s => s.SensorType == SensorType.Temperature)
            .ToList();

        if (tempSensors.Count == 0) return 0f;

        // Try each priority keyword in order
        foreach (var keyword in priorities)
        {
            var match = tempSensors.FirstOrDefault(
                s => s.Name.Contains(keyword, StringComparison.OrdinalIgnoreCase));
            if (match != null)
                return match.Value ?? 0f;
        }

        // Fallback: first temp sensor with a value > 0
        foreach (var sensor in tempSensors)
        {
            float val = sensor.Value ?? 0f;
            if (val > 0f) return val;
        }

        return tempSensors[0].Value ?? 0f;
    }

    /// <summary>
    /// Fallback: deep-search the Motherboard tree (SuperIO / EC chips)
    /// for temperature sensors named "CPU" or "Socket".
    /// </summary>
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

                float used = 0f, available = 0f;
                foreach (var sensor in hw.Sensors)
                {
                    if (sensor.SensorType == SensorType.Data && sensor.Name == "Memory Used")
                        used = sensor.Value ?? 0f;
                    if (sensor.SensorType == SensorType.Data && sensor.Name == "Memory Available")
                        available = sensor.Value ?? 0f;
                }

                s.RamUsedGb = used;
                s.RamTotalGb = used + available;
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
        var gpuUsage = _gpu!.UsageInformation.GPU;
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
                if (hw.HardwareType is not (HardwareType.GpuNvidia or HardwareType.GpuAmd or HardwareType.GpuIntel))
                    continue;

                foreach (var sensor in GetAllSensorsRecursive(hw))
                {
                    if (sensor.SensorType == SensorType.Load && sensor.Name == "GPU Core")
                        s.GpuLoad = sensor.Value ?? 0f;
                    if (sensor.SensorType == SensorType.Temperature && sensor.Name == "GPU Core")
                        s.GpuTemp = sensor.Value ?? 0f;
                    if (sensor.SensorType == SensorType.SmallData && sensor.Name == "GPU Memory Used")
                        s.GpuVramUsed = (sensor.Value ?? 0f) / 1024f;
                    if (sensor.SensorType == SensorType.SmallData && sensor.Name == "GPU Memory Total")
                        s.GpuVramTotal = (sensor.Value ?? 0f) / 1024f;
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
        _netBytesRecv?.Dispose();
        _netBytesSent?.Dispose();
    }
}
