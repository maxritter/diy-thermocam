/*
*
* Updater Main - Main entry point for the application
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
using System.Net;
using System.Windows.Forms;

namespace DIYThermocamUpdater
{
    /* Main Updater Class */
    internal class UpdaterMain
    {
        //Check if we can get the version file
        private static bool CheckVersionFile()
        {
            //Set marker to true
            bool validFile = true;

            using (var client = new WebClient())
            {
                //First try to download it from the server
                try
                {
                    client.DownloadFile("http://diy-thermocam.bplaced.net/Versions.xml", "Versions_new.dat");
                }

                //If that does not work, set marker to false
                catch (WebException)
                {
                    validFile = false;
                }
            }

            //Check if this is the ERROR404 page - file not found
            if (File.ReadAllText("Versions_new.dat").Contains("Diese Seite ist leider nicht mehr verfügbar"))
            {
                //Delete the downlaoded file
                File.Delete("Versions_new.dat");

                //Set marker to false
                validFile = false;
            }

            //If the new file could not be loaded
            if (!validFile)
            {
                //Check if a local copy is available
                if (File.Exists("Versions.dat"))
                {
                    //Works, but show a warning message
                    MessageBox.Show("Unable to download version file, using old one..\n" +
                                    "If you want to update the version file, please connect to the internet!",
                        "Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
                //No copy available, return false
                else
                    return false;
            }

            //Working
            return true;
        }


        //Main entry point
        [STAThread]
        public static void Main()
        {
            //Check if the FlashHelper and FlashUpdater are available
            if (!File.Exists("FlashHelper.exe") || (!File.Exists("FlashReboot.exe")))
            {
                //Show message
                MessageBox.Show("The FlashHelper.exe or the FlashReboot.exe is missing!\n" +
                                "Please copy those two files to the same folder than the updater.",
                    "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                //End program execution
                return;
            }

            //Check if we can get the version file
            if (CheckVersionFile())
            {
                //Delete old local file if new one downloaded
                if (File.Exists("Versions.dat") && File.Exists("Versions_new.dat"))
                    File.Delete("Versions.dat");

                //Rename the new file to use ist
                if (File.Exists("Versions_new.dat"))
                    File.Move("Versions_new.dat", "Versions.dat");

                //Start the updater
                UpdaterLogic.Start();
            }

            //If no version file is available, show message and end program
            else
                MessageBox.Show("Unable to download version file and no old copy found, exit!\n" +
                                "Please connect to the internet and try again.",
                                "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

        }
    }
}
