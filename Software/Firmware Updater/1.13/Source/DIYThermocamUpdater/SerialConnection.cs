/*
*
* Serial Connection - Communicate with the Thermocam
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
using System.IO;
using System.IO.Ports;
using System.Windows.Forms;

namespace DIYThermocamUpdater
{
    /* Serial connection */
    class SerialConnection
    {
        /* This is our serial port */
        private SerialPort serialPort;

        /* Constructor */
        public SerialConnection()
        {
            //Open a new serial port on COM10 with max baudrate
            serialPort = new SerialPort
            {
                BaudRate = 115200,
                PortName = "COM10"
            };
        }

        /* Async check for the exit command from the serial port */
        private void DataReceivedHandler(object sender, SerialDataReceivedEventArgs e)
        {
            //Read the byte command
            var data = (byte) serialPort.ReadByte();

            //If exit command is received, end connection
            if (data == 200)
            {
                //Set connected status to false
                UpdaterLogic.Connected = false;

                //Discard buffer
                serialPort.DiscardOutBuffer();
                serialPort.DiscardInBuffer();

                //Close serial port
                serialPort.Close();

                //Remove data received handler
                serialPort.DataReceived -= DataReceivedHandler;

                //Show error message
                UpdaterLogic.GUI.ShowMessageBox("The connection to the device has been lost.\n" +
                                            "Please reconnect and try again.",
                    "Connection lost", MessageBoxIcon.Exclamation);

                //Restore UI
                MethodInvoker mi = UpdaterLogic.GUI.RestoreUI;
                UpdaterLogic.GUI.Invoke(mi);
            }
        }

        /* Get the firmware version from the device */
        public void GetFirmwareVersion()
        {
            //Show waiting message
            UpdaterLogic.GUI.ConnectInfo.Text = "Getting firmware version..";
            UpdaterLogic.GUI.ConnectInfo.Update();

            //Try to get the firmware version
            try
            {
                //Send get firmware version command
                serialPort.Write(new byte[] {129}, 0, 1);

                //Read the answer
                byte input = (byte) serialPort.ReadByte();

                //Check if data matches answer
                if (input < 100)
                    UpdaterLogic.Connected = false;

                //Save firmware version
                else
                    UpdaterLogic.FirmwareInstalled = input;
            }
            //Timeout
            catch (TimeoutException)
            {
                UpdaterLogic.Connected = false;
            }
        }

        /* Get the hardware version from the device */
        public void GetHardwareVersion()
        {
            //Show waiting message
            UpdaterLogic.GUI.ConnectInfo.Text = "Getting hardware version..";
            UpdaterLogic.GUI.ConnectInfo.Update();

            //Try to get the hardware version
            try
            {
                //Send get hardware version command
                serialPort.Write(new byte[] {138}, 0, 1);

                //Read the answer
                byte input = (byte) serialPort.ReadByte();

                //Check if data matches answer
                if ((input != 0) && (input != 1))

                    UpdaterLogic.Connected = false;
                //Save hardware version
                else
                    UpdaterLogic.HardwareVersion = (byte) (input + 1);
            }

            //Timeout - function not implemented
            catch (TimeoutException)
            {
                UpdaterLogic.Connected = false;
            }
        }

        /* Get the current battery level from the device */
        public bool GetBatteryStatus()
        {
            //Show waiting message
            UpdaterLogic.GUI.FlashInfo.Text = "Getting battery status..";
            UpdaterLogic.GUI.FlashInfo.Update();

            //Try to get the hardware version
            try
            {
                //Send get battery status command
                serialPort.Write(new byte[] {124}, 0, 1);

                //Read the answer
                byte input = (byte) serialPort.ReadByte();

                //If battery is too low
                if (input < 20)
                {
                    //Show error message when connection is not possible
                    UpdaterLogic.GUI.ShowMessageBox("Please recharge the battery to continue!\n" +
                                   "The status has to be 20 percentage or higher.",
                        "Recharge battery", MessageBoxIcon.Error);

                    //Refresh flash info
                    UpdaterLogic.GUI.FlashInfo.Text = "Version " + UpdaterLogic.FirmwareVersion + " from " + UpdaterLogic.BuildDate;
                    UpdaterLogic.GUI.FlashInfo.Update();

                    //Exit handler
                    return false;
                }
            }
            //Timeout
            catch (TimeoutException)
            {
                //Show error message when connection is not possible
                UpdaterLogic.GUI.ShowMessageBox("Error getting the battery status!" +
                               "Please make sure the device is still connected.",
                    "Connection erorr", MessageBoxIcon.Error);

                //Refresh flash info
                UpdaterLogic.GUI.FlashInfo.Text = "Version " + UpdaterLogic.FirmwareVersion + " from " + UpdaterLogic.BuildDate;
                UpdaterLogic.GUI.FlashInfo.Update();

                //Exit handler
                return false;
            }
            //Everything worked
            return true;
        }

        /* Try to establish a serial connection to the device */
        public bool Connect()
        {
            //Try to connect to COM10
            try
            {
                //Open port
                serialPort.Open();

                //Clear buffer
                serialPort.DiscardOutBuffer();
                serialPort.DiscardInBuffer();

                //Set timeout to three seconds
                serialPort.ReadTimeout = 3000;

                //And send start command
                serialPort.Write(new byte[] {100}, 0, 1);
            }

            //Catch possible connection error
            catch (IOException)
            {
                //Ask the user if he wants to flash anyway
                DialogResult result = MessageBox.Show("Unable to connect to the Thermocam on port COM10.\n" +
                                                      "Make sure the device is turned on and set to live mode.\n" +
                                                      "Do you want to continue flashing without an active connection?",
                    "Continue without Connection", MessageBoxButtons.YesNo);

                //Continue without connection
                if (result == DialogResult.Yes)
                {
                    UpdaterLogic.Connected = false;
                    return true;
                }

                //Do not continue
                return false;
            }

            //Everything worked
            return true;
        }

        /* Send the disconnect command if required */
        public void Disconnect()
        {
            //Check if the connection is still alive
            if (serialPort.IsOpen)
            {
                //Send exit command
                serialPort.Write(new byte[] { 200 }, 0, 1);

                //Close port
                serialPort.Close();
            }
        }

        /* Add the interrupt based serial handler */
        public void AddSerialHandler()
        {
            serialPort.DataReceived += DataReceivedHandler;
        }

        /* Close the serial connection */
        public void CloseConnection()
        {
            serialPort.Close();
        }

        /* Returns true if the serial port is open */
        public bool CheckConnection()
        {
            return serialPort.IsOpen;
        }

        /* Try to get information from the device on startup */
        public void GetInfos()
        {
            //Try to get an answer within three seconds
            try
            {
                //Read the acknowledge byte
                byte input = (byte)serialPort.ReadByte();

                //Check if data matches answer
                if (input != 100)
                    UpdaterLogic.Connected = false;
            }
            //Timeout
            catch (TimeoutException)
            {
                UpdaterLogic.Connected = false;
            }

            //If that worked, proceed with next information
            if (UpdaterLogic.Connected)
            {
                //Set timeout to one second
                serialPort.ReadTimeout = 1000;

                //Get the hardware & firmware version
                GetFirmwareVersion();
                GetHardwareVersion();
            }
        }
    }
}
