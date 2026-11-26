/*
 *
 * MASS STORAGE -  Mass storage mode to connect the internal storage to the PC
 *
 * DIY-Thermocam Firmware
 *
 * GNU General Public License v3.0
 *
 * Copyright by Max Ritter
 *
 * http://www.diy-thermocam.net
 * https://github.com/maxritter/diy-thermocam
 *
 */

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <mainmenu.h>
#include <hardware.h>
#include <touchscreen.h>
#include <EEPROM.h>
#include <gui.h>
#include <massstorage.h>
#include <thermal.h>

#include <SD.h>
#include <MTP_Teensy.h>
#include <TimeLib.h>
#include <sdcard.h>
#include <lepton.h>
#include <display.h>
#include <fonts.h>

/*################# DATA TYPES, CONSTANTS & MACRO DEFINITIONS #################*/

#define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
#define CPU_RESTART_VAL 0x5FA0004
#define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL);

/*######################## PUBLIC FUNCTION BODIES #############################*/

static void mtpDateTime(uint16_t *date, uint16_t *time, uint8_t *ms10)
{
	*date = FS_DATE(year(), month(), day());
	*time = FS_TIME(hour(), minute(), second());
	*ms10 = second() & 1 ? 100 : 0;
}

bool storage_configure()
{
	if (SD.begin(BUILTIN_SDCARD))
	{
		MTP.addFilesystem(SD, "Thermocam SD");
		return true;
	}
	return false;
}

void enterMassStorage()
{
	//Only do this if the EEPROM marker is set
	if ((EEPROM.read(eeprom_massStorage) == eeprom_setValue))
	{
		EEPROM.write(eeprom_massStorage, 0);
		showModalCard(
			"USB FILE TRANSFER",
			"MTP active on SD card (FAT/exFAT)",
			"Win Explorer / Linux / macOS MTP",
			"Touch screen to exit to Live View");

		//Start the core's MTP responder, then mount SD via SD.begin(BUILTIN_SDCARD)
		//so SDClass sets cardPreviouslyPresent & cdPin_ for MTP mediaPresent()/totalSize()
		MTP.begin();
		if (!storage_configure())
		{
			showFullMessage((char *)"SD card not found!");
			delay(1200);
			ui_waitTouchReleaseAndLockout(250u);
			return;
		}
		FsDateTime::setCallback(mtpDateTime);

		//Ensure no residual touch from boot immediately exits MTP
		uint32_t t0 = millis();
		while (touch_touched() && (millis() - t0) < 500u)
		{
			MTP.loop();
			delay(5);
		}

		//Serve MTP at full wire speed (fast digitalRead on touch IRQ avoids XPT2046 delay(20)!)
		uint32_t lastHousekeepMs = millis();
		while (true)
		{
			MTP.loop();
			if (!digitalRead(pin_touch_irq))
			{
				if (touch_touched())
					break;
			}
			uint32_t nowMs = millis();
			if ((nowMs - lastHousekeepMs) >= 250u)
			{
				lastHousekeepMs = nowMs;
				if (!isUSBConnected())
					break;
				if (touch_capacitive && touch_touched())
					break;
			}
		}

		showFullMessage((char *)"Exiting USB File Transfer..");
		MTP.reset();
		delay(250);
		SD.sdfs.end();
		ui_waitTouchReleaseAndLockout(250u);
		disableSPIIRQ = false;
		beginSD();
	}
}

void setMassStorage()
{
	//Set marker and reboot
	showFullMessage((char *)"Entering MTP, device reboots..");
	EEPROM.write(eeprom_massStorage, eeprom_setValue);
	delay(1000);
	CPU_RESTART;
}