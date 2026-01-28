using System;
using System.Drawing;
using System.Windows.Forms;

namespace PCMonitorClient
{
    public class StatusForm : Form
    {
        private readonly TextBox _txtStatus;
        private readonly Label _lblConnection;

        public StatusForm()
        {
            // Form properties
            Text = "Scarab Monitor Status";
            Size = new Size(420, 340);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(30, 30, 30);

            // Connection status label
            _lblConnection = new Label
            {
                Text = "Status: Disconnected",
                Location = new Point(12, 12),
                Size = new Size(380, 24),
                ForeColor = Color.OrangeRed,
                Font = new Font("Segoe UI", 11f, FontStyle.Bold),
                BackColor = Color.Transparent
            };
            Controls.Add(_lblConnection);

            // Data display textbox
            _txtStatus = new TextBox
            {
                Location = new Point(12, 44),
                Size = new Size(380, 240),
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                BackColor = Color.FromArgb(20, 20, 20),
                ForeColor = Color.LimeGreen,
                Font = new Font("Consolas", 10f),
                BorderStyle = BorderStyle.FixedSingle
            };
            Controls.Add(_txtStatus);

            // Admin warning label
            if (!HardwareCollector.IsAdmin())
            {
                var lblAdmin = new Label
                {
                    Text = "Warning: Not running as Administrator",
                    Location = new Point(12, 290),
                    Size = new Size(380, 20),
                    ForeColor = Color.Yellow,
                    Font = new Font("Segoe UI", 9f),
                    BackColor = Color.Transparent
                };
                Controls.Add(lblAdmin);
            }
        }

        /// <summary>
        /// Update the connection status label (thread-safe).
        /// </summary>
        public void UpdateConnectionStatus(bool connected, string portName)
        {
            if (InvokeRequired)
            {
                Invoke(new Action(() => UpdateConnectionStatus(connected, portName)));
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

        /// <summary>
        /// Update the data display (thread-safe).
        /// </summary>
        public void UpdateData(string data)
        {
            if (InvokeRequired)
            {
                Invoke(new Action(() => UpdateData(data)));
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

        /// <summary>
        /// Append a log line (thread-safe).
        /// </summary>
        public void AppendLog(string line)
        {
            if (InvokeRequired)
            {
                Invoke(new Action(() => AppendLog(line)));
                return;
            }

            _txtStatus.AppendText(line + "\r\n");
            // Limit log size
            if (_txtStatus.Text.Length > 8000)
            {
                _txtStatus.Text = _txtStatus.Text.Substring(4000);
            }
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
