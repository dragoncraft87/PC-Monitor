using System;
using System.Drawing;
using System.Windows.Forms;

namespace PCMonitorClient
{
    public class StatusForm : Form
    {
        private readonly TextBox _txtStatus;
        private readonly Label _lblConnection;
        private readonly Label _lblMode;
        private readonly Label _lblAdminWarning;

        public StatusForm()
        {
            // Form properties
            Text = "Scarab Monitor Status";
            Size = new Size(440, 380);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(30, 30, 30);

            // Connection status label
            _lblConnection = new Label
            {
                Text = "Status: Disconnected",
                Location = new Point(12, 12),
                Size = new Size(400, 24),
                ForeColor = Color.OrangeRed,
                Font = new Font("Segoe UI", 11f, FontStyle.Bold),
                BackColor = Color.Transparent
            };
            Controls.Add(_lblConnection);

            // Mode indicator label
            _lblMode = new Label
            {
                Text = "MODE: INITIALIZING...",
                Location = new Point(12, 38),
                Size = new Size(400, 20),
                ForeColor = Color.Gray,
                Font = new Font("Segoe UI", 9f, FontStyle.Bold),
                BackColor = Color.Transparent
            };
            Controls.Add(_lblMode);

            // Data display textbox
            _txtStatus = new TextBox
            {
                Location = new Point(12, 64),
                Size = new Size(400, 240),
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                BackColor = Color.FromArgb(20, 20, 20),
                ForeColor = Color.LimeGreen,
                Font = new Font("Consolas", 10f),
                BorderStyle = BorderStyle.FixedSingle
            };
            Controls.Add(_txtStatus);

            // Admin warning label (shown only if not admin)
            _lblAdminWarning = new Label
            {
                Text = "",
                Location = new Point(12, 310),
                Size = new Size(400, 32),
                ForeColor = Color.Yellow,
                Font = new Font("Segoe UI", 9f),
                BackColor = Color.Transparent
            };
            Controls.Add(_lblAdminWarning);
        }

        /// <summary>
        /// Update the mode indicator (thread-safe, non-blocking).
        /// </summary>
        public void UpdateMode(bool isLiteMode, bool isAdmin)
        {
            if (IsDisposed) return;

            try
            {
                if (InvokeRequired)
                {
                    BeginInvoke(new Action(() => UpdateMode(isLiteMode, isAdmin)));
                    return;
                }

                if (isLiteMode)
                {
                    _lblMode.Text = "MODE: LITE (LIMITED SENSORS)";
                    _lblMode.ForeColor = Color.Orange;
                    _lblAdminWarning.Text = "Running without admin rights. CPU temp unavailable.\nRight-click tray icon -> 'Restart as Admin' for full access.";
                }
                else
                {
                    _lblMode.Text = "MODE: FULL (ALL SENSORS)";
                    _lblMode.ForeColor = Color.LimeGreen;
                    _lblAdminWarning.Text = "";
                }
            }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
        }

        /// <summary>
        /// Update the connection status label (thread-safe, non-blocking).
        /// </summary>
        public void UpdateConnectionStatus(bool connected, string portName)
        {
            if (IsDisposed) return;

            try
            {
                if (InvokeRequired)
                {
                    BeginInvoke(new Action(() => UpdateConnectionStatus(connected, portName)));
                    return;
                }

                if (connected)
                {
                    _lblConnection.Text = "Status: Connected (" + portName + ")";
                    _lblConnection.ForeColor = Color.LimeGreen;
                }
                else
                {
                    _lblConnection.Text = "Status: " + portName;
                    _lblConnection.ForeColor = Color.OrangeRed;
                }
            }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
        }

        /// <summary>
        /// Update the data display (thread-safe, non-blocking).
        /// </summary>
        public void UpdateData(string data)
        {
            if (IsDisposed) return;

            try
            {
                if (InvokeRequired)
                {
                    BeginInvoke(new Action(() => UpdateData(data)));
                    return;
                }

                // Format the data nicely
                string formatted = data
                    .Replace(",CPU", "\r\nCPU")
                    .Replace(",GPU", "\r\nGPU")
                    .Replace(",VRAM", "\r\nVRAM")
                    .Replace(",RAM", "\r\nRAM")
                    .Replace(",NET", "\r\nNET")
                    .Replace(",SPEED", "\r\nSPEED")
                    .Replace(",DOWN", "\r\nDOWN")
                    .Replace(",UP", "\r\nUP")
                    .Replace(":", ":  ");

                _txtStatus.Text = formatted;
            }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
        }

        /// <summary>
        /// Append a log line (thread-safe, non-blocking).
        /// </summary>
        public void AppendLog(string line)
        {
            if (IsDisposed) return;

            try
            {
                if (InvokeRequired)
                {
                    BeginInvoke(new Action(() => AppendLog(line)));
                    return;
                }

                _txtStatus.AppendText(line + "\r\n");
                // Limit log size
                if (_txtStatus.Text.Length > 8000)
                {
                    _txtStatus.Text = _txtStatus.Text.Substring(4000);
                }
            }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
        }

        /// <summary>
        /// Override closing to hide instead of close (keep app running in tray).
        /// </summary>
        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (e.CloseReason == CloseReason.UserClosing)
            {
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
