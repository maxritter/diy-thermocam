/*
*
* SD Card - Methods to access the internal SD storage
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
#include <SdFat.h>
#include <hardware.h>
#include <TimeLib.h>
#include <sdcard.h>

/*######################### STATIC DATA DECLARATIONS ##########################*/

uint32_t cardSectorCount = 0;
uint8_t sectorBuffer[512];
SdCardFactory cardFactory;
SdCard *m_card = nullptr;
uint32_t const ERASE_SIZE = 262144L;

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Returns the free space on the card in KB */
uint32_t getSDSpace()
{
	return sd.freeClusterCount() * sd.sectorsPerCluster() * 0.000512;
}

/* Returns the full card size in MB */
uint32_t getCardSize()
{
	return sd.clusterCount() * sd.sectorsPerCluster() * 0.000512;
}

//Refresh free space of the internal space
void refreshFreeSpace()
{
	uint16_t cardSize = getCardSize();
	if (cardSize != 0)
		sdInfo = String(getSDSpace()) + "/" + String(cardSize) + " MB";
}

/* Timestamp set for SDFat */
void dateTime(uint16_t *date, uint16_t *time)
{
	// return date using FAT_DATE macro to format fields
	*date = FAT_DATE(year(), month(), day());
	// return time using FAT_TIME macro to format fields
	*time = FAT_TIME(hour(), minute(), second());
}

/* Begin the SD card */
bool beginSD()
{
	return sd.begin(SdioConfig(FIFO_SDIO));
}

/* Initializes the SD card */
void initSD()
{
	//Storage info string
	sdInfo = " -  /  -  MB";

	//Check if the sd card works
	if (!beginSD())
		setDiagnostic(diag_sd);		

	refreshFreeSpace();
	SdFile::dateTimeCallback(dateTime);
}

/* Erase Card before formatting */
bool eraseCard()
{
	uint32_t firstBlock = 0;
	uint32_t lastBlock;
	uint16_t n = 0;

	do
	{
		lastBlock = firstBlock + ERASE_SIZE - 1;
		if (lastBlock >= cardSectorCount)
		{
			lastBlock = cardSectorCount - 1;
		}
		if (!m_card->erase(firstBlock, lastBlock))
		{
			return false;
		}
		if ((n++) % 64 == 63)
		{
		}
		firstBlock += ERASE_SIZE;
	} while (firstBlock < cardSectorCount);

	if (!m_card->readSector(0, sectorBuffer))
	{
		return false;
	}

	return true;
}

/* Format FAT card */
bool formatCard()
{	
	m_card = cardFactory.newCard(SdioConfig(FIFO_SDIO));

	if (!m_card || m_card->errorCode())
	{
		return false;
	}

	cardSectorCount = m_card->sectorCount();
	if (!cardSectorCount)
	{
		return false;
	}

	//exFAT is not supported
	if (cardSectorCount > 67108864)
		return false;

	//First try to erase card
	if (eraseCard())
	{
		//Then format it
		FatFormatter fatFormatter;
		bool ret = fatFormatter.format(m_card, sectorBuffer, &Serial);
		if (!ret)
			return false;
		return true;
	}
	return false;
}