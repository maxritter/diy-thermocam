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
#include <MTP.h>
#include <LittleFS.h>
#include <TimeLib.h>

/*################# DATA TYPES, CONSTANTS & MACRO DEFINITIONS #################*/

#define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
#define CPU_RESTART_VAL 0x5FA0004
#define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL);

const char *sd_str[] = {"sdio", "sd1"};
const int cs[] = {BUILTIN_SDCARD, 10};
const int nsd = sizeof(sd_str) / sizeof(const char *);
SDClass sdx[nsd];
MTPStorage_SD storage;
MTPD mtpd(&storage);

/*######################## PUBLIC FUNCTION BODIES #############################*/

extern "C" int usb_init_events(void);

void dateTime(uint16_t *date, uint16_t *time, uint8_t *ms10)
{
	*date = FS_DATE(year(), month(), day());
	*time = FS_TIME(hour(), minute(), second());
	*ms10 = second() & 1 ? 100 : 0;
}

void storage_configure()
{
	for (int ii = 0; ii < nsd; ii++)
	{
		if (cs[ii] == BUILTIN_SDCARD)
		{
			if (sdx[ii].sdfs.begin(SdioConfig(FIFO_SDIO)))
			{
				storage.addFilesystem(sdx[ii], sd_str[ii]);
			}
		}
	}
}

void enterMassStorage()
{
	//Only do this if the EEPROM marker is set
	if ((EEPROM.read(eeprom_massStorage) == eeprom_setValue))
	{
		EEPROM.write(eeprom_massStorage, 0);
		showFullMessage((char *)"USB File Transfer, touch to exit!");
		usb_init_events();
		storage_configure();
		FsDateTime::callback = dateTime;

		//Do MTP until user wants to exit via touch
		while (true)
		{
			mtpd.loop();
			if (touch_touched() || !isUSBConnected())
				break;
		}

		showFullMessage((char *)"Exiting File Transfer mode..");
		delay(1000);
		while (touch_touched())
			;
		sd.begin(SdioConfig(FIFO_SDIO));
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

void checkMassStorage()
{
	if (!usbConnected && isUSBConnected())
	{
		detachInterrupt(pin_touch_irq);

		//Check if the user really wants to enter MTP
		if (massStoragePrompt())
		{
			setMassStorage();
		}
		
		attachInterrupt(pin_touch_irq, touchIRQ, FALLING);
		usbConnected = true;
	}
	else if (usbConnected && !isUSBConnected())
	{
		usbConnected = false;
	}
}