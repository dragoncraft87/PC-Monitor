using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Management;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PCMonitorClient
{
    internal static class Program
    {
        private static string _logFile;

        [STAThread]
        static void Main(string[] args)
        {
            _logFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "crash_log.txt");

            AppDomain.CurrentDomain.UnhandledException += (s, e) =>
            {
                LogCrash("UnhandledException", e.ExceptionObject as Exception);
            };

            Application.ThreadException += (s, e) =>
            {
                LogCrash("ThreadException", e.Exception);
            };

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            // DLL Check
            string basePath = AppDomain.CurrentDomain.BaseDirectory;
            if (!File.Exists(Path.Combine(basePath, "LibreHardwareMonitorLib.dll")))
            {
                MessageBox.Show("LibreHardwareMonitorLib.dll missing!", "Scarab Monitor", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            try
            {
                Application.Run(new TrayContext(args));
            }
            catch (Exception ex)
            {
                LogCrash("Main", ex);
                MessageBox.Show("Fatal error: " + ex.Message, "Scarab Monitor", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        public static void LogCrash(string source, Exception ex)
        {
            try
            {
                string msg = string.Format("[{0}] {1}\r\n{2}\r\n\r\n",
                    DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"),
                    source,
                    ex != null ? ex.ToString() : "Unknown error");
                File.AppendAllText(_logFile, msg);
            }
            catch { }
        }
    }

    internal class TrayContext : ApplicationContext
    {
        private const string HANDSHAKE_QUERY = "WHO_ARE_YOU?\n";
        private const string HANDSHAKE_RESPONSE = "SCARAB_CLIENT_OK";

        // Identity Sync Protocol
        private const string NAME_CMD_CPU = "NAME_CPU=";
        private const string NAME_CMD_GPU = "NAME_GPU=";
        private const string NAME_CMD_HASH = "NAME_HASH=";

        private static readonly string[] SKIP_PORT_KEYWORDS = { "JTAG", "Debug", "Debugger", "JLink", "ST-Link" };
        private static readonly string[] PREFER_PORT_KEYWORDS = { "USB Serial", "USB-SERIAL", "CP210", "CH340", "CH341", "FTDI", "Silicon Labs" };

        private readonly NotifyIcon _trayIcon;
        private readonly StatusForm _statusForm;
        private readonly SettingsForm _settingsForm;
        private readonly ContextMenuStrip _contextMenu;
        private readonly string[] _args;

        // === ICON SYSTEM ===
        private readonly Icon _baseIcon;      // Original icon from file
        private readonly Icon _iconBase;      // Base icon without overlay (for blinking)
        private readonly Icon _iconRed;       // Disconnected / Error
        private readonly Icon _iconYellow;    // Lite Mode + Connected
        private readonly Icon _iconGreen;     // Full Mode + Connected

        // === HEARTBEAT ANIMATION ===
        private readonly System.Windows.Forms.Timer _blinkTimer;
        private bool _blinkState = false;
        private volatile bool _isFirstConnect = true;  // True until first successful connection

        // === STATE ===
        private HardwareCollector _collector;
        private CancellationTokenSource _cts;
        private Task _backgroundTask;
        private SerialPort _activePort;
        private readonly object _portLock = new object();

        private volatile bool _isShuttingDown = false;
        private volatile bool _isConnected = false;
        private volatile bool _isLiteMode = false;
        private volatile bool _isUploadMode = false;  // Pauses data loop during image upload

        public TrayContext(string[] args)
        {
            _args = args;

            // ============================================================
            // 1. ICON LOADING & GENERATION
            // ============================================================
            _baseIcon = LoadBaseIcon();
            _iconBase = CloneIcon(_baseIcon);
            _iconRed = GenerateIconWithOverlay(_baseIcon, Color.Red);
            _iconYellow = GenerateIconWithOverlay(_baseIcon, Color.Orange);
            _iconGreen = GenerateIconWithOverlay(_baseIcon, Color.LimeGreen);

            // ============================================================
            // 2. STATUS FORM (UI Thread)
            // ============================================================
            _statusForm = new StatusForm();

            // ============================================================
            // 2b. SETTINGS FORM (UI Thread)
            // ============================================================
            _settingsForm = new SettingsForm
            {
                SendCommand = SendCommandToEsp,
                IsConnected = () => _isConnected,
                GetSerialPort = () => _activePort,
                SetUploadMode = (mode) => _isUploadMode = mode
            };

            // ============================================================
            // 3. CONTEXT MENU
            // ============================================================
            _contextMenu = new ContextMenuStrip();
            _contextMenu.Items.Add("Show Status", null, (s, e) => ShowStatus());
            _contextMenu.Items.Add("Settings...", null, (s, e) => ShowSettings());
            _contextMenu.Items.Add(new ToolStripSeparator());

            var adminItem = new ToolStripMenuItem("Restart as Admin", null, (s, e) => RestartAsAdmin());
            adminItem.Enabled = !HardwareCollector.IsAdmin();
            _contextMenu.Items.Add(adminItem);

            _contextMenu.Items.Add(new ToolStripSeparator());
            _contextMenu.Items.Add("Exit", null, (s, e) => Exit());

            // ============================================================
            // 4. TRAY ICON (Start with red, will animate)
            // ============================================================
            _trayIcon = new NotifyIcon
            {
                Icon = _iconRed,
                ContextMenuStrip = _contextMenu,
                Visible = true,
                Text = "Scarab Monitor: Initializing..."
            };
            _trayIcon.DoubleClick += (s, e) => ShowStatus();

            // ============================================================
            // 5. HEARTBEAT TIMER (500ms blink during startup)
            // ============================================================
            _blinkTimer = new System.Windows.Forms.Timer();
            _blinkTimer.Interval = 500;
            _blinkTimer.Tick += BlinkTimer_Tick;
            _blinkTimer.Start();

            // ============================================================
            // 6. START BACKGROUND LOOP
            // ============================================================
            _cts = new CancellationTokenSource();
            _backgroundTask = Task.Run(() => MonitorLoop(_cts.Token));
        }

        // ====================================================================
        //  ICON LOADING
        // ====================================================================

        /// <summary>
        /// Loads icon.ico from app directory, falls back to SystemIcons.Application
        /// </summary>
        private Icon LoadBaseIcon()
        {
            string iconPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "icon.ico");
            if (File.Exists(iconPath))
            {
                try
                {
                    return new Icon(iconPath);
                }
                catch (Exception ex)
                {
                    Program.LogCrash("LoadBaseIcon", ex);
                }
            }
            return SystemIcons.Application;
        }

        /// <summary>
        /// Creates a copy of the icon (for base state without overlay)
        /// </summary>
        private Icon CloneIcon(Icon source)
        {
            using (Bitmap bmp = new Bitmap(16, 16))
            {
                using (Graphics g = Graphics.FromImage(bmp))
                {
                    g.Clear(Color.Transparent);
                    g.DrawIcon(source, new Rectangle(0, 0, 16, 16));
                }
                return Icon.FromHandle(bmp.GetHicon());
            }
        }

        // ====================================================================
        //  ICON OVERLAY GENERATION
        // ====================================================================

        /// <summary>
        /// Generates an icon with a colored status dot overlay in the bottom-right corner.
        /// Dot size is ~35% of icon, with black outline for contrast.
        /// </summary>
        private Icon GenerateIconWithOverlay(Icon baseIcon, Color dotColor)
        {
            using (Bitmap bmp = new Bitmap(16, 16))
            {
                using (Graphics g = Graphics.FromImage(bmp))
                {
                    g.Clear(Color.Transparent);
                    g.SmoothingMode = SmoothingMode.AntiAlias;

                    // Draw base icon
                    g.DrawIcon(baseIcon, new Rectangle(0, 0, 16, 16));

                    // Status dot: ~35% of 16px = 5-6px
                    int dotSize = 6;
                    int dotX = 16 - dotSize - 1;  // 1px from right edge
                    int dotY = 16 - dotSize - 1;  // 1px from bottom edge

                    // Black outline (1px thick) for contrast
                    using (Pen outlinePen = new Pen(Color.Black, 1.5f))
                    {
                        g.DrawEllipse(outlinePen, dotX, dotY, dotSize, dotSize);
                    }

                    // Fill with status color
                    using (Brush dotBrush = new SolidBrush(dotColor))
                    {
                        g.FillEllipse(dotBrush, dotX, dotY, dotSize, dotSize);
                    }

                    // Subtle highlight for 3D effect (top-left of dot)
                    using (Brush highlightBrush = new SolidBrush(Color.FromArgb(80, 255, 255, 255)))
                    {
                        g.FillEllipse(highlightBrush, dotX + 1, dotY + 1, dotSize / 2, dotSize / 2);
                    }
                }

                return Icon.FromHandle(bmp.GetHicon());
            }
        }

        // ====================================================================
        //  HEARTBEAT ANIMATION (Startup)
        // ====================================================================

        /// <summary>
        /// Blinks red/base icon until first connection is established
        /// </summary>
        private void BlinkTimer_Tick(object sender, EventArgs e)
        {
            if (_isShuttingDown) return;

            if (_isFirstConnect)
            {
                // Toggle between red dot and base icon (no dot)
                _blinkState = !_blinkState;
                try
                {
                    _trayIcon.Icon = _blinkState ? _iconRed : _iconBase;
                }
                catch { }
            }
            else
            {
                // First connection established - stop blinking
                _blinkTimer.Stop();
            }
        }

        /// <summary>
        /// Call this when first connection is established
        /// </summary>
        private void OnFirstConnect()
        {
            _isFirstConnect = false;
            try
            {
                _blinkTimer.Stop();
            }
            catch { }
        }

        // ====================================================================
        //  TRAY ICON UPDATE (Thread-Safe)
        // ====================================================================

        /// <summary>
        /// Updates the tray icon based on connection state and mode.
        /// Logic: Red=Disconnected, Yellow=Lite+Connected, Green=Full+Connected
        /// </summary>
        private void UpdateTrayIcon()
        {
            if (_isShuttingDown) return;

            try
            {
                Icon newIcon;
                string tooltip;

                // Status logic: Red if disconnected, Yellow if Lite, Green if Full
                if (!_isConnected)
                {
                    newIcon = _iconRed;
                    tooltip = "Scarab Monitor: Disconnected";
                }
                else if (_isLiteMode)
                {
                    newIcon = _iconYellow;
                    tooltip = "Scarab Monitor: Connected (LITE)";
                }
                else
                {
                    newIcon = _iconGreen;
                    tooltip = "Scarab Monitor: Connected (FULL)";
                }

                // Thread-safe UI update via BeginInvoke
                if (_statusForm.IsHandleCreated && !_statusForm.IsDisposed)
                {
                    _statusForm.BeginInvoke((MethodInvoker)delegate
                    {
                        if (!_isShuttingDown)
                        {
                            _trayIcon.Icon = newIcon;
                            _trayIcon.Text = tooltip;
                        }
                    });
                }
                else
                {
                    // Fallback: direct update (risky but necessary during init)
                    _trayIcon.Icon = newIcon;
                    _trayIcon.Text = tooltip;
                }
            }
            catch { }
        }

        // ====================================================================
        //  UI HELPERS
        // ====================================================================

        private void ShowStatus()
        {
            // Use Show(), NOT ShowDialog() - critical for non-blocking!
            if (_statusForm.Visible)
            {
                _statusForm.Activate();
            }
            else
            {
                _statusForm.Show();
            }
        }

        private void ShowSettings()
        {
            if (_settingsForm.Visible)
            {
                _settingsForm.Activate();
            }
            else
            {
                _settingsForm.Show();
            }
        }

        /// <summary>
        /// Sends a command string to the ESP32 via serial port.
        /// Thread-safe, can be called from UI.
        /// </summary>
        private void SendCommandToEsp(string command)
        {
            if (string.IsNullOrEmpty(command)) return;

            lock (_portLock)
            {
                if (_activePort == null || !_activePort.IsOpen)
                {
                    _statusForm.AppendLog("[Cmd] Error: Not connected");
                    return;
                }

                try
                {
                    string line = command.EndsWith("\n") ? command : command + "\n";
                    _activePort.Write(line);
                    _activePort.BaseStream.Flush();
                    _statusForm.AppendLog("[Cmd] TX: " + command);
                }
                catch (Exception ex)
                {
                    _statusForm.AppendLog("[Cmd] Error: " + ex.Message);
                }
            }
        }

        private void RestartAsAdmin()
        {
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = Application.ExecutablePath,
                    UseShellExecute = true,
                    Verb = "runas"
                };
                Process.Start(psi);
                Exit();
            }
            catch (System.ComponentModel.Win32Exception)
            {
                // User cancelled UAC
            }
            catch (Exception ex)
            {
                MessageBox.Show("Could not restart: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void Exit()
        {
            if (_isShuttingDown) return;
            _isShuttingDown = true;

            // 1. Stop blink timer
            try { _blinkTimer.Stop(); _blinkTimer.Dispose(); } catch { }

            // 2. Cancel background task
            try { _cts.Cancel(); } catch { }

            // 3. Close serial port to unblock reads
            lock (_portLock)
            {
                if (_activePort != null)
                {
                    try { _activePort.Close(); _activePort.Dispose(); } catch { }
                    _activePort = null;
                }
            }

            // 4. Wait for background task (max 1.5s)
            try { _backgroundTask?.Wait(1500); } catch { }

            // 5. Dispose hardware collector
            try { _collector?.Dispose(); } catch { }

            // 6. Hide tray icon
            try { _trayIcon.Visible = false; _trayIcon.Dispose(); } catch { }

            // 7. Exit application
            Application.Exit();
        }

        // ====================================================================
        //  BACKGROUND MONITOR LOOP
        // ====================================================================

        private void MonitorLoop(CancellationToken ct)
        {
            try
            {
                // Initialize hardware collector
                _statusForm.AppendLog("[Init] Starting hardware collector...");
                _statusForm.AppendLog("[Init] Admin: " + (HardwareCollector.IsAdmin() ? "YES" : "NO"));

                _collector = new HardwareCollector();
                _isLiteMode = _collector.IsLiteMode;

                _statusForm.AppendLog("[Init] " + _collector.InitStatus);
                _statusForm.UpdateConnectionStatus("Initialized", false, _isLiteMode);

                string cliPort = (_args.Length > 0) ? _args[0] : null;

                while (!ct.IsCancellationRequested)
                {
                    // --- CONNECT ---
                    _isConnected = false;
                    UpdateTrayIcon();
                    _statusForm.UpdateConnectionStatus("Searching...", false, _isLiteMode);

                    string portName = cliPort ?? FindEsp32ByHandshake(ct);
                    if (string.IsNullOrEmpty(portName))
                    {
                        // Wait and retry (in chunks for fast cancel)
                        for (int i = 0; i < 30 && !ct.IsCancellationRequested; i++)
                            Thread.Sleep(100);
                        continue;
                    }

                    _statusForm.AppendLog("[Serial] Connecting to " + portName + "...");

                    try
                    {
                        using (var port = new SerialPort(portName, 115200))
                        {
                            port.DtrEnable = true;
                            port.ReadTimeout = 500;
                            port.WriteTimeout = 1000;

                            lock (_portLock)
                            {
                                if (ct.IsCancellationRequested) break;
                                port.Open();
                                _activePort = port;
                            }

                            // Wait for ESP32 reset (in chunks)
                            for (int i = 0; i < 20 && !ct.IsCancellationRequested; i++)
                                Thread.Sleep(100);

                            if (ct.IsCancellationRequested) break;

                            // Verify handshake and get ESP hash
                            string espHash = VerifyHandshakeAndGetHash(port);
                            if (espHash == null)
                            {
                                _statusForm.AppendLog("[Serial] Handshake FAILED on " + portName);
                                lock (_portLock) { _activePort = null; }
                                for (int i = 0; i < 20 && !ct.IsCancellationRequested; i++)
                                    Thread.Sleep(100);
                                continue;
                            }

                            // === FIRST CONNECTION ESTABLISHED ===
                            _statusForm.AppendLog("[Serial] Connected to " + portName);

                            // Sync hardware identity if needed
                            SyncIdentityIfNeeded(port, espHash);
                            _isConnected = true;

                            // Stop heartbeat animation on first connect
                            if (_isFirstConnect)
                            {
                                OnFirstConnect();
                            }

                            UpdateTrayIcon();
                            _statusForm.UpdateConnectionStatus("Connected: " + portName, true, _isLiteMode);

                            // --- DATA LOOP ---
                            RunDataLoop(port, ct);

                            lock (_portLock) { _activePort = null; }
                        }
                    }
                    catch (OperationCanceledException)
                    {
                        break;
                    }
                    catch (Exception ex)
                    {
                        Program.LogCrash("MonitorLoop", ex);
                        _statusForm.AppendLog("[Error] " + ex.Message);
                        lock (_portLock) { _activePort = null; }

                        if (!ct.IsCancellationRequested)
                        {
                            for (int i = 0; i < 20 && !ct.IsCancellationRequested; i++)
                                Thread.Sleep(100);
                        }
                    }

                    _isConnected = false;
                    UpdateTrayIcon();
                    _statusForm.UpdateConnectionStatus("Disconnected", false, _isLiteMode);

                    // If auto-detected, re-scan
                    if (_args.Length == 0) cliPort = null;
                }
            }
            catch (Exception ex)
            {
                Program.LogCrash("MonitorLoop_Outer", ex);
                _statusForm.AppendLog("[FATAL] " + ex.Message);
            }
        }

        private void RunDataLoop(SerialPort port, CancellationToken ct)
        {
            // Detect network info once
            string netType = "LAN", netSpeed = "1000 Mbps";
            try
            {
                var iface = NetworkInterface.GetAllNetworkInterfaces()
                    .FirstOrDefault(n => n.OperationalStatus == OperationalStatus.Up
                        && n.NetworkInterfaceType != NetworkInterfaceType.Loopback
                        && n.GetIPProperties().UnicastAddresses
                            .Any(a => a.Address.AddressFamily == AddressFamily.InterNetwork));

                if (iface != null)
                {
                    netType = (iface.NetworkInterfaceType == NetworkInterfaceType.Wireless80211) ? "WLAN" : "LAN";
                    netSpeed = (iface.Speed / 1000000) + " Mbps";
                }
            }
            catch { }

            while (!ct.IsCancellationRequested && port.IsOpen)
            {
                try
                {
                    // Pause data transmission during image upload
                    if (_isUploadMode)
                    {
                        Thread.Sleep(100);
                        continue;
                    }

                    var s = _collector.GetStats();

                    // Format data string (matches ESP32 parser)
                    string data = string.Format(CultureInfo.InvariantCulture,
                        "CPU:{0},CPUT:{1:0.0},GPU:{2},GPUT:{3:0.0},VRAM:{4:0.0}/{5:0.0},RAM:{6:0.0}/{7:0.0},NET:{8},SPEED:{9},DOWN:{10:0.0},UP:{11:0.0}\n",
                        (int)s.CpuLoad, s.CpuTemp,
                        (int)s.GpuLoad, s.GpuTemp,
                        s.GpuVramUsed, s.GpuVramTotal,
                        s.RamUsedGb, s.RamTotalGb,
                        netType, netSpeed,
                        s.NetDown, s.NetUp);

                    port.Write(data);
                    port.BaseStream.Flush();

                    // Update status form (only if visible, handled internally)
                    _statusForm.UpdateData("TX: " + data.Trim());

                    // Sleep in chunks for fast cancel
                    for (int i = 0; i < 10 && !ct.IsCancellationRequested; i++)
                        Thread.Sleep(100);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Program.LogCrash("DataLoop", ex);
                    _statusForm.AppendLog("[TX Error] " + ex.Message);
                    break;
                }
            }
        }

        // ====================================================================
        //  HANDSHAKE & PORT DISCOVERY
        // ====================================================================

        /// <summary>
        /// Verifies handshake and returns ESP's identity hash (or null on failure).
        /// Response format: SCARAB_CLIENT_OK|H:XXXXXXXX
        /// </summary>
        private string VerifyHandshakeAndGetHash(SerialPort port)
        {
            try
            {
                port.DiscardInBuffer();
                port.Write(HANDSHAKE_QUERY);
                port.BaseStream.Flush();

                int origTimeout = port.ReadTimeout;
                port.ReadTimeout = 200;

                try
                {
                    string response = port.ReadLine();
                    if (response != null && response.Contains(HANDSHAKE_RESPONSE))
                    {
                        // Parse hash: SCARAB_CLIENT_OK|H:XXXXXXXX
                        int hashPos = response.IndexOf("|H:");
                        if (hashPos >= 0 && response.Length >= hashPos + 11)
                        {
                            return response.Substring(hashPos + 3, 8);
                        }
                        return "00000000"; // Old firmware without hash support
                    }
                }
                finally
                {
                    port.ReadTimeout = origTimeout;
                }
            }
            catch { }
            return null; // Handshake failed
        }

        private bool VerifyHandshake(SerialPort port)
        {
            return VerifyHandshakeAndGetHash(port) != null;
        }

        /// <summary>
        /// Syncs hardware identity if ESP hash differs from local.
        /// Sends NAME_CPU, NAME_GPU, NAME_HASH commands.
        /// </summary>
        private void SyncIdentityIfNeeded(SerialPort port, string espHash)
        {
            if (_collector == null) return;

            string localHash = _collector.IdentityHash;

            if (espHash == localHash)
            {
                _statusForm.AppendLog("[Sync] Hash match: " + localHash);
                return;
            }

            _statusForm.AppendLog("[Sync] Hash mismatch! ESP=" + espHash + " Local=" + localHash);
            _statusForm.AppendLog("[Sync] Sending hardware names...");

            try
            {
                // Send CPU name
                string cmdCpu = NAME_CMD_CPU + _collector.CpuName + "\n";
                port.Write(cmdCpu);
                port.BaseStream.Flush();
                Thread.Sleep(50);

                // Send GPU name
                string cmdGpu = NAME_CMD_GPU + _collector.GpuName + "\n";
                port.Write(cmdGpu);
                port.BaseStream.Flush();
                Thread.Sleep(50);

                // Send new hash (ESP will store it)
                string cmdHash = NAME_CMD_HASH + localHash + "\n";
                port.Write(cmdHash);
                port.BaseStream.Flush();
                Thread.Sleep(50);

                _statusForm.AppendLog("[Sync] Names sent: CPU=" + _collector.CpuName + ", GPU=" + _collector.GpuName);
            }
            catch (Exception ex)
            {
                _statusForm.AppendLog("[Sync] Error: " + ex.Message);
            }
        }

        private string FindEsp32ByHandshake(CancellationToken ct)
        {
            var ports = GetFilteredPorts();
            if (ports.Count == 0) return null;

            _statusForm.AppendLog("[Serial] Scanning " + ports.Count + " ports...");

            foreach (var portInfo in ports)
            {
                if (ct.IsCancellationRequested) return null;
                if (portInfo.ShouldSkip)
                {
                    _statusForm.AppendLog("  " + portInfo.Name + ": SKIP (JTAG)");
                    continue;
                }

                _statusForm.AppendLog("  " + portInfo.Name + ": Trying...");

                try
                {
                    using (var port = new SerialPort(portInfo.Name, 115200))
                    {
                        port.DtrEnable = true;
                        port.ReadTimeout = 200;
                        port.WriteTimeout = 500;
                        port.Open();

                        Thread.Sleep(1000); // Wait for device

                        if (VerifyHandshake(port))
                        {
                            _statusForm.AppendLog("  " + portInfo.Name + ": FOUND!");
                            port.Close();
                            return portInfo.Name;
                        }
                        port.Close();
                    }
                }
                catch (UnauthorizedAccessException)
                {
                    _statusForm.AppendLog("  " + portInfo.Name + ": In use");
                }
                catch { }
            }

            return null;
        }

        private class PortInfo
        {
            public string Name;
            public string Caption;
            public bool ShouldSkip;
            public bool IsPreferred;
        }

        private List<PortInfo> GetFilteredPorts()
        {
            var result = new List<PortInfo>();

            try
            {
                var systemPorts = SerialPort.GetPortNames().ToHashSet(StringComparer.OrdinalIgnoreCase);
                if (systemPorts.Count == 0) return result;

                // WMI for port details
                using (var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_PnPEntity WHERE Caption LIKE '%(COM%'"))
                {
                    foreach (ManagementObject obj in searcher.Get())
                    {
                        try
                        {
                            string caption = obj["Caption"]?.ToString() ?? "";
                            int start = caption.LastIndexOf("(COM");
                            int end = caption.LastIndexOf(")");

                            if (start >= 0 && end > start)
                            {
                                string portName = caption.Substring(start + 1, end - start - 1);
                                if (systemPorts.Contains(portName))
                                {
                                    result.Add(new PortInfo
                                    {
                                        Name = portName,
                                        Caption = caption,
                                        ShouldSkip = SKIP_PORT_KEYWORDS.Any(k => caption.IndexOf(k, StringComparison.OrdinalIgnoreCase) >= 0),
                                        IsPreferred = PREFER_PORT_KEYWORDS.Any(k => caption.IndexOf(k, StringComparison.OrdinalIgnoreCase) >= 0)
                                    });
                                    systemPorts.Remove(portName);
                                }
                            }
                        }
                        catch { }
                    }
                }

                // Add remaining (unknown) ports
                foreach (var port in systemPorts)
                {
                    result.Add(new PortInfo { Name = port, Caption = port, ShouldSkip = false, IsPreferred = false });
                }
            }
            catch
            {
                // Fallback
                foreach (var port in SerialPort.GetPortNames())
                {
                    result.Add(new PortInfo { Name = port, Caption = port, ShouldSkip = false, IsPreferred = false });
                }
            }

            // Sort: preferred first, then by name descending
            return result
                .Where(p => !p.ShouldSkip)
                .OrderByDescending(p => p.IsPreferred)
                .ThenByDescending(p => p.Name)
                .Concat(result.Where(p => p.ShouldSkip))
                .ToList();
        }
    }
}
