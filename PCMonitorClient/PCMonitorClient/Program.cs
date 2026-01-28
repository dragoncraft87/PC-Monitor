using System;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PCMonitorClient
{
    internal static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new TrayContext(args));
        }
    }

    internal class TrayContext : ApplicationContext
    {
        private const string HANDSHAKE_QUERY = "WHO_ARE_YOU?\n";
        private const string HANDSHAKE_RESPONSE = "SCARAB_CLIENT_OK";

        private readonly NotifyIcon _trayIcon;
        private readonly StatusForm _statusForm;
        private readonly ContextMenuStrip _contextMenu;
        private readonly ToolStripMenuItem _menuShowStatus;
        private readonly ToolStripMenuItem _menuRestartAdmin;
        private readonly ToolStripMenuItem _menuExit;

        private readonly Icon _iconBase;
        private readonly string[] _args;

        private volatile bool _isConnected = false;
        private volatile string _currentPort = "";
        private CancellationTokenSource _cts;

        public TrayContext(string[] args)
        {
            _args = args;

            // Load or create base icon
            _iconBase = LoadOrCreateIcon();

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
                Icon = CreateStatusIcon(false),
                Text = "Scarab Monitor: Starting...",
                ContextMenuStrip = _contextMenu,
                Visible = true
            };
            _trayIcon.DoubleClick += (s, e) => OnShowStatus(s, e);

            // Start background worker
            _cts = new CancellationTokenSource();
            Task.Run(() => BackgroundLoop(_cts.Token));
        }

        // ====================================================================
        //  Icon Helpers
        // ====================================================================

        private Icon LoadOrCreateIcon()
        {
            // Try to load icon.ico from app directory
            string iconPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "icon.ico");
            if (File.Exists(iconPath))
            {
                try
                {
                    return new Icon(iconPath);
                }
                catch { }
            }

            // Fallback: use system application icon
            return SystemIcons.Application;
        }

        private Icon CreateStatusIcon(bool connected)
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
                    Color dotColor = connected ? Color.LimeGreen : Color.Red;
                    using (Brush dotBrush = new SolidBrush(dotColor))
                    {
                        g.FillEllipse(dotBrush, 10, 10, 5, 5);
                    }
                }

                return Icon.FromHandle(bmp.GetHicon());
            }
        }

        private void UpdateTrayStatus(bool connected, string status)
        {
            _isConnected = connected;

            // Update icon (must be on UI thread if called from background)
            if (_trayIcon != null)
            {
                try
                {
                    _trayIcon.Icon = CreateStatusIcon(connected);
                    _trayIcon.Text = "Scarab Monitor: " + status;
                }
                catch { }
            }

            // Update status form
            _statusForm.UpdateConnectionStatus(connected, status);
        }

        // ====================================================================
        //  Menu Handlers
        // ====================================================================

        private void OnShowStatus(object sender, EventArgs e)
        {
            _statusForm.Show();
            _statusForm.BringToFront();
            _statusForm.Focus();
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
                Application.Exit();
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to restart as admin: " + ex.Message,
                    "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void OnExit(object sender, EventArgs e)
        {
            _cts.Cancel();
            _trayIcon.Visible = false;
            _trayIcon.Dispose();
            Application.Exit();
        }

        // ====================================================================
        //  Background Loop (Serial Communication)
        // ====================================================================

        private void BackgroundLoop(CancellationToken ct)
        {
            HardwareCollector collector = null;

            try
            {
                // Initialize hardware collector
                _statusForm.AppendLog("[Init] Starting hardware collector...");
                collector = new HardwareCollector();
                _statusForm.AppendLog("[Init] Hardware collector ready.");

                // Get CLI argument port if provided
                string cliPort = (_args.Length > 0) ? _args[0] : null;

                while (!ct.IsCancellationRequested)
                {
                    // Discover port via handshake
                    string targetPort = cliPort ?? FindEsp32ByHandshake(ct);

                    if (targetPort == null)
                    {
                        UpdateTrayStatus(false, "Searching...");
                        Thread.Sleep(3000);
                        continue;
                    }

                    UpdateTrayStatus(false, "Connecting to " + targetPort);
                    _statusForm.AppendLog("[Serial] Connecting to " + targetPort + "...");

                    try
                    {
                        using (var port = new SerialPort(targetPort, 115200))
                        {
                            port.WriteTimeout = 2000;
                            port.ReadTimeout = 1000;
                            port.DtrEnable = true;
                            port.Open();

                            // Give ESP32 time to reset after DTR
                            Thread.Sleep(2000);

                            // Verify handshake
                            if (!VerifyHandshake(port))
                            {
                                _statusForm.AppendLog("[Serial] Handshake FAILED");
                                UpdateTrayStatus(false, "Handshake failed");
                                Thread.Sleep(2000);
                                continue;
                            }

                            _statusForm.AppendLog("[Serial] Handshake OK!");
                            _currentPort = targetPort;
                            UpdateTrayStatus(true, "Connected (" + targetPort + ")");

                            // Main data loop
                            RunDataLoop(collector, port, ct);
                        }
                    }
                    catch (Exception ex)
                    {
                        _statusForm.AppendLog("[Serial] Error: " + ex.Message);
                        UpdateTrayStatus(false, "Error: " + ex.Message);
                        Thread.Sleep(2000);
                    }

                    // If auto-detected, re-scan next iteration
                    if (_args.Length == 0)
                        cliPort = null;
                }
            }
            catch (Exception ex)
            {
                _statusForm.AppendLog("[Fatal] " + ex.Message);
                UpdateTrayStatus(false, "Fatal error");
            }
            finally
            {
                if (collector != null)
                    collector.Dispose();
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

                    // Update status form with current data
                    _statusForm.UpdateData(data.TrimEnd());

                    Thread.Sleep(1000);
                }
                catch (Exception ex)
                {
                    _statusForm.AppendLog("[TX] Error: " + ex.Message);
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

                string response = port.ReadLine();
                return response != null && response.Trim().Contains(HANDSHAKE_RESPONSE);
            }
            catch (TimeoutException) { }
            catch { }
            return false;
        }

        private string FindEsp32ByHandshake(CancellationToken ct)
        {
            var ports = SerialPort.GetPortNames();
            if (ports.Length == 0) return null;

            var sorted = ports.OrderByDescending(p => p).ToArray();
            _statusForm.AppendLog("[Serial] Scanning: " + string.Join(", ", sorted));

            foreach (var name in sorted)
            {
                if (ct.IsCancellationRequested) return null;

                try
                {
                    using (var port = new SerialPort(name, 115200))
                    {
                        port.WriteTimeout = 1000;
                        port.ReadTimeout = 500;
                        port.DtrEnable = true;
                        port.Open();

                        Thread.Sleep(1500);

                        if (VerifyHandshake(port))
                        {
                            _statusForm.AppendLog("[Serial] Found ESP32 on " + name);
                            port.Close();
                            return name;
                        }

                        port.Close();
                    }
                }
                catch { }
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
