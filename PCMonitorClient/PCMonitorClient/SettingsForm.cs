using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PCMonitorClient
{
    /// <summary>
    /// Settings form for configuring ESP32 theme colors and uploading custom images.
    /// Features multi-device management, profile system, and live preview.
    /// </summary>
    public class SettingsForm : Form
    {
        // Tactical Metal Theme Colors
        private static readonly Color ThemeBgDark = Color.FromArgb(12, 12, 16);
        private static readonly Color ThemeBgMedium = Color.FromArgb(22, 22, 28);
        private static readonly Color ThemeBgLight = Color.FromArgb(32, 32, 40);
        private static readonly Color ThemeBgPanel = Color.FromArgb(26, 26, 34);
        private static readonly Color ThemeAccent = Color.FromArgb(0, 200, 120);
        private static readonly Color ThemeAccentDim = Color.FromArgb(0, 140, 85);
        private static readonly Color ThemeAccentBright = Color.FromArgb(0, 255, 150);
        private static readonly Color ThemeTextPrimary = Color.FromArgb(240, 240, 245);
        private static readonly Color ThemeTextSecondary = Color.FromArgb(140, 140, 155);
        private static readonly Color ThemeBorder = Color.FromArgb(45, 45, 55);
        private static readonly Color ThemeWarning = Color.FromArgb(255, 180, 60);
        private static readonly Color ThemeError = Color.FromArgb(255, 85, 85);

        // Callbacks
        public Action<string> SendCommand { get; set; }
        public Func<bool> IsConnected { get; set; }
        public Func<System.IO.Ports.SerialPort> GetSerialPort { get; set; }
        public Action<bool> SetUploadMode { get; set; }
        public Action<bool> SetPaused { get; set; }
        public Func<bool> IsPaused { get; set; }
        public Func<string[]> GetAvailablePorts { get; set; }
        public Action<string> OnDeviceSelected { get; set; }

        // Current device info (set by main program)
        private string _connectedDeviceName = "";
        private string _connectedDeviceId = "";

        private TabControl _tabControl;
        private TabPage _tabDashboard;
        private TabPage _tabColors;
        private TabPage _tabProfiles;

        // Device status bar
        private Panel _panelDeviceStatus;
        private Label _lblDeviceStatus;
        private Label _lblDeviceName;
        private TextBox _txtDeviceName;
        private Button _btnSetDeviceName;
        private ComboBox _cboDevices;
        private Button _btnRefresh;
        private Button _btnPauseResume;

        // Live preview panels (1x4 grid)
        private CircularDisplayPanel[] _displayPanels;
        private Label[] _displayLabels;

        // Color state for live preview
        private Color _cpuArcColor = Color.FromArgb(0x00, 0x71, 0xC5);
        private Color _gpuArcColor = Color.FromArgb(0x76, 0xB9, 0x00);
        private Color _ramBarColor = Color.FromArgb(0x43, 0xE9, 0x7B);
        private Color _netColor = Color.FromArgb(0xC8, 0x96, 0x32);
        private Color _arcBgColor = Color.FromArgb(0x55, 0x55, 0x5C);

        // Color picker panels
        private Panel _panelCpuArc, _panelGpuArc, _panelArcBg;
        private Panel _panelRamBar, _panelNetDown, _panelNetUp;
        private Panel _panelBgCpu, _panelBgGpu, _panelBgRam, _panelBgNet;

        // Screensaver background color panels
        private Panel _panelSsBgCpu, _panelSsBgGpu, _panelSsBgRam, _panelSsBgNet;

        // Profile system
        private TextBox _txtProfileName;
        private ListBox _lstProfiles;

        // Upload state
        private ProgressBar _progressUpload;
        private Label _lblUploadStatus;
        private CancellationTokenSource _uploadCts;
        private bool _isUploading;

        // Debug log panel (fixed, always visible at bottom)
        private RichTextBox _rtbDebugLog;
        private Panel _panelDebugLog;
        private Button _btnClearLog;

        // Profiles directory
        private static readonly string ProfilesDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "ScarabMonitor", "Profiles");

        public SettingsForm()
        {
            InitializeComponent();
            EnsureProfilesDirectory();

            // Initial log message to verify logging works
            Load += (s, e) => AppendDebugLog("SettingsForm loaded - Debug logging active");
        }

        private void InitializeComponent()
        {
            // DPI Scaling Support
            AutoScaleMode = AutoScaleMode.Dpi;
            AutoScaleDimensions = new SizeF(96F, 96F);

            Text = "Scarab Monitor - Device Manager";
            Size = new Size(900, 860);  // Increased height for fixed log panel
            StartPosition = FormStartPosition.CenterScreen;
            FormBorderStyle = FormBorderStyle.Sizable;
            MaximizeBox = false;
            MinimumSize = new Size(900, 860);
            BackColor = ThemeBgDark;
            ForeColor = ThemeTextPrimary;
            Font = new Font("Segoe UI", 9F);

            // === Device Status Bar (Top) ===
            CreateDeviceStatusBar();

            // === Live Preview Panel (1x4 Grid) ===
            CreateLivePreviewPanel();

            // === Tab Control ===
            _tabControl = new TabControl
            {
                Location = new Point(10, 310),
                Size = new Size(865, 325),
                DrawMode = TabDrawMode.OwnerDrawFixed,
                ItemSize = new Size(120, 30)
            };
            _tabControl.DrawItem += TabControl_DrawItem;

            // === Dashboard Tab ===
            _tabDashboard = new TabPage("Quick Settings")
            {
                BackColor = ThemeBgMedium
            };
            CreateDashboardTab();
            _tabControl.TabPages.Add(_tabDashboard);

            // === Colors Tab ===
            _tabColors = new TabPage("Theme Colors")
            {
                BackColor = ThemeBgMedium
            };
            CreateColorsTab();
            _tabControl.TabPages.Add(_tabColors);

            // === Profiles Tab ===
            _tabProfiles = new TabPage("Profiles")
            {
                BackColor = ThemeBgMedium
            };
            CreateProfilesTab();
            _tabControl.TabPages.Add(_tabProfiles);

            Controls.Add(_tabControl);

            // === Debug Log Panel (Bottom, Collapsible) ===
            CreateDebugLogPanel();
        }

        private void TabControl_DrawItem(object sender, DrawItemEventArgs e)
        {
            var tab = _tabControl.TabPages[e.Index];
            var bounds = _tabControl.GetTabRect(e.Index);
            bool selected = (_tabControl.SelectedIndex == e.Index);

            using (var brush = new SolidBrush(selected ? ThemeBgLight : ThemeBgMedium))
            {
                e.Graphics.FillRectangle(brush, bounds);
            }

            using (var brush = new SolidBrush(selected ? ThemeAccent : ThemeTextSecondary))
            {
                var sf = new StringFormat
                {
                    Alignment = StringAlignment.Center,
                    LineAlignment = StringAlignment.Center
                };
                e.Graphics.DrawString(tab.Text, Font, brush, bounds, sf);
            }
        }

        #region Debug Log Panel

        private void CreateDebugLogPanel()
        {
            // Container panel - FIXED at bottom, always visible
            _panelDebugLog = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 180,
                BackColor = Color.Black,
                Padding = new Padding(5)
            };

            // Clear button - floating top-right
            _btnClearLog = new Button
            {
                Text = "Clear",
                Size = new Size(70, 24),
                FlatStyle = FlatStyle.Flat,
                ForeColor = Color.White,
                BackColor = Color.FromArgb(50, 50, 50),
                Cursor = Cursors.Hand,
                Anchor = AnchorStyles.Top | AnchorStyles.Right
            };
            _btnClearLog.FlatAppearance.BorderColor = Color.Gray;
            _btnClearLog.Click += (s, e) =>
            {
                _rtbDebugLog.Clear();
                _rtbDebugLog.AppendText($"[{DateTime.Now:HH:mm:ss.fff}] Log cleared\n");
            };

            // RichTextBox - fills entire panel
            _rtbDebugLog = new RichTextBox
            {
                Dock = DockStyle.Fill,
                BackColor = Color.Black,
                ForeColor = Color.LimeGreen,
                Font = new Font("Consolas", 9F),
                ReadOnly = true,
                BorderStyle = BorderStyle.None,
                ScrollBars = RichTextBoxScrollBars.Vertical,
                WordWrap = false
            };

            // Add controls to panel (order matters for docking)
            _panelDebugLog.Controls.Add(_rtbDebugLog);
            _panelDebugLog.Controls.Add(_btnClearLog);

            // Position clear button after adding (needs parent for anchor to work)
            _btnClearLog.Location = new Point(_panelDebugLog.Width - _btnClearLog.Width - 10, 5);

            // Add panel to form
            Controls.Add(_panelDebugLog);

            // Initial message
            _rtbDebugLog.AppendText($"[{DateTime.Now:HH:mm:ss.fff}] Debug Log Panel initialized (Fixed Mode)\n");
        }

        /// <summary>
        /// Appends a timestamped message to the debug log (thread-safe).
        /// </summary>
        public void AppendDebugLog(string message)
        {
            if (_rtbDebugLog == null || _rtbDebugLog.IsDisposed)
                return;

            if (InvokeRequired)
            {
                try
                {
                    BeginInvoke((MethodInvoker)delegate { AppendDebugLog(message); });
                }
                catch { }
                return;
            }

            try
            {
                string timestamp = DateTime.Now.ToString("HH:mm:ss.fff");
                _rtbDebugLog.AppendText($"[{timestamp}] {message}\n");

                // Auto-scroll to bottom
                _rtbDebugLog.SelectionStart = _rtbDebugLog.TextLength;
                _rtbDebugLog.ScrollToCaret();
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"AppendDebugLog failed: {ex.Message}");
            }
        }

        #endregion

        #region Device Status Bar

        private void CreateDeviceStatusBar()
        {
            _panelDeviceStatus = new Panel
            {
                Location = new Point(10, 8),
                Size = new Size(865, 70),
                BackColor = ThemeBgPanel
            };

            // Draw border
            _panelDeviceStatus.Paint += (s, e) =>
            {
                using (var pen = new Pen(ThemeBorder, 1))
                {
                    e.Graphics.DrawRectangle(pen, 0, 0, _panelDeviceStatus.Width - 1, _panelDeviceStatus.Height - 1);
                }
            };

            // Connection status indicator
            _lblDeviceStatus = new Label
            {
                Text = "DISCONNECTED",
                Location = new Point(15, 8),
                Size = new Size(150, 20),
                ForeColor = ThemeError,
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };
            _panelDeviceStatus.Controls.Add(_lblDeviceStatus);

            // Device name display
            _lblDeviceName = new Label
            {
                Text = "No device connected",
                Location = new Point(15, 32),
                Size = new Size(300, 25),
                ForeColor = ThemeTextSecondary,
                Font = new Font("Segoe UI", 11F, FontStyle.Bold)
            };
            _panelDeviceStatus.Controls.Add(_lblDeviceName);

            // Rename section
            var lblRename = new Label
            {
                Text = "Rename Device:",
                Location = new Point(340, 10),
                AutoSize = true,
                ForeColor = ThemeTextSecondary
            };
            _panelDeviceStatus.Controls.Add(lblRename);

            _txtDeviceName = new TextBox
            {
                Location = new Point(340, 32),
                Size = new Size(180, 25),
                BackColor = ThemeBgDark,
                ForeColor = ThemeTextPrimary,
                BorderStyle = BorderStyle.FixedSingle,
                MaxLength = 20
            };
            _txtDeviceName.KeyPress += (s, e) =>
            {
                if (e.KeyChar == (char)Keys.Enter)
                {
                    SetDeviceName();
                    e.Handled = true;
                }
            };
            _panelDeviceStatus.Controls.Add(_txtDeviceName);

            _btnSetDeviceName = CreateStyledButton("Set Name", 525, 31, 80, 27);
            _btnSetDeviceName.Click += (s, e) => SetDeviceName();
            _panelDeviceStatus.Controls.Add(_btnSetDeviceName);

            // Port selector
            var lblPort = new Label
            {
                Text = "COM Port:",
                Location = new Point(630, 10),
                AutoSize = true,
                ForeColor = ThemeTextSecondary
            };
            _panelDeviceStatus.Controls.Add(lblPort);

            _cboDevices = new ComboBox
            {
                Location = new Point(630, 32),
                Size = new Size(130, 25),
                DropDownStyle = ComboBoxStyle.DropDownList,
                BackColor = ThemeBgDark,
                ForeColor = ThemeTextPrimary,
                FlatStyle = FlatStyle.Flat
            };
            _cboDevices.SelectedIndexChanged += (s, e) =>
            {
                if (_cboDevices.SelectedItem != null)
                    OnDeviceSelected?.Invoke(_cboDevices.SelectedItem.ToString());
            };
            _panelDeviceStatus.Controls.Add(_cboDevices);

            _btnRefresh = CreateStyledButton("Scan", 770, 31, 70, 27);
            _btnRefresh.Click += (s, e) => RefreshDeviceList();
            _panelDeviceStatus.Controls.Add(_btnRefresh);

            // Pause/Resume button (prominent, next to status)
            _btnPauseResume = new Button
            {
                Text = "⏸ Pause",
                Location = new Point(170, 6),
                Size = new Size(90, 26),
                FlatStyle = FlatStyle.Flat,
                ForeColor = Color.White,
                BackColor = Color.FromArgb(200, 130, 0),  // Orange
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                Cursor = Cursors.Hand
            };
            _btnPauseResume.FlatAppearance.BorderColor = Color.FromArgb(255, 180, 60);
            _btnPauseResume.FlatAppearance.MouseOverBackColor = Color.FromArgb(230, 150, 20);
            _btnPauseResume.Click += BtnPauseResume_Click;
            _panelDeviceStatus.Controls.Add(_btnPauseResume);

            Controls.Add(_panelDeviceStatus);
        }

        private void BtnPauseResume_Click(object sender, EventArgs e)
        {
            if (SetPaused == null || IsPaused == null) return;

            bool currentlyPaused = IsPaused();
            bool newState = !currentlyPaused;

            SetPaused(newState);
            UpdatePauseButtonState(newState);

            AppendDebugLog(newState ? "Data transmission PAUSED by user" : "Data transmission RESUMED");
        }

        private void UpdatePauseButtonState(bool isPaused)
        {
            if (_btnPauseResume == null) return;

            if (isPaused)
            {
                _btnPauseResume.Text = "▶ Resume";
                _btnPauseResume.BackColor = Color.FromArgb(0, 150, 80);  // Green
                _btnPauseResume.FlatAppearance.BorderColor = Color.FromArgb(0, 200, 100);
                _btnPauseResume.FlatAppearance.MouseOverBackColor = Color.FromArgb(0, 180, 100);
            }
            else
            {
                _btnPauseResume.Text = "⏸ Pause";
                _btnPauseResume.BackColor = Color.FromArgb(200, 130, 0);  // Orange
                _btnPauseResume.FlatAppearance.BorderColor = Color.FromArgb(255, 180, 60);
                _btnPauseResume.FlatAppearance.MouseOverBackColor = Color.FromArgb(230, 150, 20);
            }
        }

        private void SetDeviceName()
        {
            string newName = _txtDeviceName.Text.Trim();
            if (string.IsNullOrEmpty(newName))
            {
                MessageBox.Show("Please enter a device name.", "Invalid Name",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (!IsConnected?.Invoke() ?? true)
            {
                MessageBox.Show("Not connected to any device!", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            SendCommandSafe($"SET_ID:{newName}");
            _lblDeviceName.Text = newName;
            _connectedDeviceName = newName;
        }

        public void RefreshDeviceList()
        {
            _cboDevices.Items.Clear();
            if (GetAvailablePorts != null)
            {
                var ports = GetAvailablePorts();
                foreach (var port in ports)
                    _cboDevices.Items.Add(port);

                if (_cboDevices.Items.Count > 0)
                    _cboDevices.SelectedIndex = 0;
            }
        }

        public void SetConnectionStatus(bool connected, string deviceName = "", string deviceId = "")
        {
            if (InvokeRequired)
            {
                BeginInvoke((MethodInvoker)delegate { SetConnectionStatus(connected, deviceName, deviceId); });
                return;
            }

            _connectedDeviceName = deviceName;
            _connectedDeviceId = deviceId;

            if (connected)
            {
                _lblDeviceStatus.Text = "CONNECTED";
                _lblDeviceStatus.ForeColor = ThemeAccent;
                _lblDeviceName.Text = string.IsNullOrEmpty(deviceName) ? "Scarab Monitor" : deviceName;
                _lblDeviceName.ForeColor = ThemeTextPrimary;
                _txtDeviceName.Text = deviceName;

                // Check if this is a new device
                if (!string.IsNullOrEmpty(deviceId) && IsNewDevice(deviceId))
                {
                    PromptLoadProfile(deviceId);
                }
            }
            else
            {
                _lblDeviceStatus.Text = "DISCONNECTED";
                _lblDeviceStatus.ForeColor = ThemeError;
                _lblDeviceName.Text = "No device connected";
                _lblDeviceName.ForeColor = ThemeTextSecondary;
                _txtDeviceName.Text = "";
            }
        }

        #endregion

        #region Live Preview Panel

        private void CreateLivePreviewPanel()
        {
            var panelPreview = new Panel
            {
                Location = new Point(10, 85),
                Size = new Size(865, 220),
                BackColor = ThemeBgPanel
            };

            // Draw border and title
            panelPreview.Paint += (s, e) =>
            {
                using (var pen = new Pen(ThemeBorder, 1))
                {
                    e.Graphics.DrawRectangle(pen, 0, 0, panelPreview.Width - 1, panelPreview.Height - 1);
                }

                using (var brush = new SolidBrush(ThemeAccent))
                {
                    e.Graphics.DrawString("LIVE DISPLAY PREVIEW", new Font("Segoe UI", 10F, FontStyle.Bold),
                        brush, new Point(15, 8));
                }

                using (var brush = new SolidBrush(ThemeTextSecondary))
                {
                    e.Graphics.DrawString("Drag & drop images onto displays. The circular mask shows the visible area on GC9A01 (240x240).",
                        Font, brush, new Point(220, 10));
                }
            };

            // Create 4 circular display panels in 1x4 horizontal grid
            _displayPanels = new CircularDisplayPanel[4];
            _displayLabels = new Label[4];
            string[] labels = { "CPU", "GPU", "RAM", "NET" };
            Color[] accentColors = { _cpuArcColor, _gpuArcColor, _ramBarColor, _netColor };

            int panelSize = 160;
            int spacing = 45;
            int startX = 50;
            int startY = 40;

            for (int i = 0; i < 4; i++)
            {
                int x = startX + i * (panelSize + spacing);

                // Label above panel
                _displayLabels[i] = new Label
                {
                    Text = labels[i],
                    Location = new Point(x + panelSize / 2 - 20, startY - 5),
                    Size = new Size(40, 18),
                    ForeColor = accentColors[i],
                    Font = new Font("Segoe UI", 10F, FontStyle.Bold),
                    TextAlign = ContentAlignment.MiddleCenter
                };
                panelPreview.Controls.Add(_displayLabels[i]);

                // Circular display panel
                _displayPanels[i] = new CircularDisplayPanel
                {
                    Location = new Point(x, startY + 15),
                    Size = new Size(panelSize, panelSize),
                    AccentColor = accentColors[i],
                    SlotIndex = i,
                    AllowDrop = true,
                    ShowMask = true
                };
                _displayPanels[i].ImageDropped += DisplayPanel_ImageDropped;
                _displayPanels[i].Click += DisplayPanel_Click;
                panelPreview.Controls.Add(_displayPanels[i]);

                // Slot indicator
                var lblSlot = new Label
                {
                    Text = $"Slot {i}",
                    Location = new Point(x + panelSize / 2 - 20, startY + panelSize + 18),
                    Size = new Size(40, 15),
                    ForeColor = ThemeTextSecondary,
                    Font = new Font("Segoe UI", 7.5F),
                    TextAlign = ContentAlignment.MiddleCenter
                };
                panelPreview.Controls.Add(lblSlot);
            }

            // Upload progress area
            _progressUpload = new ProgressBar
            {
                Location = new Point(startX, startY + panelSize + 40),
                Size = new Size(700, 16),
                Style = ProgressBarStyle.Continuous
            };
            panelPreview.Controls.Add(_progressUpload);

            _lblUploadStatus = new Label
            {
                Text = "Drop images on displays to upload",
                Location = new Point(startX, startY + panelSize + 58),
                Size = new Size(700, 18),
                ForeColor = ThemeTextSecondary,
                Font = new Font("Segoe UI", 8F)
            };
            panelPreview.Controls.Add(_lblUploadStatus);

            Controls.Add(panelPreview);
        }

        private void DisplayPanel_Click(object sender, EventArgs e)
        {
            if (_isUploading) return;

            var panel = sender as CircularDisplayPanel;
            if (panel == null) return;

            using (var dlg = new OpenFileDialog())
            {
                dlg.Title = $"Select Image for {GetSlotName(panel.SlotIndex)} Display";
                dlg.Filter = "Image Files|*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.webp|All Files|*.*";

                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    LoadAndUploadImage(panel, dlg.FileName);
                }
            }
        }

        private void DisplayPanel_ImageDropped(object sender, string filePath)
        {
            var panel = sender as CircularDisplayPanel;
            if (panel != null)
            {
                LoadAndUploadImage(panel, filePath);
            }
        }

        private async void LoadAndUploadImage(CircularDisplayPanel panel, string filePath)
        {
            try
            {
                // Load preview
                panel.LoadImage(filePath);

                // Ask to upload if connected
                if (IsConnected?.Invoke() ?? false)
                {
                    var result = MessageBox.Show(
                        $"Upload image to {GetSlotName(panel.SlotIndex)} slot on ESP32?",
                        "Upload Image", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

                    if (result == DialogResult.Yes)
                    {
                        await UploadImageAsync(filePath, (ImageSlot)panel.SlotIndex);
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to load image: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private string GetSlotName(int slot)
        {
            switch (slot)
            {
                case 0: return "CPU";
                case 1: return "GPU";
                case 2: return "RAM";
                case 3: return "NET";
                default: return $"Slot {slot}";
            }
        }

        /// <summary>
        /// Updates the live preview accent colors when theme colors change.
        /// </summary>
        public void UpdatePreviewColors(Color cpuArc, Color gpuArc, Color ramBar, Color netColor)
        {
            if (InvokeRequired)
            {
                BeginInvoke((MethodInvoker)delegate { UpdatePreviewColors(cpuArc, gpuArc, ramBar, netColor); });
                return;
            }

            _cpuArcColor = cpuArc;
            _gpuArcColor = gpuArc;
            _ramBarColor = ramBar;
            _netColor = netColor;

            if (_displayPanels != null && _displayLabels != null)
            {
                Color[] colors = { cpuArc, gpuArc, ramBar, netColor };
                for (int i = 0; i < 4; i++)
                {
                    if (_displayPanels[i] != null)
                    {
                        _displayPanels[i].AccentColor = colors[i];
                        _displayPanels[i].Invalidate();
                    }
                    if (_displayLabels[i] != null)
                    {
                        _displayLabels[i].ForeColor = colors[i];
                    }
                }
            }
        }

        #endregion

        #region Dashboard Tab

        private void CreateDashboardTab()
        {
            int leftX = 20;
            int rightX = 450;
            int y = 20;

            // Quick Color Settings
            AddSectionLabel(_tabDashboard, "Quick Theme Colors", leftX, y);
            y += 30;

            var colorGrid = new Panel
            {
                Location = new Point(leftX, y),
                Size = new Size(400, 120),
                BackColor = ThemeBgLight
            };

            int cY = 15;
            _panelCpuArc = AddQuickColorRow(colorGrid, "CPU Arc:", 15, cY, _cpuArcColor, "SET_CLR_ARC_CPU", 0);
            _panelGpuArc = AddQuickColorRow(colorGrid, "GPU Arc:", 200, cY, _gpuArcColor, "SET_CLR_ARC_GPU", 1);
            cY += 35;
            _panelRamBar = AddQuickColorRow(colorGrid, "RAM Bar:", 15, cY, _ramBarColor, "SET_CLR_BAR_RAM", 2);
            _panelNetDown = AddQuickColorRow(colorGrid, "NET Down:", 200, cY, Color.FromArgb(0x00, 0xE6, 0x76), "SET_CLR_NET_DN", -1);
            cY += 35;
            _panelArcBg = AddQuickColorRow(colorGrid, "Arc BG:", 15, cY, _arcBgColor, "SET_CLR_ARC_BG", -1);
            _panelNetUp = AddQuickColorRow(colorGrid, "NET Up:", 200, cY, Color.FromArgb(0xFF, 0x6B, 0x6B), "SET_CLR_NET_UP", -1);

            _tabDashboard.Controls.Add(colorGrid);
            y += 135;

            // Quick Actions
            AddSectionLabel(_tabDashboard, "Quick Actions", leftX, y);
            y += 30;

            var btnResetTheme = CreateStyledButton("Reset to Defaults", leftX, y, 150, 32);
            btnResetTheme.Click += (s, e) => SendCommandSafe("RESET_THEME");
            _tabDashboard.Controls.Add(btnResetTheme);

            var btnSyncAll = CreateStyledButton("Sync All Images", leftX + 160, y, 150, 32);
            btnSyncAll.Click += BtnSyncAll_Click;
            _tabDashboard.Controls.Add(btnSyncAll);

            // Right column - Display settings
            y = 20;
            AddSectionLabel(_tabDashboard, "Display Settings", rightX, y);
            y += 30;

            var settingsPanel = new Panel
            {
                Location = new Point(rightX, y),
                Size = new Size(380, 140),
                BackColor = ThemeBgLight
            };

            int sY = 15;

            // Screensaver timeout
            var lblTimeout = new Label
            {
                Text = "Screensaver Timeout:",
                Location = new Point(15, sY + 3),
                AutoSize = true,
                ForeColor = ThemeTextSecondary
            };
            settingsPanel.Controls.Add(lblTimeout);

            var numTimeout = new NumericUpDown
            {
                Location = new Point(150, sY),
                Size = new Size(80, 25),
                Minimum = 10,
                Maximum = 3600,
                Value = 60,
                BackColor = ThemeBgDark,
                ForeColor = ThemeTextPrimary
            };
            settingsPanel.Controls.Add(numTimeout);

            var lblSec = new Label
            {
                Text = "seconds",
                Location = new Point(235, sY + 3),
                AutoSize = true,
                ForeColor = ThemeTextSecondary
            };
            settingsPanel.Controls.Add(lblSec);

            var btnSetTimeout = CreateStyledButton("Apply", 300, sY, 60, 25);
            btnSetTimeout.Click += (s, e) =>
            {
                SendCommandSafe($"SET_SS_TIMEOUT:{(int)numTimeout.Value}");
            };
            settingsPanel.Controls.Add(btnSetTimeout);
            sY += 45;

            // Rotation cycle time
            var lblRotation = new Label
            {
                Text = "Rotation Cycle:",
                Location = new Point(15, sY + 3),
                AutoSize = true,
                ForeColor = ThemeTextSecondary
            };
            settingsPanel.Controls.Add(lblRotation);

            var numRotation = new NumericUpDown
            {
                Location = new Point(150, sY),
                Size = new Size(80, 25),
                Minimum = 1,
                Maximum = 60,
                Value = 5,
                BackColor = ThemeBgDark,
                ForeColor = ThemeTextPrimary
            };
            settingsPanel.Controls.Add(numRotation);

            var lblSecRot = new Label
            {
                Text = "seconds",
                Location = new Point(235, sY + 3),
                AutoSize = true,
                ForeColor = ThemeTextSecondary
            };
            settingsPanel.Controls.Add(lblSecRot);

            var btnSetRotation = CreateStyledButton("Apply", 300, sY, 60, 25);
            btnSetRotation.Click += (s, e) =>
            {
                SendCommandSafe($"SET_ROTATION:{(int)numRotation.Value}");
            };
            settingsPanel.Controls.Add(btnSetRotation);

            _tabDashboard.Controls.Add(settingsPanel);
        }

        private Panel AddQuickColorRow(Control parent, string label, int x, int y, Color defaultColor, string command, int previewIndex)
        {
            var lbl = new Label
            {
                Text = label,
                Location = new Point(x, y + 4),
                Size = new Size(70, 20),
                ForeColor = ThemeTextSecondary
            };
            parent.Controls.Add(lbl);

            var panel = new Panel
            {
                Location = new Point(x + 75, y),
                Size = new Size(28, 28),
                BackColor = defaultColor,
                Cursor = Cursors.Hand,
                Tag = new object[] { command, previewIndex }
            };

            panel.Paint += (s, e) =>
            {
                using (var pen = new Pen(ThemeBorder, 2))
                {
                    e.Graphics.DrawRectangle(pen, 0, 0, panel.Width - 1, panel.Height - 1);
                }
            };

            panel.Click += QuickColorPanel_Click;
            parent.Controls.Add(panel);

            return panel;
        }

        private void QuickColorPanel_Click(object sender, EventArgs e)
        {
            var panel = sender as Panel;
            if (panel == null) return;

            var data = panel.Tag as object[];
            if (data == null) return;

            string command = data[0] as string;
            int previewIndex = (int)data[1];

            using (var dlg = new ColorDialog())
            {
                dlg.Color = panel.BackColor;
                dlg.FullOpen = true;

                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    panel.BackColor = dlg.Color;

                    // Send to ESP
                    if (!string.IsNullOrEmpty(command))
                    {
                        SendCommandSafe($"{command}:{ColorToHex(dlg.Color)}");
                    }

                    // Update live preview
                    if (previewIndex >= 0 && previewIndex < 4 && _displayPanels != null)
                    {
                        _displayPanels[previewIndex].AccentColor = dlg.Color;
                        _displayPanels[previewIndex].Invalidate();
                        _displayLabels[previewIndex].ForeColor = dlg.Color;
                    }
                }
            }
        }

        private async void BtnSyncAll_Click(object sender, EventArgs e)
        {
            if (_isUploading)
            {
                MessageBox.Show("Upload in progress!", "Busy", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (!IsConnected?.Invoke() ?? true)
            {
                MessageBox.Show("Not connected to ESP32!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            for (int i = 0; i < 4; i++)
            {
                if (_displayPanels[i].HasImage && !string.IsNullOrEmpty(_displayPanels[i].ImagePath))
                {
                    await UploadImageAsync(_displayPanels[i].ImagePath, (ImageSlot)i);
                }
            }
        }

        #endregion

        #region Colors Tab

        private void CreateColorsTab()
        {
            int y = 15;
            int labelWidth = 130;
            int panelSize = 32;
            int spacing = 42;

            // Group: Arc Colors
            AddSectionLabel(_tabColors, "Arc Colors", 15, y);
            y += 28;

            AddColorRow(_tabColors, "CPU Arc:", 15, y, labelWidth, panelSize, _cpuArcColor, "SET_CLR_ARC_CPU");
            y += spacing;
            AddColorRow(_tabColors, "GPU Arc:", 15, y, labelWidth, panelSize, _gpuArcColor, "SET_CLR_ARC_GPU");
            y += spacing;
            AddColorRow(_tabColors, "Arc Background:", 15, y, labelWidth, panelSize, _arcBgColor, "SET_CLR_ARC_BG");
            y += spacing + 15;

            // Group: Bar/Chart Colors
            AddSectionLabel(_tabColors, "Bar/Chart Colors", 15, y);
            y += 28;

            AddColorRow(_tabColors, "RAM Bar:", 15, y, labelWidth, panelSize, _ramBarColor, "SET_CLR_BAR_RAM");
            y += spacing;
            AddColorRow(_tabColors, "Net Download:", 15, y, labelWidth, panelSize, Color.FromArgb(0x00, 0xE6, 0x76), "SET_CLR_NET_DN");
            y += spacing;
            AddColorRow(_tabColors, "Net Upload:", 15, y, labelWidth, panelSize, Color.FromArgb(0xFF, 0x6B, 0x6B), "SET_CLR_NET_UP");

            // Right column - Background Colors
            int rightCol = 450;
            y = 15;

            AddSectionLabel(_tabColors, "Screen Backgrounds", rightCol, y);
            y += 28;

            _panelBgCpu = AddColorRow(_tabColors, "CPU Screen:", rightCol, y, labelWidth, panelSize, Color.Black, "SET_CLR_BG_NORM:0");
            y += spacing;
            _panelBgGpu = AddColorRow(_tabColors, "GPU Screen:", rightCol, y, labelWidth, panelSize, Color.Black, "SET_CLR_BG_NORM:1");
            y += spacing;
            _panelBgRam = AddColorRow(_tabColors, "RAM Screen:", rightCol, y, labelWidth, panelSize, Color.Black, "SET_CLR_BG_NORM:2");
            y += spacing;
            _panelBgNet = AddColorRow(_tabColors, "NET Screen:", rightCol, y, labelWidth, panelSize, Color.Black, "SET_CLR_BG_NORM:3");
            y += spacing + 15;

            // === Screensaver Backgrounds (Desert-Spec Mode) ===
            AddSectionLabel(_tabColors, "Screensaver Backgrounds", rightCol, y);
            y += 28;

            // Default colors from ESP32 gui_settings.h
            _panelSsBgCpu = AddScreensaverColorRow(_tabColors, "CPU (Sonic):", rightCol, y, labelWidth, panelSize,
                Color.FromArgb(0x00, 0x00, 0x8B), 0);  // Dark Blue
            y += spacing;
            _panelSsBgGpu = AddScreensaverColorRow(_tabColors, "GPU (Triforce):", rightCol, y, labelWidth, panelSize,
                Color.FromArgb(0x8B, 0x00, 0x00), 1);  // Dark Red
            y += spacing;
            _panelSsBgRam = AddScreensaverColorRow(_tabColors, "RAM (DK):", rightCol, y, labelWidth, panelSize,
                Color.FromArgb(0x5D, 0x40, 0x37), 2);  // Dark Brown
            y += spacing;
            _panelSsBgNet = AddScreensaverColorRow(_tabColors, "NET (PacMan):", rightCol, y, labelWidth, panelSize,
                Color.Black, 3);  // Black
        }

        private void AddSectionLabel(Control parent, string text, int x, int y)
        {
            var lbl = new Label
            {
                Text = text,
                Location = new Point(x, y),
                AutoSize = true,
                ForeColor = ThemeAccent,
                Font = new Font("Segoe UI", 10F, FontStyle.Bold)
            };
            parent.Controls.Add(lbl);
        }

        private Panel AddColorRow(Control parent, string label, int x, int y, int labelWidth, int panelSize, Color defaultColor, string command)
        {
            var lbl = new Label
            {
                Text = label,
                Location = new Point(x, y + 6),
                Size = new Size(labelWidth, 20),
                ForeColor = ThemeTextSecondary
            };
            parent.Controls.Add(lbl);

            var panel = new Panel
            {
                Location = new Point(x + labelWidth, y),
                Size = new Size(panelSize, panelSize),
                BackColor = defaultColor,
                Cursor = Cursors.Hand,
                Tag = command
            };

            panel.Paint += (s, e) =>
            {
                using (var pen = new Pen(ThemeBorder, 2))
                {
                    e.Graphics.DrawRectangle(pen, 0, 0, panel.Width - 1, panel.Height - 1);
                }
            };
            panel.Click += ColorPanel_Click;
            parent.Controls.Add(panel);

            return panel;
        }

        private void ColorPanel_Click(object sender, EventArgs e)
        {
            var panel = sender as Panel;
            if (panel == null) return;

            using (var dlg = new ColorDialog())
            {
                dlg.Color = panel.BackColor;
                dlg.FullOpen = true;

                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    panel.BackColor = dlg.Color;

                    string cmd = panel.Tag as string;
                    if (!string.IsNullOrEmpty(cmd))
                    {
                        SendCommandSafe($"{cmd}:{ColorToHex(dlg.Color)}");
                    }
                }
            }
        }

        /// <summary>
        /// Adds a color row for screensaver background with immediate send on selection.
        /// Uses SET_SS_BG=slot,hexcode protocol.
        /// </summary>
        private Panel AddScreensaverColorRow(Control parent, string label, int x, int y,
            int labelWidth, int panelSize, Color defaultColor, int slotIndex)
        {
            var lbl = new Label
            {
                Text = label,
                Location = new Point(x, y + 6),
                Size = new Size(labelWidth, 20),
                ForeColor = ThemeTextSecondary
            };
            parent.Controls.Add(lbl);

            var panel = new Panel
            {
                Location = new Point(x + labelWidth, y),
                Size = new Size(panelSize, panelSize),
                BackColor = defaultColor,
                Cursor = Cursors.Hand,
                Tag = slotIndex  // Store slot index for command
            };

            panel.Paint += (s, e) =>
            {
                using (var pen = new Pen(ThemeBorder, 2))
                {
                    e.Graphics.DrawRectangle(pen, 0, 0, panel.Width - 1, panel.Height - 1);
                }
            };
            panel.Click += ScreensaverBgPanel_Click;
            parent.Controls.Add(panel);

            return panel;
        }

        /// <summary>
        /// Handles screensaver background color selection.
        /// Sends SET_SS_BG=slot,hexcode command immediately.
        /// </summary>
        private void ScreensaverBgPanel_Click(object sender, EventArgs e)
        {
            var panel = sender as Panel;
            if (panel == null) return;

            int slotIndex = (int)panel.Tag;

            using (var dlg = new ColorDialog())
            {
                dlg.Color = panel.BackColor;
                dlg.FullOpen = true;

                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    panel.BackColor = dlg.Color;

                    // Send command immediately: SET_SS_BG=slot,hexcode
                    string hex = ColorToHex(dlg.Color);
                    SendCommandSafe($"SET_SS_BG={slotIndex},{hex}");

                    AppendDebugLog($"Sent screensaver BG color for slot {slotIndex}: #{hex}");
                }
            }
        }

        private string ColorToHex(Color c)
        {
            return $"{c.R:X2}{c.G:X2}{c.B:X2}";
        }

        #endregion

        #region Profiles Tab

        private void CreateProfilesTab()
        {
            int y = 15;

            AddSectionLabel(_tabProfiles, "Theme Profiles", 15, y);
            y += 30;

            // Profile list
            _lstProfiles = new ListBox
            {
                Location = new Point(15, y),
                Size = new Size(250, 200),
                BackColor = ThemeBgDark,
                ForeColor = ThemeTextPrimary,
                BorderStyle = BorderStyle.FixedSingle
            };
            _lstProfiles.SelectedIndexChanged += LstProfiles_SelectedIndexChanged;
            _tabProfiles.Controls.Add(_lstProfiles);

            // Profile actions
            int btnX = 280;
            var btnLoad = CreateStyledButton("Load Profile", btnX, y, 130, 30);
            btnLoad.Click += BtnLoadProfile_Click;
            _tabProfiles.Controls.Add(btnLoad);

            var btnDelete = CreateStyledButton("Delete Profile", btnX, y + 40, 130, 30);
            btnDelete.Click += BtnDeleteProfile_Click;
            _tabProfiles.Controls.Add(btnDelete);

            y += 90;

            // Save new profile
            AddSectionLabel(_tabProfiles, "Save Current Theme", btnX, y);
            y += 28;

            _txtProfileName = new TextBox
            {
                Location = new Point(btnX, y),
                Size = new Size(130, 25),
                BackColor = ThemeBgDark,
                ForeColor = ThemeTextPrimary,
                BorderStyle = BorderStyle.FixedSingle,
                MaxLength = 30
            };
            _tabProfiles.Controls.Add(_txtProfileName);

            var btnSave = CreateStyledButton("Save", btnX + 140, y, 70, 25);
            btnSave.Click += BtnSaveProfile_Click;
            _tabProfiles.Controls.Add(btnSave);

            // Right side - Profile info
            int rightX = 450;
            y = 15;

            AddSectionLabel(_tabProfiles, "Profile Information", rightX, y);
            y += 30;

            var lblInfo = new Label
            {
                Text = "Profiles save all theme colors and can be\n" +
                       "quickly applied to any connected Scarab device.\n\n" +
                       "When a new device is detected, you'll be\n" +
                       "prompted to apply a saved profile.\n\n" +
                       "Profile storage location:\n" +
                       ProfilesDir,
                Location = new Point(rightX, y),
                Size = new Size(350, 150),
                ForeColor = ThemeTextSecondary
            };
            _tabProfiles.Controls.Add(lblInfo);

            // Load profile list
            RefreshProfileList();
        }

        private void RefreshProfileList()
        {
            _lstProfiles.Items.Clear();
            if (Directory.Exists(ProfilesDir))
            {
                var profiles = Directory.GetFiles(ProfilesDir, "*.profile")
                    .Select(Path.GetFileNameWithoutExtension)
                    .OrderBy(n => n);

                foreach (var profile in profiles)
                {
                    _lstProfiles.Items.Add(profile);
                }
            }
        }

        private void LstProfiles_SelectedIndexChanged(object sender, EventArgs e)
        {
            // Could show profile preview here
        }

        private void BtnSaveProfile_Click(object sender, EventArgs e)
        {
            string name = _txtProfileName.Text.Trim();
            if (string.IsNullOrEmpty(name))
            {
                MessageBox.Show("Please enter a profile name.", "Invalid Name",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            // Sanitize filename
            foreach (char c in Path.GetInvalidFileNameChars())
            {
                name = name.Replace(c, '_');
            }

            var profile = new ThemeProfile
            {
                Name = name,
                CpuArcColor = _panelCpuArc?.BackColor ?? _cpuArcColor,
                GpuArcColor = _panelGpuArc?.BackColor ?? _gpuArcColor,
                ArcBgColor = _panelArcBg?.BackColor ?? _arcBgColor,
                RamBarColor = _panelRamBar?.BackColor ?? _ramBarColor,
                NetDownColor = _panelNetDown?.BackColor ?? Color.FromArgb(0x00, 0xE6, 0x76),
                NetUpColor = _panelNetUp?.BackColor ?? Color.FromArgb(0xFF, 0x6B, 0x6B),
                BgCpuColor = _panelBgCpu?.BackColor ?? Color.Black,
                BgGpuColor = _panelBgGpu?.BackColor ?? Color.Black,
                BgRamColor = _panelBgRam?.BackColor ?? Color.Black,
                BgNetColor = _panelBgNet?.BackColor ?? Color.Black
            };

            string path = Path.Combine(ProfilesDir, name + ".profile");
            profile.Save(path);

            RefreshProfileList();
            _txtProfileName.Text = "";

            MessageBox.Show($"Profile '{name}' saved!", "Success",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void BtnLoadProfile_Click(object sender, EventArgs e)
        {
            if (_lstProfiles.SelectedItem == null)
            {
                MessageBox.Show("Please select a profile.", "No Selection",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            string name = _lstProfiles.SelectedItem.ToString();
            LoadProfile(name);
        }

        private void LoadProfile(string name)
        {
            string path = Path.Combine(ProfilesDir, name + ".profile");
            if (!File.Exists(path))
            {
                MessageBox.Show("Profile not found.", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            var profile = ThemeProfile.Load(path);
            if (profile == null) return;

            // Apply to UI
            if (_panelCpuArc != null) _panelCpuArc.BackColor = profile.CpuArcColor;
            if (_panelGpuArc != null) _panelGpuArc.BackColor = profile.GpuArcColor;
            if (_panelArcBg != null) _panelArcBg.BackColor = profile.ArcBgColor;
            if (_panelRamBar != null) _panelRamBar.BackColor = profile.RamBarColor;
            if (_panelNetDown != null) _panelNetDown.BackColor = profile.NetDownColor;
            if (_panelNetUp != null) _panelNetUp.BackColor = profile.NetUpColor;
            if (_panelBgCpu != null) _panelBgCpu.BackColor = profile.BgCpuColor;
            if (_panelBgGpu != null) _panelBgGpu.BackColor = profile.BgGpuColor;
            if (_panelBgRam != null) _panelBgRam.BackColor = profile.BgRamColor;
            if (_panelBgNet != null) _panelBgNet.BackColor = profile.BgNetColor;

            // Update live preview
            UpdatePreviewColors(profile.CpuArcColor, profile.GpuArcColor, profile.RamBarColor, Color.FromArgb(0xC8, 0x96, 0x32));

            // Send all colors to ESP if connected
            if (IsConnected?.Invoke() ?? false)
            {
                SendCommandSafe($"SET_CLR_ARC_CPU:{ColorToHex(profile.CpuArcColor)}");
                SendCommandSafe($"SET_CLR_ARC_GPU:{ColorToHex(profile.GpuArcColor)}");
                SendCommandSafe($"SET_CLR_ARC_BG:{ColorToHex(profile.ArcBgColor)}");
                SendCommandSafe($"SET_CLR_BAR_RAM:{ColorToHex(profile.RamBarColor)}");
                SendCommandSafe($"SET_CLR_NET_DN:{ColorToHex(profile.NetDownColor)}");
                SendCommandSafe($"SET_CLR_NET_UP:{ColorToHex(profile.NetUpColor)}");
                SendCommandSafe($"SET_CLR_BG_NORM:0:{ColorToHex(profile.BgCpuColor)}");
                SendCommandSafe($"SET_CLR_BG_NORM:1:{ColorToHex(profile.BgGpuColor)}");
                SendCommandSafe($"SET_CLR_BG_NORM:2:{ColorToHex(profile.BgRamColor)}");
                SendCommandSafe($"SET_CLR_BG_NORM:3:{ColorToHex(profile.BgNetColor)}");
            }

            MessageBox.Show($"Profile '{name}' loaded!", "Success",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void BtnDeleteProfile_Click(object sender, EventArgs e)
        {
            if (_lstProfiles.SelectedItem == null)
            {
                MessageBox.Show("Please select a profile.", "No Selection",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            string name = _lstProfiles.SelectedItem.ToString();
            var result = MessageBox.Show($"Delete profile '{name}'?", "Confirm Delete",
                MessageBoxButtons.YesNo, MessageBoxIcon.Question);

            if (result == DialogResult.Yes)
            {
                string path = Path.Combine(ProfilesDir, name + ".profile");
                try
                {
                    File.Delete(path);
                    RefreshProfileList();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Failed to delete: {ex.Message}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private bool IsNewDevice(string deviceId)
        {
            string knownDevicesPath = Path.Combine(ProfilesDir, "known_devices.txt");
            if (!File.Exists(knownDevicesPath))
                return true;

            var knownIds = File.ReadAllLines(knownDevicesPath);
            return !knownIds.Contains(deviceId);
        }

        private void MarkDeviceKnown(string deviceId)
        {
            string knownDevicesPath = Path.Combine(ProfilesDir, "known_devices.txt");
            File.AppendAllText(knownDevicesPath, deviceId + Environment.NewLine);
        }

        private void PromptLoadProfile(string deviceId)
        {
            if (!Directory.Exists(ProfilesDir)) return;

            var profiles = Directory.GetFiles(ProfilesDir, "*.profile");
            if (profiles.Length == 0) return;

            var result = MessageBox.Show(
                "New device detected! Would you like to load a saved profile?",
                "New Device", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

            if (result == DialogResult.Yes)
            {
                // Show profile selection
                _tabControl.SelectedTab = _tabProfiles;
                this.Show();
                this.Activate();
            }

            MarkDeviceKnown(deviceId);
        }

        private void EnsureProfilesDirectory()
        {
            if (!Directory.Exists(ProfilesDir))
            {
                Directory.CreateDirectory(ProfilesDir);
            }
        }

        #endregion

        #region Image Upload

        private async Task UploadImageAsync(string filePath, ImageSlot slot)
        {
            // Immediate debug output
            AppendDebugLog($"UploadImageAsync called: file={Path.GetFileName(filePath)}, slot={slot}");

            if (GetSerialPort == null)
            {
                AppendDebugLog("ERROR: GetSerialPort delegate is null");
                UpdateUploadStatus("Error: No serial port", false);
                return;
            }

            var port = GetSerialPort();
            if (port == null || !port.IsOpen)
            {
                AppendDebugLog($"ERROR: Port is null={port == null} or not open");
                UpdateUploadStatus("Error: Port not open", false);
                return;
            }

            AppendDebugLog($"Port {port.PortName} is open, starting upload process...");

            _isUploading = true;
            _uploadCts = new CancellationTokenSource();
            SetUploadMode?.Invoke(true);

            // Wait for data loop to pause (avoid race condition with PC stats transmission)
            await Task.Delay(200);

            try
            {
                var uploader = new ImageUploader(port);
                AppendDebugLog("ImageUploader instance created, subscribing to events...");

                // Subscribe to LogMessage for debug output
                uploader.LogMessage += (s, msg) => AppendDebugLog($"[Upload] {msg}");

                uploader.ProgressChanged += (s, p) =>
                {
                    BeginInvoke((MethodInvoker)delegate
                    {
                        _progressUpload.Value = (int)p.PercentComplete;
                        _lblUploadStatus.Text = $"Uploading {GetSlotName((int)slot)}: {p.BytesSent:N0}/{p.TotalBytes:N0} ({p.PercentComplete:F0}%)";
                    });
                };

                AppendDebugLog($"=== Starting upload: {Path.GetFileName(filePath)} -> Slot {slot} ===");
                UpdateUploadStatus($"Converting image for {GetSlotName((int)slot)}...", true);
                _progressUpload.Value = 0;

                bool success = await uploader.UploadImageAsync(filePath, slot, _uploadCts.Token);

                AppendDebugLog(success ? "=== Upload completed successfully ===" : "=== Upload FAILED ===");
                UpdateUploadStatus(success ? "Upload complete!" : "Upload failed!", success);
            }
            catch (Exception ex)
            {
                AppendDebugLog($"=== EXCEPTION: {ex.Message} ===");
                UpdateUploadStatus($"Error: {ex.Message}", false);
            }
            finally
            {
                _isUploading = false;
                SetUploadMode?.Invoke(false);

                await Task.Delay(3000);
                if (!_isUploading)
                {
                    UpdateUploadStatus("Drop images on displays to upload", true);
                    _progressUpload.Value = 0;
                }
            }
        }

        private void UpdateUploadStatus(string message, bool success)
        {
            if (InvokeRequired)
            {
                BeginInvoke((MethodInvoker)delegate { UpdateUploadStatus(message, success); });
                return;
            }

            _lblUploadStatus.Text = message;
            _lblUploadStatus.ForeColor = success ? ThemeTextSecondary : ThemeError;
        }

        #endregion

        #region Helpers

        private Button CreateStyledButton(string text, int x, int y, int width, int height)
        {
            var btn = new Button
            {
                Text = text,
                Location = new Point(x, y),
                Size = new Size(width, height),
                BackColor = ThemeBgLight,
                ForeColor = ThemeTextPrimary,
                FlatStyle = FlatStyle.Flat,
                Cursor = Cursors.Hand
            };
            btn.FlatAppearance.BorderColor = ThemeBorder;
            btn.FlatAppearance.MouseOverBackColor = ThemeAccentDim;
            return btn;
        }

        private void SendCommandSafe(string command)
        {
            if (SendCommand == null || !(IsConnected?.Invoke() ?? false))
                return;

            try
            {
                SendCommand(command);
            }
            catch { }
        }

        #endregion

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (e.CloseReason == CloseReason.UserClosing)
            {
                _uploadCts?.Cancel();
                e.Cancel = true;
                Hide();
            }
            else
            {
                base.OnFormClosing(e);
            }
        }
    }

    /// <summary>
    /// Theme profile for saving/loading color configurations.
    /// </summary>
    public class ThemeProfile
    {
        public string Name { get; set; }
        public Color CpuArcColor { get; set; }
        public Color GpuArcColor { get; set; }
        public Color ArcBgColor { get; set; }
        public Color RamBarColor { get; set; }
        public Color NetDownColor { get; set; }
        public Color NetUpColor { get; set; }
        public Color BgCpuColor { get; set; }
        public Color BgGpuColor { get; set; }
        public Color BgRamColor { get; set; }
        public Color BgNetColor { get; set; }

        public void Save(string path)
        {
            var lines = new[]
            {
                $"Name={Name}",
                $"CpuArc={CpuArcColor.ToArgb()}",
                $"GpuArc={GpuArcColor.ToArgb()}",
                $"ArcBg={ArcBgColor.ToArgb()}",
                $"RamBar={RamBarColor.ToArgb()}",
                $"NetDown={NetDownColor.ToArgb()}",
                $"NetUp={NetUpColor.ToArgb()}",
                $"BgCpu={BgCpuColor.ToArgb()}",
                $"BgGpu={BgGpuColor.ToArgb()}",
                $"BgRam={BgRamColor.ToArgb()}",
                $"BgNet={BgNetColor.ToArgb()}"
            };
            File.WriteAllLines(path, lines);
        }

        public static ThemeProfile Load(string path)
        {
            try
            {
                var lines = File.ReadAllLines(path);
                var dict = lines
                    .Where(l => l.Contains("="))
                    .ToDictionary(
                        l => l.Split('=')[0],
                        l => l.Split('=')[1]);

                return new ThemeProfile
                {
                    Name = dict.GetValueOrDefault("Name", "Unknown"),
                    CpuArcColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("CpuArc", "-16750592"))),
                    GpuArcColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("GpuArc", "-9066240"))),
                    ArcBgColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("ArcBg", "-11184804"))),
                    RamBarColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("RamBar", "-12325509"))),
                    NetDownColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("NetDown", "-16720266"))),
                    NetUpColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("NetUp", "-38293"))),
                    BgCpuColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("BgCpu", "-16777216"))),
                    BgGpuColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("BgGpu", "-16777216"))),
                    BgRamColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("BgRam", "-16777216"))),
                    BgNetColor = Color.FromArgb(int.Parse(dict.GetValueOrDefault("BgNet", "-16777216")))
                };
            }
            catch
            {
                return null;
            }
        }
    }

    internal static class DictionaryExtensions
    {
        public static TValue GetValueOrDefault<TKey, TValue>(this Dictionary<TKey, TValue> dict, TKey key, TValue defaultValue)
        {
            return dict.TryGetValue(key, out TValue value) ? value : defaultValue;
        }
    }

    /// <summary>
    /// Custom circular panel for displaying round display previews with drag-and-drop support.
    /// Shows the GC9A01 circular mask to indicate visible area.
    /// </summary>
    public class CircularDisplayPanel : Control
    {
        private Image _image;
        private string _imagePath;
        private bool _isDragOver;

        public Color AccentColor { get; set; } = Color.FromArgb(0, 200, 120);
        public int SlotIndex { get; set; }
        public bool HasImage => _image != null;
        public string ImagePath => _imagePath;
        public bool ShowMask { get; set; } = true;

        public event EventHandler<string> ImageDropped;

        public CircularDisplayPanel()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            AllowDrop = true;
            Cursor = Cursors.Hand;

            DragEnter += OnDragEnter;
            DragLeave += OnDragLeave;
            DragDrop += OnDragDrop;
        }

        public void LoadImage(string path)
        {
            try
            {
                _image?.Dispose();
                _image = Image.FromFile(path);
                _imagePath = path;
                Invalidate();
            }
            catch
            {
                _image = null;
                _imagePath = null;
                throw;
            }
        }

        public void ClearImage()
        {
            _image?.Dispose();
            _image = null;
            _imagePath = null;
            Invalidate();
        }

        private void OnDragEnter(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                var files = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (files.Length > 0 && IsImageFile(files[0]))
                {
                    e.Effect = DragDropEffects.Copy;
                    _isDragOver = true;
                    Invalidate();
                    return;
                }
            }
            e.Effect = DragDropEffects.None;
        }

        private void OnDragLeave(object sender, EventArgs e)
        {
            _isDragOver = false;
            Invalidate();
        }

        private void OnDragDrop(object sender, DragEventArgs e)
        {
            _isDragOver = false;
            Invalidate();

            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                var files = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (files.Length > 0 && IsImageFile(files[0]))
                {
                    ImageDropped?.Invoke(this, files[0]);
                }
            }
        }

        private bool IsImageFile(string path)
        {
            string ext = Path.GetExtension(path).ToLowerInvariant();
            return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                   ext == ".gif" || ext == ".bmp" || ext == ".webp";
        }

        /// <summary>
        /// WYSIWYG Preview: Simulates how the image will appear on the 240x240 GC9A01 display.
        /// - Images smaller than 240px are NOT scaled up (appear small and centered)
        /// - Images larger than 240px are scaled down (aspect fit)
        /// - Black background fills uncovered circular area (like ImageConverter does)
        /// </summary>
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;

            // Physical display resolution (GC9A01)
            const float DISPLAY_RESOLUTION = 240f;

            int panelSize = Math.Min(Width, Height);
            int x = (Width - panelSize) / 2;
            int y = (Height - panelSize) / 2;
            var rect = new Rectangle(x, y, panelSize - 1, panelSize - 1);

            // UI scale factor: how panel pixels map to display pixels
            // e.g., 160px panel / 240px display = 0.667
            float uiScale = (panelSize - 8) / DISPLAY_RESOLUTION;  // -8 for inner margin

            // Draw outer frame (represents physical display bezel)
            using (var frameBrush = new SolidBrush(Color.FromArgb(8, 8, 10)))
            {
                g.FillEllipse(frameBrush, rect);
            }

            // Inner circular area (visible display area - 240x240 logical)
            int innerMargin = 4;
            var innerRect = new Rectangle(x + innerMargin, y + innerMargin,
                                          panelSize - innerMargin * 2 - 1, panelSize - innerMargin * 2 - 1);

            using (var path = new GraphicsPath())
            {
                path.AddEllipse(innerRect);
                g.SetClip(path);

                // Black background (simulates ImageConverter's black fill)
                using (var bgBrush = new SolidBrush(Color.Black))
                {
                    g.FillEllipse(bgBrush, innerRect);
                }

                // Image if loaded - WYSIWYG logic
                if (_image != null)
                {
                    int imgW, imgH;

                    // Determine logical size on 240x240 display
                    if (_image.Width <= DISPLAY_RESOLUTION && _image.Height <= DISPLAY_RESOLUTION)
                    {
                        // Image fits within 240x240 - use original size (no upscaling!)
                        // This ensures a 50x50 icon looks small and centered
                        imgW = (int)(_image.Width * uiScale);
                        imgH = (int)(_image.Height * uiScale);
                    }
                    else
                    {
                        // Image larger than 240x240 - scale down to fit (aspect fit)
                        float scaleToFit = Math.Min(DISPLAY_RESOLUTION / _image.Width,
                                                     DISPLAY_RESOLUTION / _image.Height);
                        float logicalW = _image.Width * scaleToFit;
                        float logicalH = _image.Height * scaleToFit;

                        // Convert logical display pixels to UI pixels
                        imgW = (int)(logicalW * uiScale);
                        imgH = (int)(logicalH * uiScale);
                    }

                    // Center the image within the inner circular area
                    int innerSize = panelSize - innerMargin * 2;
                    int imgX = x + innerMargin + (innerSize - imgW) / 2;
                    int imgY = y + innerMargin + (innerSize - imgH) / 2;

                    g.DrawImage(_image, imgX, imgY, imgW, imgH);
                }
                else
                {
                    // Draw placeholder text
                    using (var brush = new SolidBrush(Color.FromArgb(60, 60, 70)))
                    {
                        var sf = new StringFormat
                        {
                            Alignment = StringAlignment.Center,
                            LineAlignment = StringAlignment.Center
                        };
                        g.DrawString("Click to\nSelect Image", new Font("Segoe UI", 8F), brush, innerRect, sf);
                    }
                }

                g.ResetClip();
            }

            // Circular mask indicator (shows 240x240 boundary)
            if (ShowMask)
            {
                using (var maskPen = new Pen(Color.FromArgb(80, AccentColor), 2))
                {
                    maskPen.DashStyle = DashStyle.Dot;
                    g.DrawEllipse(maskPen, innerRect);
                }
            }

            // Outer ring (highlight on drag)
            int ringWidth = _isDragOver ? 3 : 2;
            using (var pen = new Pen(_isDragOver ? AccentColor : Color.FromArgb(45, 45, 55), ringWidth))
            {
                var ringRect = new Rectangle(x + 1, y + 1, panelSize - 3, panelSize - 3);
                g.DrawEllipse(pen, ringRect);
            }

            // Drag over glow effect
            if (_isDragOver)
            {
                using (var glowPen = new Pen(Color.FromArgb(40, AccentColor), 6))
                {
                    var glowRect = new Rectangle(x + 3, y + 3, panelSize - 7, panelSize - 7);
                    g.DrawEllipse(glowPen, glowRect);
                }
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _image?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
