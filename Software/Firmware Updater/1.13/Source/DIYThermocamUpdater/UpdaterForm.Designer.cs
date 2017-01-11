using System.ComponentModel;
using System.Windows.Forms;

namespace DIYThermocamUpdater
{
    partial class UpdaterForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.title = new System.Windows.Forms.Label();
            this.connectButton = new System.Windows.Forms.Button();
            this.connectInfo = new System.Windows.Forms.Label();
            this.exitButton = new System.Windows.Forms.Button();
            this.flashButton = new System.Windows.Forms.Button();
            this.loadButton = new System.Windows.Forms.Button();
            this.flashInfo = new System.Windows.Forms.Label();
            this.panel1 = new System.Windows.Forms.Panel();
            this.panel2 = new System.Windows.Forms.Panel();
            this.panel3 = new System.Windows.Forms.Panel();
            this.panel1.SuspendLayout();
            this.panel2.SuspendLayout();
            this.panel3.SuspendLayout();
            this.SuspendLayout();
            // 
            // Title
            // 
            this.title.Font = new System.Drawing.Font("Arial", 14.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.title.ForeColor = System.Drawing.Color.White;
            this.title.Location = new System.Drawing.Point(19, 14);
            this.title.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.title.Name = "title";
            this.title.Size = new System.Drawing.Size(237, 22);
            this.title.TabIndex = 1;
            this.title.Text = "DIY-Thermocam Updater";
            this.title.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // connectButton
            // 
            this.connectButton.BackColor = System.Drawing.Color.White;
            this.connectButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.connectButton.Font = new System.Drawing.Font("Arial", 10.8F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.connectButton.ForeColor = System.Drawing.Color.Black;
            this.connectButton.Location = new System.Drawing.Point(57, 57);
            this.connectButton.Margin = new System.Windows.Forms.Padding(2);
            this.connectButton.Name = "connectButton";
            this.connectButton.Size = new System.Drawing.Size(156, 43);
            this.connectButton.TabIndex = 3;
            this.connectButton.Text = "Connect";
            this.connectButton.UseVisualStyleBackColor = false;
            this.connectButton.Click += new System.EventHandler(this.ConnectButton_Click);
            // 
            // connectInfo
            // 
            this.connectInfo.Font = new System.Drawing.Font("Arial", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.connectInfo.ForeColor = System.Drawing.Color.White;
            this.connectInfo.Location = new System.Drawing.Point(20, 102);
            this.connectInfo.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.connectInfo.Name = "connectInfo";
            this.connectInfo.Size = new System.Drawing.Size(233, 26);
            this.connectInfo.TabIndex = 4;
            this.connectInfo.Text = "Press the connect button";
            this.connectInfo.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // exitButton
            // 
            this.exitButton.BackColor = System.Drawing.Color.White;
            this.exitButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.exitButton.Font = new System.Drawing.Font("Arial", 10.8F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.exitButton.ForeColor = System.Drawing.Color.Black;
            this.exitButton.Location = new System.Drawing.Point(57, 90);
            this.exitButton.Margin = new System.Windows.Forms.Padding(2);
            this.exitButton.Name = "exitButton";
            this.exitButton.Size = new System.Drawing.Size(156, 41);
            this.exitButton.TabIndex = 5;
            this.exitButton.Text = "Exit";
            this.exitButton.UseVisualStyleBackColor = false;
            this.exitButton.Click += new System.EventHandler(this.ExitButton_Click);
            // 
            // flashButton
            // 
            this.flashButton.BackColor = System.Drawing.Color.DimGray;
            this.flashButton.Enabled = false;
            this.flashButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.flashButton.Font = new System.Drawing.Font("Arial", 10.8F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.flashButton.ForeColor = System.Drawing.Color.Black;
            this.flashButton.Location = new System.Drawing.Point(57, 11);
            this.flashButton.Margin = new System.Windows.Forms.Padding(2);
            this.flashButton.Name = "flashButton";
            this.flashButton.Size = new System.Drawing.Size(156, 41);
            this.flashButton.TabIndex = 6;
            this.flashButton.Text = "Flash";
            this.flashButton.UseVisualStyleBackColor = false;
            this.flashButton.Click += new System.EventHandler(this.FlashButton_Click);
            // 
            // loadButton
            // 
            this.loadButton.BackColor = System.Drawing.Color.DimGray;
            this.loadButton.Enabled = false;
            this.loadButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.loadButton.Font = new System.Drawing.Font("Arial", 10.8F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.loadButton.ForeColor = System.Drawing.Color.Black;
            this.loadButton.Location = new System.Drawing.Point(57, 15);
            this.loadButton.Margin = new System.Windows.Forms.Padding(2);
            this.loadButton.Name = "loadButton";
            this.loadButton.Size = new System.Drawing.Size(156, 41);
            this.loadButton.TabIndex = 7;
            this.loadButton.Text = "Load Hex File";
            this.loadButton.UseVisualStyleBackColor = false;
            this.loadButton.Click += new System.EventHandler(this.LoadButton_Click);
            // 
            // flashInfo
            // 
            this.flashInfo.Font = new System.Drawing.Font("Arial", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.flashInfo.ForeColor = System.Drawing.Color.White;
            this.flashInfo.Location = new System.Drawing.Point(20, 53);
            this.flashInfo.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.flashInfo.Name = "flashInfo";
            this.flashInfo.Size = new System.Drawing.Size(233, 26);
            this.flashInfo.TabIndex = 8;
            this.flashInfo.Text = "Connect and load file";
            this.flashInfo.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // panel1
            // 
            this.panel1.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.panel1.Controls.Add(this.title);
            this.panel1.Controls.Add(this.connectButton);
            this.panel1.Controls.Add(this.connectInfo);
            this.panel1.Location = new System.Drawing.Point(-4, 0);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(272, 135);
            this.panel1.TabIndex = 9;
            // 
            // panel2
            // 
            this.panel2.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.panel2.Controls.Add(this.loadButton);
            this.panel2.Location = new System.Drawing.Point(-4, 134);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(272, 74);
            this.panel2.TabIndex = 10;
            // 
            // panel3
            // 
            this.panel3.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.panel3.Controls.Add(this.flashButton);
            this.panel3.Controls.Add(this.flashInfo);
            this.panel3.Controls.Add(this.exitButton);
            this.panel3.Location = new System.Drawing.Point(-4, 207);
            this.panel3.Name = "panel3";
            this.panel3.Size = new System.Drawing.Size(272, 143);
            this.panel3.TabIndex = 11;
            // 
            // Updater
            // 
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.None;
            this.BackColor = System.Drawing.Color.MidnightBlue;
            this.ClientSize = new System.Drawing.Size(265, 350);
            this.ControlBox = false;
            this.Controls.Add(this.panel3);
            this.Controls.Add(this.panel2);
            this.Controls.Add(this.panel1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Margin = new System.Windows.Forms.Padding(2);
            this.Name = "Updater";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "DIY-Thermocam Updater 1.13";
            this.panel1.ResumeLayout(false);
            this.panel2.ResumeLayout(false);
            this.panel3.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion
        private Button connectButton;
        private Label connectInfo;
        private Button exitButton;
        private Button flashButton;
        private Button loadButton;
        private Label  flashInfo;
        private Panel panel1;
        private Panel panel2;
        private Panel panel3;
        private Label title;
    }
}

