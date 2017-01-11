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
using System.Globalization;
using System.IO;
using System.Windows.Forms;
using System.Xml;

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
    internal static class UpdaterLogic
    {
        /* An instance of the GUI */
        public static UpdaterForm GUI { get; set; }

        /* An instance of the Serial Connection */
        public static SerialConnection Serial { get; set; }

        /* Variable definitions */
        public static bool Connected { get; set; }
        public static byte HardwareVersion { get; set; }
        public static byte FirmwareInstalled { get; set; }
        public static string FirmwareVersion { get; set; }
        public static string BuildDate { get; set; }
        public static bool FlashStarted { get; set; }
        public static int FlashCounter { get; set; }
        public static string Filepath { get; set; }
        public static List<Firmware> Versions { get; set; }

        /* Start the Updater */
        public static void Start()
        {
            //Initiate the GUI
            GUI = new UpdaterForm();

            //Initiate the Serial Connection
            Serial = new SerialConnection();

            //Initiate the Flasher Thread
            new FlashThread();

            //Enable visual styles for the application
            Application.EnableVisualStyles();

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

        /* Connect to the device */
        public static void Connect()
        {
            //Set connection marker to true
            Connected = true;

            //Try to connect, otherwise return
            if (!Serial.Connect())
                return;

            //If the connection worked
            if (Connected)
            {
                //Show waiting message
                GUI.ConnectInfo.Text = "Waiting for device..";
                GUI.ConnectInfo.Update();

                //Get the infos from the device
                Serial.GetInfos();

                //Connection not working because of an error
                if (!Connected)
                {
                    //Ask the user if he wants to flash anyway
                    DialogResult result = MessageBox.Show("Unable to connect to the Thermocam on port COM10.\n" +
                                                           "Make sure the device is turned on and set to live mode.\n" +
                                                           "Do you want to continue flashing without an active connection?",
                                            "Continue without Connection", MessageBoxButtons.YesNo);

                    //If the answer is no, return
                    if (result == DialogResult.No)
                    {
                        //Reset label
                        GUI.ConnectInfo.Text = "Press the connect button";

                        //Close the serial connection
                        Serial.CloseConnection();

                        //Leave
                        return;
                    }

                    //If it is yes, set hardware version to unknown
                    HardwareVersion = 0;
                }
            }
            //Set hardware version to unknown for no connection
            else
                HardwareVersion = 0;

            //Connection established
            GUI.ConnectButton.Enabled = false;
            GUI.ConnectButton.BackColor = Color.DimGray;

            //Set status text
            if (HardwareVersion == 0)
                GUI.ConnectInfo.Text = "Flash without active connection";
            else
                GUI.ConnectInfo.Text = "Connected to DIY-Thermocam V" + HardwareVersion;

            //Add interrupt based serial handler
            Serial.AddSerialHandler();

            //Enable load and button
            GUI.LoadButton.Enabled = true;
            GUI.LoadButton.BackColor = Color.White;
        }

        /* Start the flash procedure */
        public static void StartFlash()
        {
            //Get battery status, only when connected
            if (Connected)
                if (!Serial.GetBatteryStatus())
                    return;

            //Refresh flash info
            GUI.FlashInfo.Text = "Waiting for user confirmation..";
            GUI.FlashInfo.Update();

            //Dialog
            DialogResult result;

            //When there is an active connection, show current installed version
            if (Connected)
            {
                //Calc current firmware version
                double fw = FirmwareInstalled / 100.0;

                //Ask the user if he really wants to flash the new FW
                result =
                    MessageBox.Show("Flash version " + FirmwareVersion + " from " + BuildDate + "?\n" +
                                    "Current installed firmware version is " +
                                    fw.ToString("0.00", CultureInfo.InvariantCulture) + ".",
                        "Flash OK?", MessageBoxButtons.YesNo);
            }

            //Otherwise, show the target hardware
            else
                //Ask the user if he really wants to flash the new FW
                result = MessageBox.Show("Flash version " + FirmwareVersion + " from " + BuildDate + "?\n" +
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

            //Action done
            FlashStarted = true;
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
                //If there is no active connection, does not work
                if (!Connected)
                {
                    GUI.ShowMessageBox("Unable to recognize the device firmware!\n" +
                                    "As there is no active connection, the\n" +
                                    "hardware version could not be detected.\n" +
                                    "Please try to load another valid file.",
                                    "Unknown firmware version", MessageBoxIcon.Error);

                    //Refresh flash info
                    GUI.FlashInfo.Text = "Connect and load file";
                    GUI.FlashInfo.Update();

                    //Leave
                    return;
                }

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

                //If there is no active connection, get the hardware info from file
                if ((HardwareVersion == 0) || (!Connected))
                {
                    HardwareVersion = hw;
                    GUI.ConnectInfo.Text = "File for DIY-Thermocam V" + HardwareVersion;
                }

                //Compare with hardware info from serial port
                else if (hw != HardwareVersion)
                {
                    //Default means no valid firmware, show error msg
                    GUI.ShowMessageBox("The loaded firmware version\n" +
                                    "does not work on this device!",
                                    "Invalid firmware", MessageBoxIcon.Error);

                    //Refresh flash info
                    GUI.FlashInfo.Text = "Connect and load file";
                    GUI.FlashInfo.Update();

                    //Leave
                    return;
                }

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
