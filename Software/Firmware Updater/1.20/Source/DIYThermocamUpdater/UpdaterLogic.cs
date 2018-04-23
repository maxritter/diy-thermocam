/*
*
* Updater Logic - Program logic of the updater
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
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using System.Xml;
using System.Threading;

namespace DIYThermocamUpdater
{
    /* Contains the available firmware versions */
    class Firmware
    {
        public string Version { get; set; }
        public string Size { get; set; }
        public string BuildDate { get; set; }
        public string Target { get; set; }
    }

    /* Main Updater Logic */
    static class UpdaterLogic
    {
        /* An instance of the GUI */
        private static UpdaterForm GUI;

        /* Variable declaration */
        private static bool FlashFinished;
        private static int FlashSize;
        private static byte HardwareVersion;
        private static string FirmwareVersion;
        private static string BuildDate;
        private static int FlashCounter;
        private static string Filepath;
        private static List<Firmware> Versions;

        /* Start the Updater */
        public static void Start()
        {
            //Initiate the GUI
            GUI = new UpdaterForm();

            //Enable visual styles for the application
            Application.EnableVisualStyles();

            ExtractVersions();

            //Run the GUI
            Application.Run(GUI);
        }

        /* Exit the application */
        public static void Exit()
        {
            //Kill flash helper process if still running
            foreach (var process in Process.GetProcessesByName("flashHelper"))
            {
                process.Kill();
            }

            //Kill flash reboot process if still running
            foreach (var process in Process.GetProcessesByName("flashReboot"))
            {
                process.Kill();
            }

            //Close the GUI
            GUI.Exit();

            //End the program
            Environment.Exit(0);
        }

        /* Called when the flash helper outprints data to the console */
        private static void FlashDataHandler(object sender, DataReceivedEventArgs e)
        {
            //No data, return 
            if (e.Data == null) return;

            //Flash finished
            if (e.Data.StartsWith("Booting"))
            {
                FlashFinished = true;
                //Attention message
                MessageBox.Show("Firmware update completed!\nThe device should restart now..",
                    "Download complete",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Exclamation,
                    MessageBoxDefaultButton.Button1);
            }

            //One block (1024 byte) transferred
            if (e.Data.StartsWith("."))
                FlashCounter++;

            //Get the used flash size to calc the progressbar
            if (e.Data.StartsWith("Read"))
            {
                string[] words = e.Data.Split(' ');
                FlashSize = Convert.ToInt32(words[2]);
            }
        }

        /* Called when flash helper outprints error messages */
        private static void FlashErrorHandler(object sender, DataReceivedEventArgs e)
        {
            //Return if no data
            if (e.Data == null) return;

            //Any other error than reading hex file, return
            if (!e.Data.StartsWith("error reading intel hex file")) return;

            //When already finished, return
            if (FlashFinished) return;

            //Only show error if not finished so far
            FlashFinished = true;

            //Show error message
            GUI.ShowMessageBox("There was an error opening the hex file.\n" +
                           "Please move it to a folder without a space in the path.",
                           "Download Error", MessageBoxIcon.Exclamation);
        }

        /* Start the flash procedure */
        public static void StartFlash()
        {
            //Refresh flash info
            GUI.FlashInfo.Text = "Waiting for user confirmation..";
            GUI.FlashInfo.Update();

            //Ask the user if he really wants to flash the new FW
            DialogResult result = MessageBox.Show("Flash version " + FirmwareVersion + " from " + BuildDate + "?\n" +
                                                       "Make sure your hardware is really the DIY-Thermocam V" + HardwareVersion + "!",
                                        "Flash OK?", MessageBoxButtons.YesNo);

            //If the answer is no, return
            if (result != DialogResult.Yes)
            {
                //Refresh flash info
                GUI.FlashInfo.Text = "Version " + FirmwareVersion + " from " + BuildDate;
                GUI.FlashInfo.Update();
                return;
            }

            //Disable the load button
            GUI.LoadButton.Enabled = false;
            GUI.LoadButton.BackColor = Color.DimGray;

            //Show warning to turn the device on 
            UpdaterLogic.GUI.ShowMessageBox("Turn the device on and connect it to the PC\n" +
                           "The update process should start after you press OK!",
                           "Important infos", MessageBoxIcon.Exclamation);

            //Trigger a software reboot and put the MCU into programmer mode
            Process flashReboot = new Process { StartInfo = { FileName = "FlashReboot.exe" } };
            flashReboot.Start();

            //Wait until the reboot is done
            flashReboot.WaitForExit();

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
                int counter = 0;
                while (!flashHelper.HasExited)
                {
                    //Wait for device
                    if (FlashCounter == 0)
                    {
                        //Disable flash button
                        GUI.FlashButton.Enabled = false;
                        GUI.FlashButton.BackColor = Color.DimGray;
                        GUI.FlashButton.Update();

                        //Show waiting for device
                        GUI.FlashInfo.Text = "Waiting for device...";
                        GUI.FlashInfo.Update();

                        //Only wait a maximum of two seconds for device to show up
                        Thread.Sleep(100);
                        counter++;
                        if (counter >= 20)
                        {
                            GUI.ShowMessageBox("Connection to the DIY-Thermocam failed..\n" +
                                                       "Please make sure the device is turned on and connected!",
                           "Flash Error", MessageBoxIcon.Exclamation);
                            break;
                        }
                    }
                    //Program devices
                    else
                    {
                        //Calculate percentage
                        byte percentage = (byte)((UpdaterLogic.FlashCounter / (FlashSize / 1024.0)) * 100.0);

                        //Show programming status
                        GUI.FlashInfo.Text = "Programming... (" + percentage + "%)";
                        GUI.FlashInfo.Update();
                    }
                }

                //Error during transmission, try again
                if (FlashFinished == false)
                {
                    //No connection possible
                    if (counter >= 20)
                    {
                        flashHelper.Kill();
                        break;
                    }

                    //Show error message if not start problems
                    if (FlashCounter > 4)
                    {
                        GUI.ShowMessageBox("There was a critical error during the flash procedure.\n" +
                                                        "Open the backside of the enclosure and short the two pins\n" +
                                                        "P & G or the program jumper (V2 only) for about a second.\n" +
                                                        "This puts the device in update mode, so the flash could start again.",
                            "Flash Error", MessageBoxIcon.Exclamation);
                    }

                    //Reset flash coutner and try again
                    FlashCounter = 0;
                }

                //Transmission worked, continue
                else
                    break;
            }

            //Disable load button
            GUI.LoadButton.Enabled = true;
            GUI.LoadButton.BackColor = Color.White;
            GUI.LoadButton.Update();

            //Disable flash button
            GUI.FlashButton.Enabled = false;
            GUI.FlashButton.BackColor = Color.DimGray;
            GUI.FlashButton.Update();

            //Refresh flash label
            GUI.FlashInfo.Text = "Load firmware file first";
            GUI.FlashInfo.Update();

            //Enable exit button
            GUI.ExitButton.Enabled = true;
            GUI.ExitButton.BackColor = Color.White;
            GUI.ExitButton.Update();

            //Reset file path
            Filepath = "";

            //Update status
            FlashFinished = false;
        }

        /* Load a firmware from the hex file */
        public static void LoadFirmware()
        {
            //Store version, hardware and builddate
            string version = "", hardware = "", builddate = "";

            //Create an instance of the open file dialog box
            OpenFileDialog dialog = new OpenFileDialog
            {
                //Only show hex files
                Filter = "Firmware (.hex)|*.hex",
                FilterIndex = 1,
                Multiselect = false
            };

            //Show the file dialog 
            DialogResult result = dialog.ShowDialog();

            //Return if user did not select a file
            if (result != DialogResult.OK) return;

            //Open the selected file to read
            Filepath = dialog.FileName;

            //Clear marker
            bool validFirmware = false;

            //Calculate filesize
            long filesize = new FileInfo(Filepath).Length;

            //Go through the firmware version - recognized by byte size
            foreach (Firmware fw in Versions)
            {
                if (Convert.ToInt64(fw.Size) == filesize)
                {
                    //We found a firmware that matches
                    validFirmware = true;

                    //Save infos
                    version = fw.Version;
                    hardware = fw.Target;
                    builddate = fw.BuildDate;

                    //Leave loop
                    break;
                }
            }

            //If the firmware version is not known
            if (!validFirmware)
            {
                //Default means no valid firmware, show error msg
                GUI.ShowMessageBox("Unable to recognize the device firmware!\n" +
                                "If this is a beta, continue flashing.\n" +
                                "Otherwise download the correct file again.",
                                "Unknown firmware version", MessageBoxIcon.Exclamation);

                //We do not know the version and build date
                FirmwareVersion = "unknown";
                BuildDate = "unknown";

                //Refresh flash info
                GUI.FlashInfo.Text = "Unknown firmware version (beta?)";
                GUI.FlashInfo.Update();
            }

            //If it is known, check for valid hardware
            else
            {
                //Check for which hardware this file is
                byte hw = (byte)(string.Equals(hardware, "V1") ? 1 : 2);

                //Get the hardware version from file
                HardwareVersion = hw;

                //Set firmware version and build date
                FirmwareVersion = version;
                BuildDate = builddate;

                //Refresh flash info
                GUI.FlashInfo.Text = "Version " + FirmwareVersion + " from " + BuildDate;
                GUI.FlashInfo.Update();
            }

            //Reset flash counter
            FlashCounter = 0;

            //Enable flash button
            GUI.FlashButton.BackColor = Color.White;
            GUI.FlashButton.Enabled = true;
        }

        /* Fill the datatable with the versions from the file */
        public static void ExtractVersions()
        {
            //Set marker to true
            bool fileValid = true;

            //Check again, if the file is there
            if (!File.Exists("Versions.dat"))
            {
                MessageBox.Show("Version file not found, program will close!",
                    "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                Exit();
            }

            //Create a new list to store the elements
            Versions = new List<Firmware>();

            //Try to load the XML document
            XmlDocument doc = new XmlDocument();
            try
            {
                doc.Load("Versions.dat");
            }
            //Not working
            catch (Exception)
            {
                fileValid = false;
            }

            //Check for file valid
            if (doc.DocumentElement == null) fileValid = false;

            //Get parent nodes
            XmlNodeList nodes = null;
            if (fileValid)
                nodes = doc.DocumentElement.SelectNodes("/versions/firmware");

            // Check for file valid
            if (nodes == null) fileValid = false;

            //Extract attributes and child nodes
            if (fileValid)
            {
                //Go through all firmware nodes
                foreach (XmlNode node in nodes)
                {
                    //Create a new firmware object and read the properties from the XML
                    Firmware fw = new Firmware
                    {
                        Version = node.Attributes?["version"].Value,
                        Size = node.SelectSingleNode("size")?.InnerText,
                        BuildDate = node.SelectSingleNode("builddate")?.InnerText,
                        Target = node.SelectSingleNode("target")?.InnerText
                    };

                    //Add this object if it contains all required information
                    if ((fw.Version != null) && (fw.Size != null) &&
                        (fw.BuildDate != null) && (fw.Target != null))
                        Versions.Add(fw);
                }
            }

            //Show error message and exit, if file invalid
            else
            {
                MessageBox.Show("Version file invalid, program will close!",
                    "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                Exit();
            }
        }

    }
}
