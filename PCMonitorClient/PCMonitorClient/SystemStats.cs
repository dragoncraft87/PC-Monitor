using System;

namespace PCMonitorClient
{
    public sealed class SystemStats
    {
        // CPU (-1 = sensor error)
        public float CpuLoad { get; set; } = -1f;
        public float CpuTemp { get; set; } = -1f;

        // GPU (-1 = sensor error)
        public float GpuLoad { get; set; } = -1f;
        public float GpuTemp { get; set; } = -1f;
        public float GpuVramUsed { get; set; } = -1f;
        public float GpuVramTotal { get; set; } = -1f;

        // RAM (-1 = sensor error)
        public float RamUsedGb { get; set; } = -1f;
        public float RamTotalGb { get; set; } = -1f;

        // Network (MB/s) - 0 is valid (idle), -1 = error
        public float NetDown { get; set; } = 0f;
        public float NetUp { get; set; } = 0f;
    }
}
