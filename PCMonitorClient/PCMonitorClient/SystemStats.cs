using System;

namespace PCMonitorClient
{
    public sealed class SystemStats
    {
        // CPU
        public float CpuLoad { get; set; }
        public float CpuTemp { get; set; }

        // GPU
        public float GpuLoad { get; set; }
        public float GpuTemp { get; set; }
        public float GpuVramUsed { get; set; }
        public float GpuVramTotal { get; set; }

        // RAM
        public float RamUsedGb { get; set; }
        public float RamTotalGb { get; set; }

        // Network (MB/s)
        public float NetDown { get; set; }
        public float NetUp { get; set; }
    }
}
