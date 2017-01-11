/*
*
* Updater Form - Graphical User Interface
*
* DIY-Thermocam Firmware Updater
*
* GNU General Public License v3.0
*
* Copyright by Max Ritter
*
* http://www.diy-thermocam.net
* https://github.com/maxritter/DIY-Thermocam
*
*/

/* Includes */
using System;
using System.Drawing;
using System.Windows.Forms;

namespace DIYThermocamUpdater
{
    /* Updater form */
    internal partial class UpdaterForm : Form
    {
        /* Property modifier */
        public Button ConnectButton
        {
            get { return connectButton; }
            set { connectButton = value; }
        }
        public Label ConnectInfo
        {
            get { return connectInfo; }
            set { connectInfo = value; }
        }
        public Button ExitButton
        {
            get { return exitButton; }
            set { exitButton = value; }
        }
        public Button FlashButton
        {
            get { return flashButton; }
            set { flashButton = value; }
        }
        public Button LoadButton
        {
            get { return loadButton; }
            set { loadButton = value; }
        }
        public Label FlashInfo
        {
            get { return flashInfo; }
            set { flashInfo = value; }
        }

        /* Constructor */
        public UpdaterForm()
        {
            //Init the UI
            InitializeComponent(); 
        }

        /* Close the UI */
        public void Exit()
        {
            Application.Exit();
        }

        /* Restores the UI after a complete flash or connection lost */
        public void RestoreUI()
        {
            //Enable connect button
            connectButton.Enabled = true;
            connectButton.BackColor = Color.White;
            connectButton.Update();

            //Refresh connect label
            connectInfo.Text = "Press the connect button";
            connectInfo.Update();

            //Disable load button
            loadButton.Enabled = false;
            loadButton.BackColor = Color.DimGray;
            loadButton.Update();

            //Disable flash button
            flashButton.Enabled = false;
            flashButton.BackColor = Color.DimGray;
            flashButton.Update();

            //Refresh flash label
            flashInfo.Text = "Connect and load file";
            flashInfo.Update();

            //Enable exit button
            exitButton.Enabled = true;
            exitButton.BackColor = Color.White;
            exitButton.Update();
        }

        /* Show a message box */
        public void ShowMessageBox(string text, string heading, MessageBoxIcon icon)
        {
            MessageBox.Show(text,
                    heading,
                    MessageBoxButtons.OK,
                    icon,
                    MessageBoxDefaultButton.Button1);
        }

        /* Connect to the DIY-Thermocam V1 / V2 */
        private void ConnectButton_Click(object sender, EventArgs e)
        {
            UpdaterLogic.Connect();
        }

        /* Load a firmware from file */
        private void LoadButton_Click(object sender, EventArgs e)
        {
            UpdaterLogic.LoadFirmware();
        }

        /* Start the flash procedure */
        private void FlashButton_Click(object sender, EventArgs e)
        {
            UpdaterLogic.StartFlash();
        }

        /* Exit the program */
        private void ExitButton_Click(object sender, EventArgs e)
        {
            UpdaterLogic.Exit();
        }
    }
}
