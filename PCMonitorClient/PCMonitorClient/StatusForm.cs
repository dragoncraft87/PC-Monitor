using System;
using System.Drawing;
using System.Windows.Forms;

namespace PCMonitorClient
{
    public class StatusForm : Form
    {
        private readonly Label _lblStatus;
        private readonly Label _lblMode;
        private readonly TextBox _txtLog;

        public StatusForm()
        {
            // Form properties
            Text = "Scarab Monitor Status";
            Size = new Size(500, 400);
            StartPosition = FormStartPosition.CenterScreen;
            FormBorderStyle = FormBorderStyle.SizableToolWindow;
            BackColor = Color.FromArgb(30, 30, 30);
            ForeColor = Color.White;
            Font = new Font("Consolas", 10F);

            // Mode Label (Oben Rechts)
            _lblMode = new Label
            {
                Location = new Point(350, 10),
                Size = new Size(130, 20),
                TextAlign = ContentAlignment.TopRight,
                Text = "INIT",
                ForeColor = Color.Gray,
                BackColor = Color.Transparent
            };
            Controls.Add(_lblMode);

            // Status Label (Oben Links)
            _lblStatus = new Label
            {
                Location = new Point(10, 10),
                Size = new Size(330, 20),
                Text = "Status: Initializing...",
                ForeColor = Color.Yellow,
                BackColor = Color.Transparent
            };
            Controls.Add(_lblStatus);

            // Log TextBox (Mitte)
            _txtLog = new TextBox
            {
                Location = new Point(10, 40),
                Size = new Size(465, 310),
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                BackColor = Color.FromArgb(20, 20, 20),
                ForeColor = Color.LimeGreen,
                BorderStyle = BorderStyle.FixedSingle,
                Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right
            };
            Controls.Add(_txtLog);
        }

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

        /// <summary>
        /// Thread-Safe Data Update (Fire & Forget).
        /// Only updates when visible for performance.
        /// </summary>
        public void UpdateData(string data)
        {
            // Performance: Skip if not visible
            if (!Visible) return;
            if (!IsHandleCreated || Disposing || IsDisposed) return;

            try
            {
                BeginInvoke((MethodInvoker)delegate
                {
                    // Clear buffer if too large
                    if (_txtLog.TextLength > 4000)
                    {
                        _txtLog.Clear();
                    }
                    _txtLog.AppendText(data + Environment.NewLine);
                });
            }
            catch { /* Ignore during shutdown */ }
        }

        /// <summary>
        /// Thread-Safe Connection Status Update.
        /// </summary>
        public void UpdateConnectionStatus(string status, bool isConnected, bool isLiteMode)
        {
            if (!IsHandleCreated || Disposing || IsDisposed) return;

            try
            {
                BeginInvoke((MethodInvoker)delegate
                {
                    _lblStatus.Text = status;
                    _lblStatus.ForeColor = isConnected ? Color.LimeGreen : Color.OrangeRed;

                    if (isLiteMode)
                    {
                        _lblMode.Text = "MODE: LITE";
                        _lblMode.ForeColor = Color.Orange;
                    }
                    else
                    {
                        _lblMode.Text = "MODE: FULL";
                        _lblMode.ForeColor = Color.LimeGreen;
                    }
                });
            }
            catch { /* Ignore during shutdown */ }
        }

        /// <summary>
        /// Thread-Safe Log Append.
        /// </summary>
        public void AppendLog(string line)
        {
            if (!IsHandleCreated || Disposing || IsDisposed) return;

            try
            {
                BeginInvoke((MethodInvoker)delegate
                {
                    if (_txtLog.TextLength > 4000)
                    {
                        _txtLog.Clear();
                    }
                    _txtLog.AppendText(line + Environment.NewLine);
                });
            }
            catch { /* Ignore during shutdown */ }
        }
    }
}
