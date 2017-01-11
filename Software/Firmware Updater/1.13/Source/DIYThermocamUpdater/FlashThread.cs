/*
*
* Flash Thread - The background flash procedure runs here
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
using System.Diagnostics;
using System.Drawing;
using System.Threading;
using System.Windows.Forms;

namespace DIYThermocamUpdater
{
    /* Flash thread */
    class FlashThread
    {
        /* Variable declaration */
        private bool finished;
        private int flashSize;

        /* Start flash procedure as background thread */
        public FlashThread()
        {
            var flashThread = new Thread(FlashProcedure);
            flashThread.Start();
        }

        /* Called when the flash helper outprints data to the console */
        private void FlashDataHandler(object sender, DataReceivedEventArgs e)
        {
            //No data, return 
            if (e.Data == null) return;

            //Flash finished
            if (e.Data.StartsWith("Booting"))
            {
                finished = true;
                //Attention message
                MessageBox.Show("Firmware update completed!\nThe device should restart now..",
                    "Download complete",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Exclamation,
                    MessageBoxDefaultButton.Button1);
            }

            //One block (1024 byte) transferred
            if (e.Data.StartsWith("."))
                UpdaterLogic.FlashCounter++;

            //Get the used flash size to calc the progressbar
            if (e.Data.StartsWith("Read"))
            {
                string[] words = e.Data.Split(' ');
                flashSize = Convert.ToInt32(words[2]);
            }
        }

        /* Called when flash helper outprints error messages */
        private void FlashErrorHandler(object sender, DataReceivedEventArgs e)
        {
            //Return if no data
            if (e.Data == null) return;

            //Any other error than reading hex file, return
            if (!e.Data.StartsWith("error reading intel hex file")) return;

            //When already finished, return
            if (finished) return;

            //Only show error if not finished so far
            finished = true;

            //Show error message
            UpdaterLogic.GUI.ShowMessageBox("There was an error opening the hex file.\n" +
                           "Please move it to a folder without a space in the path.",
                           "Download Error", MessageBoxIcon.Exclamation);
        }

        /* Flash procedure, that runs in the thread */
        private void FlashProcedure()
        {
            //Extract versions from file
            UpdaterLogic.ExtractVersions();

            //Loop forever
            while (true)
            {
                //As long as the user did not start the flash, wait
                if (!UpdaterLogic.FlashStarted) continue;

                //If there is no active connection, show warning to turn the device on 
                if (!UpdaterLogic.Connected)
                    UpdaterLogic.GUI.ShowMessageBox("Turn the device on and connect it to the PC\n" +
                               "It should be put into program mode automatically after you press OK.\n" +
                               "If this does not work, open the backside of the enclosure and\n" +
                               "short the P & G pins on the Teensy or the program jumper (V2 only)\n" +
                               "for about one second with a screwdriver, the flash should start then.",
                               "Important infos", MessageBoxIcon.Exclamation);

                //User wants to start the flash, show warning with active connection
                else
                    UpdaterLogic.GUI.ShowMessageBox("The flash procedure does start now.\n" +
                               "Do not close the program or disconnect the device!", "Important infos", MessageBoxIcon.Exclamation);

                //Close serial connection if still active
                if (UpdaterLogic.Serial.CheckConnection())
                    UpdaterLogic.Serial.CloseConnection();

                //Trigger a software reboot and put the MCU into programmer mode
                Process flashReboot = new Process { StartInfo = { FileName = "FlashReboot.exe" } };
                flashReboot.Start();

                //Wait until the reboot is done
                flashReboot.WaitForExit();

                //Change the status of the UI during the flash
                MethodInvoker mi;

                //Repeat until the flash has been completed
                while (true)
                {

                    //Create the argument for the flash helper
                    string arg;

                    //DIY-Thermocam V2
                    if (UpdaterLogic.HardwareVersion == 2)
                        arg = "-mmcu=mk66fx1m0 -v -w " + UpdaterLogic.Filepath;

                    //DIY-Thermocam V1
                    else
                        arg = "-mmcu=mk20dx256 -v -w " + UpdaterLogic.Filepath;

                    //Teensy flash helper process with corresponding arguments
                    Process flashHelper = new Process
                    {
                        StartInfo =
                        {
                            FileName = "FlashHelper.exe",
                            Arguments = arg,
                            RedirectStandardOutput = true,
                            RedirectStandardError = true,
                            RedirectStandardInput = true,
                            UseShellExecute = false,
                            CreateNoWindow = true
                        }
                    };

                    //Redirect messages
                    flashHelper.ErrorDataReceived += FlashErrorHandler;
                    flashHelper.OutputDataReceived += FlashDataHandler;
                    flashHelper.EnableRaisingEvents = true;

                    //Start process
                    flashHelper.Start();
                    flashHelper.BeginOutputReadLine();
                    flashHelper.BeginErrorReadLine();

                    //As long as the process has not exited
                    while (!flashHelper.HasExited)
                    {
                        //Update flash info during programming
                        mi = delegate
                        {
                            //Wait for device
                            if (UpdaterLogic.FlashCounter == 0)
                            {
                                //Disable flash button
                                UpdaterLogic.GUI.FlashButton.Enabled = false;
                                UpdaterLogic.GUI.FlashButton.BackColor = Color.DimGray;
                                UpdaterLogic.GUI.FlashButton.Update();

                                //Show waiting for device
                                UpdaterLogic.GUI.FlashInfo.Text = "Waiting for device...";
                                UpdaterLogic.GUI.FlashInfo.Update();
                            }
                            //Program devices
                            else
                            {
                                //Calculate percentage
                                byte percentage = (byte)((UpdaterLogic.FlashCounter / (flashSize / 1024.0)) * 100.0);

                                //Show programming status
                                UpdaterLogic.GUI.FlashInfo.Text = "Programming... (" + percentage + "%)";
                                UpdaterLogic.GUI.FlashInfo.Update();
                            }
                        };
                        UpdaterLogic.GUI.Invoke(mi);
                    }

                    //Error during transmission, try again
                    if (finished == false)
                    {
                        //Show error message if not start problems
                        if (UpdaterLogic.FlashCounter > 4)
                        {
                            UpdaterLogic.GUI.ShowMessageBox("There was a critical error during the flash procedure.\n" +
                                                            "Open the backside of the enclosure and short the two pins\n" +
                                                            "P & G or the program jumper (V2 only) for about a second.\n" +
                                                            "This puts the device in update mode, so the flash could start again.",
                                "Flash Error", MessageBoxIcon.Exclamation);
                        }

                        //Reset flash coutner and try again
                        UpdaterLogic.FlashCounter = 0;
                    }

                    //Transmission worked, continue
                    else
                        break;
                }

                //Restore UI
                mi = UpdaterLogic.GUI.RestoreUI;
                UpdaterLogic.GUI.Invoke(mi);

                //Reset file path
                UpdaterLogic.Filepath = "";

                //Update status
                finished = false;
                UpdaterLogic.FlashStarted = false;
            }
        }
    }
}
