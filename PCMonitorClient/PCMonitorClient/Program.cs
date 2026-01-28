using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
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
            // Setup crash log path
            _logFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "crash_log.txt");

            // Global exception handler
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

            // Check for required DLLs before starting
            if (!CheckRequiredFiles())
            {
                return;
            }

            try
            {
                Application.Run(new TrayContext(args));
            }
            catch (Exception ex)
            {
                LogCrash("Main", ex);
                MessageBox.Show("Fatal error: " + ex.Message + "\n\nSee crash_log.txt for details.",
                    "Scarab Monitor Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private static bool CheckRequiredFiles()
        {
            string basePath = AppDomain.CurrentDomain.BaseDirectory;
            string[] requiredDlls = { "LibreHardwareMonitorLib.dll", "HidSharp.dll" };

            foreach (var dll in requiredDlls)
            {
                string path = Path.Combine(basePath, dll);
                if (!File.Exists(path))
                {
                    // Also check in libs subfolder
                    string libsPath = Path.Combine(basePath, "libs", dll);
                    if (!File.Exists(libsPath))
                    {
                        MessageBox.Show("Required file missing: " + dll + "\n\nPlease ensure " + dll + " is in the application directory.",
                            "Scarab Monitor - Missing DLL", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }
            }

            return true;
        }

        public static void LogCrash(string source, Exception ex)
        {
            try
            {
                string msg = string.Format("[{0}] {1}\r\n{2}\r\n{3}\r\n\r\n",
                    DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"),
                    source,
                    ex != null ? ex.Message : "Unknown error",
                    ex != null ? ex.ToString() : "");
                File.AppendAllText(_logFile, msg);
            }
            catch { }
        }
    }

    internal class TrayContext : ApplicationContext
    {
        private const string HANDSHAKE_QUERY = "WHO_ARE_YOU?\n";
        private const string HANDSHAKE_RESPONSE = "SCARAB_CLIENT_OK";

        // Port names to SKIP (JTAG, debug interfaces, etc.)
        private static readonly string[] SKIP_PORT_KEYWORDS = {
            "JTAG", "Debug", "Debugger", "JLink", "ST-Link", "CMSIS"
        };

        // Port names to PREFER (common USB-Serial chips)
        private static readonly string[] PREFER_PORT_KEYWORDS = {
            "USB Serial", "USB-SERIAL", "CP210", "CH340", "CH341", "FTDI", "Prolific", "Silicon Labs"
        };

        private readonly NotifyIcon _trayIcon;
        private readonly StatusForm _statusForm;
        private readonly ContextMenuStrip _contextMenu;
        private readonly ToolStripMenuItem _menuShowStatus;
        private readonly ToolStripMenuItem _menuRestartAdmin;
        private readonly ToolStripMenuItem _menuExit;

        private readonly string[] _args;

        private volatile bool _isConnected = false;
        private volatile bool _isLiteMode = false;
        private volatile string _currentPort = "";
        private volatile bool _isShuttingDown = false;

        private CancellationTokenSource _cts;
        private Task _backgroundTask;
        private HardwareCollector _collector;
        private SerialPort _activeSerialPort;
        private readonly object _serialLock = new object();

        public TrayContext(string[] args)
        {
            _args = args;

            // Create status form
            _statusForm = new StatusForm();

            // Create context menu
            _contextMenu = new ContextMenuStrip();

            _menuShowStatus = new ToolStripMenuItem("Show Status", null, OnShowStatus);
            _menuRestartAdmin = new ToolStripMenuItem("Restart as Admin", null, OnRestartAdmin);
            _menuRestartAdmin.Enabled = !HardwareCollector.IsAdmin();
            _menuExit = new ToolStripMenuItem("Exit", null, OnExit);

            _contextMenu.Items.Add(_menuShowStatus);
            _contextMenu.Items.Add(_menuRestartAdmin);
            _contextMenu.Items.Add(new ToolStripSeparator());
            _contextMenu.Items.Add(_menuExit);

            // Create tray icon
            _trayIcon = new NotifyIcon
            {
                Icon = CreateStatusIcon(ConnectionState.Disconnected),
                Text = "Scarab Monitor: Starting...",
                ContextMenuStrip = _contextMenu,
                Visible = true
            };
            _trayIcon.DoubleClick += (s, e) => OnShowStatus(s, e);

            // Start background worker
            _cts = new CancellationTokenSource();
            _backgroundTask = Task.Run(() => BackgroundLoop(_cts.Token));
        }

        // ====================================================================
        //  Icon Helpers
        // ====================================================================

        private enum ConnectionState
        {
            Disconnected,       // Red
            ConnectedLite,      // Yellow (Lite Mode)
            ConnectedFull       // Green (Full Mode)
        }

        private Icon CreateStatusIcon(ConnectionState state)
        {
            // Create a new icon with a status dot overlay
            using (Bitmap bmp = new Bitmap(16, 16))
            {
                using (Graphics g = Graphics.FromImage(bmp))
                {
                    g.Clear(Color.Transparent);

                    // Draw base "S" letter for Scarab
                    using (Font f = new Font("Arial", 10f, FontStyle.Bold))
                    using (Brush textBrush = new SolidBrush(Color.White))
                    {
                        g.DrawString("S", f, textBrush, -1, 0);
                    }

                    // Draw status dot (bottom-right)
                    Color dotColor;
                    switch (state)
                    {
                        case ConnectionState.ConnectedFull:
                            dotColor = Color.LimeGreen;
                            break;
                        case ConnectionState.ConnectedLite:
                            dotColor = Color.Orange;
                            break;
                        default:
                            dotColor = Color.Red;
                            break;
                    }

                    using (Brush dotBrush = new SolidBrush(dotColor))
                    {
                        g.FillEllipse(dotBrush, 10, 10, 5, 5);
                    }
                }

                return Icon.FromHandle(bmp.GetHicon());
            }
        }

        private void SafeUpdateUI(Action action)
        {
            if (_isShuttingDown) return;

            try
            {
                if (_statusForm != null && !_statusForm.IsDisposed)
                {
                    if (_statusForm.InvokeRequired)
                        _statusForm.BeginInvoke(action);
                    else
                        action();
                }
            }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
            catch (Exception ex)
            {
                Program.LogCrash("SafeUpdateUI", ex);
            }
        }

        private void UpdateTrayStatus(bool connected, string status)
        {
            if (_isShuttingDown) return;

            _isConnected = connected;

            // Update icon (thread-safe)
            try
            {
                if (_trayIcon != null)
                {
                    ConnectionState state;
                    if (!connected)
                        state = ConnectionState.Disconnected;
                    else if (_isLiteMode)
                        state = ConnectionState.ConnectedLite;
                    else
                        state = ConnectionState.ConnectedFull;

                    _trayIcon.Icon = CreateStatusIcon(state);

                    string tooltip = "Scarab Monitor: " + status;
                    if (_isLiteMode && connected)
                        tooltip += " [LITE]";
                    // Tooltip max 63 chars
                    if (tooltip.Length > 63) tooltip = tooltip.Substring(0, 63);
                    _trayIcon.Text = tooltip;
                }
            }
            catch (Exception ex)
            {
                Program.LogCrash("UpdateTrayIcon", ex);
            }

            // Update status form (thread-safe)
            SafeUpdateUI(() => _statusForm.UpdateConnectionStatus(connected, status));
        }

        // ====================================================================
        //  Menu Handlers
        // ====================================================================

        private void OnShowStatus(object sender, EventArgs e)
        {
            _statusForm.Show();
            _statusForm.Activate();
            _statusForm.BringToFront();
        }

        private void OnRestartAdmin(object sender, EventArgs e)
        {
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = Application.ExecutablePath,
                    UseShellExecute = true,
                    Verb = "runas"
                };
                Process.Start(psi);

                // Graceful shutdown of current instance
                GracefulShutdown();
            }
            catch (System.ComponentModel.Win32Exception)
            {
                // User cancelled UAC prompt
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to restart as admin: " + ex.Message,
                    "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void OnExit(object sender, EventArgs e)
        {
            GracefulShutdown();
        }

        private void GracefulShutdown()
        {
            if (_isShuttingDown) return;
            _isShuttingDown = true;

            SafeUpdateUI(() => _statusForm.AppendLog("[Shutdown] Initiating graceful shutdown..."));

            // 1. Signal cancellation
            try { _cts.Cancel(); } catch { }

            // 2. Close serial port immediately to unblock any reads
            lock (_serialLock)
            {
                if (_activeSerialPort != null)
                {
                    try
                    {
                        _activeSerialPort.Close();
                        _activeSerialPort.Dispose();
                    }
                    catch { }
                    _activeSerialPort = null;
                }
            }

            // 3. Wait for background task to complete (max 2 seconds)
            if (_backgroundTask != null)
            {
                try
                {
                    _backgroundTask.Wait(2000);
                }
                catch { }
            }

            // 4. Dispose hardware collector
            if (_collector != null)
            {
                try { _collector.Dispose(); } catch { }
                _collector = null;
            }

            // 5. Hide and dispose tray icon
            try
            {
                _trayIcon.Visible = false;
                _trayIcon.Dispose();
            }
            catch { }

            // 6. Exit application
            Application.Exit();
        }

        // ====================================================================
        //  WMI Port Discovery (Smart Filter)
        // ====================================================================

        private class PortInfo
        {
            public string Name { get; set; }      // e.g., "COM3"
            public string Caption { get; set; }   // e.g., "Silicon Labs CP210x USB to UART Bridge (COM3)"
            public bool ShouldSkip { get; set; }
            public bool IsPreferred { get; set; }
        }

        private List<PortInfo> GetSerialPortsWithWmi()
        {
            var result = new List<PortInfo>();

            try
            {
                // Get all COM ports from system
                var systemPorts = SerialPort.GetPortNames().ToHashSet(StringComparer.OrdinalIgnoreCase);
                if (systemPorts.Count == 0) return result;

                // Query WMI for port details
                using (var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_PnPEntity WHERE Caption LIKE '%(COM%'"))
                {
                    foreach (ManagementObject obj in searcher.Get())
                    {
                        try
                        {
                            string caption = obj["Caption"]?.ToString() ?? "";

                            // Extract COM port name from caption
                            int start = caption.LastIndexOf("(COM");
                            int end = caption.LastIndexOf(")");
                            if (start >= 0 && end > start)
                            {
                                string portName = caption.Substring(start + 1, end - start - 1);
                                if (systemPorts.Contains(portName))
                                {
                                    var info = new PortInfo
                                    {
                                        Name = portName,
                                        Caption = caption,
                                        ShouldSkip = SKIP_PORT_KEYWORDS.Any(k =>
                                            caption.IndexOf(k, StringComparison.OrdinalIgnoreCase) >= 0),
                                        IsPreferred = PREFER_PORT_KEYWORDS.Any(k =>
                                            caption.IndexOf(k, StringComparison.OrdinalIgnoreCase) >= 0)
                                    };
                                    result.Add(info);
                                    systemPorts.Remove(portName);
                                }
                            }
                        }
                        catch { }
                    }
                }

                // Add any remaining ports (not found in WMI) as unknown
                foreach (var port in systemPorts)
                {
                    result.Add(new PortInfo
                    {
                        Name = port,
                        Caption = port + " (Unknown device)",
                        ShouldSkip = false,
                        IsPreferred = false
                    });
                }
            }
            catch (Exception ex)
            {
                Program.LogCrash("GetSerialPortsWithWmi", ex);

                // Fallback: just use system ports without filtering
                foreach (var port in SerialPort.GetPortNames())
                {
                    result.Add(new PortInfo
                    {
                        Name = port,
                        Caption = port,
                        ShouldSkip = false,
                        IsPreferred = false
                    });
                }
            }

            return result;
        }

        // ====================================================================
        //  Background Loop (Serial Communication)
        // ====================================================================

        private void BackgroundLoop(CancellationToken ct)
        {
            try
            {
                // Initialize hardware collector
                SafeUpdateUI(() => _statusForm.AppendLog("[Init] Starting hardware collector..."));
                SafeUpdateUI(() => _statusForm.AppendLog("[Init] Admin: " + (HardwareCollector.IsAdmin() ? "YES" : "NO")));

                _collector = new HardwareCollector();
                _isLiteMode = _collector.IsLiteMode;

                SafeUpdateUI(() => _statusForm.AppendLog("[Init] " + _collector.InitStatus));
                SafeUpdateUI(() => _statusForm.UpdateMode(_isLiteMode, HardwareCollector.IsAdmin()));

                // Get CLI argument port if provided
                string cliPort = (_args.Length > 0) ? _args[0] : null;

                while (!ct.IsCancellationRequested)
                {
                    // Discover port via handshake (with smart filtering)
                    string targetPort = cliPort ?? FindEsp32ByHandshake(ct);

                    if (targetPort == null)
                    {
                        UpdateTrayStatus(false, "Searching...");
                        if (ct.IsCancellationRequested) break;

                        // Sleep in small chunks for quick cancellation
                        for (int i = 0; i < 30 && !ct.IsCancellationRequested; i++)
                            Thread.Sleep(100);
                        continue;
                    }

                    UpdateTrayStatus(false, "Connecting to " + targetPort);
                    SafeUpdateUI(() => _statusForm.AppendLog("[Serial] Connecting to " + targetPort + "..."));

                    try
                    {
                        using (var port = new SerialPort(targetPort, 115200))
                        {
                            port.WriteTimeout = 1000;
                            port.ReadTimeout = 500;
                            port.DtrEnable = true;

                            lock (_serialLock)
                            {
                                if (ct.IsCancellationRequested) break;
                                port.Open();
                                _activeSerialPort = port;
                            }

                            // Give ESP32 time to reset after DTR (sleep in chunks)
                            for (int i = 0; i < 20 && !ct.IsCancellationRequested; i++)
                                Thread.Sleep(100);

                            if (ct.IsCancellationRequested) break;

                            // Verify handshake
                            if (!VerifyHandshake(port))
                            {
                                SafeUpdateUI(() => _statusForm.AppendLog("[Serial] Handshake FAILED"));
                                UpdateTrayStatus(false, "Handshake failed");
                                lock (_serialLock) { _activeSerialPort = null; }

                                for (int i = 0; i < 20 && !ct.IsCancellationRequested; i++)
                                    Thread.Sleep(100);
                                continue;
                            }

                            SafeUpdateUI(() => _statusForm.AppendLog("[Serial] Handshake OK!"));
                            _currentPort = targetPort;
                            UpdateTrayStatus(true, "Connected (" + targetPort + ")");

                            // Main data loop
                            RunDataLoop(_collector, port, ct);

                            lock (_serialLock) { _activeSerialPort = null; }
                        }
                    }
                    catch (OperationCanceledException)
                    {
                        break;
                    }
                    catch (Exception ex)
                    {
                        Program.LogCrash("SerialConnect", ex);
                        SafeUpdateUI(() => _statusForm.AppendLog("[Serial] Error: " + ex.Message));
                        UpdateTrayStatus(false, "Error");
                        lock (_serialLock) { _activeSerialPort = null; }

                        if (!ct.IsCancellationRequested)
                        {
                            for (int i = 0; i < 20 && !ct.IsCancellationRequested; i++)
                                Thread.Sleep(100);
                        }
                    }

                    // If auto-detected, re-scan next iteration
                    if (_args.Length == 0)
                        cliPort = null;
                }
            }
            catch (OperationCanceledException)
            {
                // Normal shutdown
            }
            catch (Exception ex)
            {
                // Log to file
                Program.LogCrash("BackgroundLoop", ex);

                // Show in UI (thread-safe)
                SafeUpdateUI(() =>
                {
                    _statusForm.AppendLog("[FATAL] " + ex.Message);
                    _statusForm.AppendLog(ex.StackTrace ?? "");
                });

                UpdateTrayStatus(false, "Fatal error");

                if (!_isShuttingDown)
                {
                    // Show message box
                    MessageBox.Show("Fatal error in monitor loop:\n\n" + ex.Message + "\n\nSee crash_log.txt for details.",
                        "Scarab Monitor Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void RunDataLoop(HardwareCollector collector, SerialPort port, CancellationToken ct)
        {
            // Detect network info once
            string netType, netSpeed;
            DetectNetworkInfo(out netType, out netSpeed);

            while (!ct.IsCancellationRequested && port.IsOpen)
            {
                try
                {
                    var s = collector.GetStats();

                    // Format data string
                    string data = string.Format(
                        CultureInfo.InvariantCulture,
                        "CPU:{0},CPUT:{1:0.0},GPU:{2},GPUT:{3:0.0},VRAM:{4:0.0}/{5:0.0},RAM:{6:0.0}/{7:0.0},NET:{8},SPEED:{9},DOWN:{10:0.0},UP:{11:0.0}\n",
                        (int)s.CpuLoad, s.CpuTemp,
                        (int)s.GpuLoad, s.GpuTemp,
                        s.GpuVramUsed, s.GpuVramTotal,
                        s.RamUsedGb, s.RamTotalGb,
                        netType, netSpeed,
                        s.NetDown, s.NetUp);

                    port.Write(data);
                    port.BaseStream.Flush();

                    // Update status form with current data (thread-safe)
                    string displayData = data.TrimEnd();
                    SafeUpdateUI(() => _statusForm.UpdateData(displayData));

                    // Sleep in small increments to allow quick cancellation
                    for (int i = 0; i < 10 && !ct.IsCancellationRequested; i++)
                        Thread.Sleep(100);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Program.LogCrash("RunDataLoop", ex);
                    SafeUpdateUI(() => _statusForm.AppendLog("[TX] Error: " + ex.Message));
                    UpdateTrayStatus(false, "Disconnected");
                    break;
                }
            }
        }

        // ====================================================================
        //  Handshake & Port Discovery
        // ====================================================================

        private bool VerifyHandshake(SerialPort port)
        {
            try
            {
                port.DiscardInBuffer();
                port.Write(HANDSHAKE_QUERY);
                port.BaseStream.Flush();

                // Short timeout for handshake response
                int originalTimeout = port.ReadTimeout;
                port.ReadTimeout = 200;

                try
                {
                    string response = port.ReadLine();
                    return response != null && response.Trim().Contains(HANDSHAKE_RESPONSE);
                }
                finally
                {
                    port.ReadTimeout = originalTimeout;
                }
            }
            catch (TimeoutException) { }
            catch (Exception ex)
            {
                Program.LogCrash("VerifyHandshake", ex);
            }
            return false;
        }

        private string FindEsp32ByHandshake(CancellationToken ct)
        {
            var ports = GetSerialPortsWithWmi();
            if (ports.Count == 0) return null;

            // Sort: preferred first, then by name descending, skip JTAG/debug ports
            var sorted = ports
                .Where(p => !p.ShouldSkip)
                .OrderByDescending(p => p.IsPreferred)
                .ThenByDescending(p => p.Name)
                .ToList();

            // Log what we found
            SafeUpdateUI(() =>
            {
                _statusForm.AppendLog("[Serial] Found " + ports.Count + " ports:");
                foreach (var p in ports)
                {
                    string status = p.ShouldSkip ? " [SKIP:JTAG]" : (p.IsPreferred ? " [PREFER]" : "");
                    _statusForm.AppendLog("  " + p.Name + ": " + p.Caption + status);
                }
            });

            if (sorted.Count == 0)
            {
                SafeUpdateUI(() => _statusForm.AppendLog("[Serial] No suitable ports after filtering"));
                return null;
            }

            SafeUpdateUI(() => _statusForm.AppendLog("[Serial] Scanning " + sorted.Count + " ports..."));

            foreach (var portInfo in sorted)
            {
                if (ct.IsCancellationRequested) return null;

                SafeUpdateUI(() => _statusForm.AppendLog("[Serial] Trying " + portInfo.Name + "..."));

                try
                {
                    using (var port = new SerialPort(portInfo.Name, 115200))
                    {
                        port.WriteTimeout = 500;
                        port.ReadTimeout = 200;  // Short timeout!
                        port.DtrEnable = true;
                        port.Open();

                        // Brief delay for device to initialize
                        Thread.Sleep(1000);

                        if (VerifyHandshake(port))
                        {
                            SafeUpdateUI(() => _statusForm.AppendLog("[Serial] Found ESP32 on " + portInfo.Name));
                            port.Close();
                            return portInfo.Name;
                        }

                        port.Close();
                    }
                }
                catch (UnauthorizedAccessException)
                {
                    SafeUpdateUI(() => _statusForm.AppendLog("[Serial] " + portInfo.Name + " - Access denied (in use)"));
                }
                catch (Exception ex)
                {
                    // Port busy or other error - skip silently
                    Program.LogCrash("ScanPort_" + portInfo.Name, ex);
                }
            }

            return null;
        }

        // ====================================================================
        //  Network Info
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

                long speedMbps = iface.Speed / 1000000;
                netSpeed = speedMbps + " Mbps";
            }
            catch { }
        }
    }
}
