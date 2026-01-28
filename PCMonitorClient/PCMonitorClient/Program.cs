using PCMonitorClient;

Console.WriteLine("Desert-Spec Monitor initializing...");

using var collector = new HardwareCollector();

Console.Clear();
Console.CursorVisible = false;

while (true)
{
    var s = collector.GetStats();

    Console.SetCursorPosition(0, 0);
    Console.WriteLine("╔══════════════════════════════════════╗");
    Console.WriteLine("║     Desert-Spec PC Monitor           ║");
    Console.WriteLine("╠══════════════════════════════════════╣");
    Console.WriteLine($"║  CPU Load:  {s.CpuLoad,6:F1} %                ║");
    Console.WriteLine($"║  CPU Temp:  {s.CpuTemp,6:F1} °C               ║");
    Console.WriteLine("╠══════════════════════════════════════╣");
    Console.WriteLine($"║  GPU Load:  {s.GpuLoad,6:F1} %                ║");
    Console.WriteLine($"║  GPU Temp:  {s.GpuTemp,6:F1} °C               ║");
    Console.WriteLine($"║  VRAM:      {s.GpuVramUsed,5:F1} / {s.GpuVramTotal,5:F1} GB      ║");
    Console.WriteLine("╠══════════════════════════════════════╣");
    Console.WriteLine($"║  RAM:       {s.RamUsedGb,5:F1} / {s.RamTotalGb,5:F1} GB      ║");
    Console.WriteLine("╠══════════════════════════════════════╣");
    Console.WriteLine($"║  Net Down:  {s.NetDown,6:F2} MB/s             ║");
    Console.WriteLine($"║  Net Up:    {s.NetUp,6:F2} MB/s             ║");
    Console.WriteLine("╚══════════════════════════════════════╝");

    Thread.Sleep(1000);
}
