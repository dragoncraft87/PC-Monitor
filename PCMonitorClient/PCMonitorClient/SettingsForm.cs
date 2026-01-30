using System;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PCMonitorClient
{
    /// <summary>
    /// Settings form for configuring ESP32 theme colors and uploading custom images.
    /// </summary>
    public class SettingsForm : Form
    {
        // Callback to send commands to ESP
        public Action<string> SendCommand { get; set; }

        // Callback to check if connected
        public Func<bool> IsConnected { get; set; }

        // Callback to get SerialPort for image upload
        public Func<System.IO.Ports.SerialPort> GetSerialPort { get; set; }

        // Callback to pause/resume data loop during upload
        public Action<bool> SetUploadMode { get; set; }

        private TabControl _tabControl;
        private TabPage _tabColors;
        private TabPage _tabImages;

        // Color controls
        private Panel _panelCpuArc, _panelGpuArc, _panelArcBg;
        private Panel _panelRamBar, _panelNetDown, _panelNetUp;
        private Panel _panelBgCpu, _panelBgGpu, _panelBgRam, _panelBgNet;
        private Panel _panelSsBgCpu, _panelSsBgGpu, _panelSsBgRam, _panelSsBgNet;

        // Image upload controls
        private Button _btnUploadCpu, _btnUploadGpu, _btnUploadRam, _btnUploadNet;
        private ProgressBar _progressUpload;
        private Label _lblUploadStatus;
        private PictureBox[] _previewBoxes;

        // Upload state
        private CancellationTokenSource _uploadCts;
        private bool _isUploading;

        public SettingsForm()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            Text = "Scarab Monitor Settings";
            Size = new Size(600, 500);
            StartPosition = FormStartPosition.CenterScreen;
            FormBorderStyle = FormBorderStyle.FixedToolWindow;
            BackColor = Color.FromArgb(30, 30, 30);
            ForeColor = Color.White;
            Font = new Font("Segoe UI", 9F);

            _tabControl = new TabControl
            {
                Location = new Point(10, 10),
                Size = new Size(565, 440),
                Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right
            };

            // === Colors Tab ===
            _tabColors = new TabPage("Theme Colors")
            {
                BackColor = Color.FromArgb(35, 35, 35)
            };
            CreateColorsTab();
            _tabControl.TabPages.Add(_tabColors);

            // === Images Tab ===
            _tabImages = new TabPage("Screensaver Images")
            {
                BackColor = Color.FromArgb(35, 35, 35)
            };
            CreateImagesTab();
            _tabControl.TabPages.Add(_tabImages);

            Controls.Add(_tabControl);
        }

        #region Colors Tab

        private void CreateColorsTab()
        {
            int y = 15;
            int labelWidth = 120;
            int panelSize = 30;
            int spacing = 40;

            // Group: Arc Colors
            AddLabel(_tabColors, "Arc Colors:", 10, y, true);
            y += 25;

            _panelCpuArc = AddColorRow(_tabColors, "CPU Arc:", 10, y, labelWidth, panelSize, Color.FromArgb(0x00, 0x71, 0xC5), "SET_CLR_ARC_CPU");
            y += spacing;

            _panelGpuArc = AddColorRow(_tabColors, "GPU Arc:", 10, y, labelWidth, panelSize, Color.FromArgb(0x76, 0xB9, 0x00), "SET_CLR_ARC_GPU");
            y += spacing;

            _panelArcBg = AddColorRow(_tabColors, "Arc Background:", 10, y, labelWidth, panelSize, Color.FromArgb(0x55, 0x55, 0x5C), "SET_CLR_ARC_BG");
            y += spacing + 10;

            // Group: Bar/Chart Colors
            AddLabel(_tabColors, "Bar/Chart Colors:", 10, y, true);
            y += 25;

            _panelRamBar = AddColorRow(_tabColors, "RAM Bar:", 10, y, labelWidth, panelSize, Color.FromArgb(0x43, 0xE9, 0x7B), "SET_CLR_BAR_RAM");
            y += spacing;

            _panelNetDown = AddColorRow(_tabColors, "Net Download:", 10, y, labelWidth, panelSize, Color.FromArgb(0x00, 0xE6, 0x76), "SET_CLR_NET_DN");
            y += spacing;

            _panelNetUp = AddColorRow(_tabColors, "Net Upload:", 10, y, labelWidth, panelSize, Color.FromArgb(0xFF, 0x6B, 0x6B), "SET_CLR_NET_UP");
            y += spacing + 10;

            // Group: Background Colors (right column)
            int rightCol = 290;
            y = 15;

            AddLabel(_tabColors, "Screen Backgrounds:", rightCol, y, true);
            y += 25;

            _panelBgCpu = AddColorRow(_tabColors, "CPU Screen:", rightCol, y, labelWidth, panelSize, Color.Black, "SET_CLR_BG_NORM:0");
            y += spacing;

            _panelBgGpu = AddColorRow(_tabColors, "GPU Screen:", rightCol, y, labelWidth, panelSize, Color.Black, "SET_CLR_BG_NORM:1");
            y += spacing;

            _panelBgRam = AddColorRow(_tabColors, "RAM Screen:", rightCol, y, labelWidth, panelSize, Color.Black, "SET_CLR_BG_NORM:2");
            y += spacing;

            _panelBgNet = AddColorRow(_tabColors, "NET Screen:", rightCol, y, labelWidth, panelSize, Color.Black, "SET_CLR_BG_NORM:3");
            y += spacing + 10;

            // Screensaver backgrounds
            AddLabel(_tabColors, "Screensaver BG:", rightCol, y, true);
            y += 25;

            _panelSsBgCpu = AddColorRow(_tabColors, "CPU SS:", rightCol, y, labelWidth, panelSize, Color.FromArgb(0x11, 0x00, 0x00), "SET_CLR_BG_SS:0");
            y += spacing;

            _panelSsBgGpu = AddColorRow(_tabColors, "GPU SS:", rightCol, y, labelWidth, panelSize, Color.FromArgb(0x1A, 0x1A, 0x00), "SET_CLR_BG_SS:1");
            y += spacing;

            _panelSsBgRam = AddColorRow(_tabColors, "RAM SS:", rightCol, y, labelWidth, panelSize, Color.FromArgb(0x00, 0x11, 0x00), "SET_CLR_BG_SS:2");
            y += spacing;

            _panelSsBgNet = AddColorRow(_tabColors, "NET SS:", rightCol, y, labelWidth, panelSize, Color.FromArgb(0x00, 0x00, 0x11), "SET_CLR_BG_SS:3");
            y += spacing + 20;

            // Reset button
            var btnReset = new Button
            {
                Text = "Reset to Defaults",
                Location = new Point(rightCol, y),
                Size = new Size(130, 30),
                BackColor = Color.FromArgb(60, 60, 60),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat
            };
            btnReset.Click += (s, e) => SendCommandSafe("RESET_THEME");
            _tabColors.Controls.Add(btnReset);
        }

        private void AddLabel(Control parent, string text, int x, int y, bool bold = false)
        {
            var lbl = new Label
            {
                Text = text,
                Location = new Point(x, y),
                AutoSize = true,
                ForeColor = bold ? Color.LimeGreen : Color.White
            };
            if (bold) lbl.Font = new Font(lbl.Font, FontStyle.Bold);
            parent.Controls.Add(lbl);
        }

        private Panel AddColorRow(Control parent, string label, int x, int y, int labelWidth, int panelSize, Color defaultColor, string command)
        {
            var lbl = new Label
            {
                Text = label,
                Location = new Point(x, y + 5),
                Size = new Size(labelWidth, 20),
                ForeColor = Color.LightGray
            };
            parent.Controls.Add(lbl);

            var panel = new Panel
            {
                Location = new Point(x + labelWidth, y),
                Size = new Size(panelSize, panelSize),
                BackColor = defaultColor,
                BorderStyle = BorderStyle.FixedSingle,
                Cursor = Cursors.Hand,
                Tag = command
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

                    // Send command to ESP
                    string cmd = panel.Tag as string;
                    if (!string.IsNullOrEmpty(cmd))
                    {
                        string hex = ColorToHex(dlg.Color);
                        SendCommandSafe($"{cmd}:{hex}");
                    }
                }
            }
        }

        private string ColorToHex(Color c)
        {
            return $"{c.R:X2}{c.G:X2}{c.B:X2}";
        }

        #endregion

        #region Images Tab

        private void CreateImagesTab()
        {
            int boxSize = 120;
            int spacing = 140;
            int startX = 20;
            int startY = 20;

            _previewBoxes = new PictureBox[4];
            string[] labels = { "CPU (Slot 0)", "GPU (Slot 1)", "RAM (Slot 2)", "NET (Slot 3)" };

            for (int i = 0; i < 4; i++)
            {
                int x = startX + (i % 2) * (spacing + boxSize);
                int y = startY + (i / 2) * (spacing + 50);

                // Label
                var lbl = new Label
                {
                    Text = labels[i],
                    Location = new Point(x, y),
                    AutoSize = true,
                    ForeColor = Color.LimeGreen
                };
                _tabImages.Controls.Add(lbl);

                // Preview box
                var box = new PictureBox
                {
                    Location = new Point(x, y + 20),
                    Size = new Size(boxSize, boxSize),
                    BackColor = Color.FromArgb(20, 20, 20),
                    BorderStyle = BorderStyle.FixedSingle,
                    SizeMode = PictureBoxSizeMode.Zoom
                };
                _previewBoxes[i] = box;
                _tabImages.Controls.Add(box);

                // Upload button
                var btn = new Button
                {
                    Text = "Upload...",
                    Location = new Point(x, y + 25 + boxSize),
                    Size = new Size(boxSize, 25),
                    BackColor = Color.FromArgb(60, 60, 60),
                    ForeColor = Color.White,
                    FlatStyle = FlatStyle.Flat,
                    Tag = i
                };
                btn.Click += BtnUpload_Click;
                _tabImages.Controls.Add(btn);

                // Store buttons
                switch (i)
                {
                    case 0: _btnUploadCpu = btn; break;
                    case 1: _btnUploadGpu = btn; break;
                    case 2: _btnUploadRam = btn; break;
                    case 3: _btnUploadNet = btn; break;
                }
            }

            // Progress bar
            _progressUpload = new ProgressBar
            {
                Location = new Point(startX, 350),
                Size = new Size(500, 20),
                Style = ProgressBarStyle.Continuous
            };
            _tabImages.Controls.Add(_progressUpload);

            // Status label
            _lblUploadStatus = new Label
            {
                Text = "Select an image to upload",
                Location = new Point(startX, 375),
                Size = new Size(500, 20),
                ForeColor = Color.Gray
            };
            _tabImages.Controls.Add(_lblUploadStatus);
        }

        private async void BtnUpload_Click(object sender, EventArgs e)
        {
            if (_isUploading)
            {
                MessageBox.Show("Upload already in progress!", "Busy", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (IsConnected == null || !IsConnected())
            {
                MessageBox.Show("Not connected to ESP32!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            var btn = sender as Button;
            int slot = (int)btn.Tag;

            using (var dlg = new OpenFileDialog())
            {
                dlg.Title = "Select Image for Slot " + slot;
                dlg.Filter = "Image Files|*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.webp|All Files|*.*";

                if (dlg.ShowDialog() != DialogResult.OK)
                    return;

                string filePath = dlg.FileName;

                // Load preview
                try
                {
                    _previewBoxes[slot].Image = Image.FromFile(filePath);
                }
                catch { }

                // Start upload
                await UploadImageAsync(filePath, (ImageSlot)slot);
            }
        }

        private async Task UploadImageAsync(string filePath, ImageSlot slot)
        {
            if (GetSerialPort == null)
            {
                _lblUploadStatus.Text = "Error: No serial port available";
                return;
            }

            var port = GetSerialPort();
            if (port == null || !port.IsOpen)
            {
                _lblUploadStatus.Text = "Error: Serial port not open";
                return;
            }

            _isUploading = true;
            _uploadCts = new CancellationTokenSource();

            // Pause data loop
            SetUploadMode?.Invoke(true);

            // Disable upload buttons
            SetUploadButtonsEnabled(false);

            try
            {
                var uploader = new ImageUploader(port);
                uploader.ProgressChanged += (s, p) =>
                {
                    BeginInvoke((MethodInvoker)delegate
                    {
                        _progressUpload.Value = (int)p.PercentComplete;
                        _lblUploadStatus.Text = $"{p.Status} - {p.BytesSent:N0} / {p.TotalBytes:N0} bytes ({p.PercentComplete:F1}%)";
                    });
                };

                uploader.LogMessage += (s, msg) =>
                {
                    Console.WriteLine("[Upload] " + msg);
                };

                _lblUploadStatus.Text = "Starting upload...";
                _progressUpload.Value = 0;

                bool success = await uploader.UploadImageAsync(filePath, slot, _uploadCts.Token);

                if (success)
                {
                    _lblUploadStatus.Text = "Upload complete!";
                    _lblUploadStatus.ForeColor = Color.LimeGreen;
                }
                else
                {
                    _lblUploadStatus.Text = "Upload failed!";
                    _lblUploadStatus.ForeColor = Color.Red;
                }
            }
            catch (Exception ex)
            {
                _lblUploadStatus.Text = "Error: " + ex.Message;
                _lblUploadStatus.ForeColor = Color.Red;
            }
            finally
            {
                _isUploading = false;

                // Resume data loop
                SetUploadMode?.Invoke(false);

                // Re-enable upload buttons
                SetUploadButtonsEnabled(true);

                // Reset status color after delay
                await Task.Delay(3000);
                if (!_isUploading)
                {
                    _lblUploadStatus.ForeColor = Color.Gray;
                }
            }
        }

        private void SetUploadButtonsEnabled(bool enabled)
        {
            _btnUploadCpu.Enabled = enabled;
            _btnUploadGpu.Enabled = enabled;
            _btnUploadRam.Enabled = enabled;
            _btnUploadNet.Enabled = enabled;
        }

        #endregion

        private void SendCommandSafe(string command)
        {
            if (SendCommand == null)
            {
                MessageBox.Show("Not connected!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (IsConnected != null && !IsConnected())
            {
                MessageBox.Show("Not connected to ESP32!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                SendCommand(command);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to send: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (e.CloseReason == CloseReason.UserClosing)
            {
                // Cancel any upload
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
}
