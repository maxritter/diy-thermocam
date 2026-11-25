/*
 * SD Card - Internal SDIO storage via SdFs (FAT16 / FAT32 / exFAT >32 GB, #110)
 */

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <SdFat.h>
#include <hardware.h>
#include <TimeLib.h>
#include <sdcard.h>

static uint32_t s_sd_total_kb = 0;
static uint32_t s_sd_free_kb = 0;
static bool s_sd_mounted = false;

static void dateTimeEx(uint16_t* date, uint16_t* time, uint8_t* ms10) {
	*date = FS_DATE(year(), month(), day());
	*time = FS_TIME(hour(), minute(), second());
	*ms10 = (second() & 1) ? 100 : 0;
}

void dateTime(uint16_t* date, uint16_t* time) {
	*date = FS_DATE(year(), month(), day());
	*time = FS_TIME(hour(), minute(), second());
}

bool beginSD() {
	s_sd_mounted = sd.begin(SdioConfig(FIFO_SDIO));
	return s_sd_mounted;
}

uint8_t getSDFsType() {
	if (!s_sd_mounted || !checkDiagnostic(diag_sd)) return 0;
	switch (sd.fatType()) {
	case FAT_TYPE_FAT16: return 1;
	case FAT_TYPE_FAT32: return 2;
	case FAT_TYPE_EXFAT: return 3;
	default: return 0;
	}
}

uint32_t getSDTotalKB() {
	return s_sd_total_kb;
}

uint32_t getSDFreeKB() {
	return s_sd_free_kb;
}

void sdNoteBytesWritten(uint32_t bytes) {
	uint32_t kb = (bytes + 1023u) / 1024u;
	if (s_sd_free_kb > kb) s_sd_free_kb -= kb;
	else s_sd_free_kb = 0;
}

uint32_t getSDSpace() {
	if (!s_sd_mounted) return 0;
	if (s_sd_free_kb > 0) return s_sd_free_kb;
	int32_t free_cl = sd.freeClusterCount();
	if (free_cl < 0) return 0;
	uint64_t kb = (static_cast<uint64_t>(free_cl) * static_cast<uint64_t>(sd.sectorsPerCluster())) >> 1;
	s_sd_free_kb = (kb > 0xFFFFFFFFULL) ? 0xFFFFFFFFu : static_cast<uint32_t>(kb);
	return s_sd_free_kb;
}

uint32_t getCardSize() {
	if (!s_sd_mounted) return 0;
	uint64_t kb = (static_cast<uint64_t>(sd.clusterCount()) * static_cast<uint64_t>(sd.sectorsPerCluster())) >> 1;
	s_sd_total_kb = (kb > 0xFFFFFFFFULL) ? 0xFFFFFFFFu : static_cast<uint32_t>(kb);
	return s_sd_total_kb / 1024u; // MB
}

void refreshFreeSpace() {
	if (!s_sd_mounted) {
		s_sd_total_kb = 0;
		s_sd_free_kb = 0;
		sdInfo = " -  /  -  MB";
		return;
	}
	uint32_t totalMB = getCardSize();
	int32_t free_cl = sd.freeClusterCount();
	if (free_cl >= 0) {
		uint64_t kb = (static_cast<uint64_t>(free_cl) * static_cast<uint64_t>(sd.sectorsPerCluster())) >> 1;
		s_sd_free_kb = (kb > 0xFFFFFFFFULL) ? 0xFFFFFFFFu : static_cast<uint32_t>(kb);
	}
	uint32_t freeMB = s_sd_free_kb / 1024u;
	if (totalMB != 0) {
		sdInfo = String(freeMB) + "/" + String(totalMB) + " MB";
	}
}

void initSD() {
	sdInfo = " -  /  -  MB";
	if (!beginSD()) {
		setDiagnostic(diag_sd);
	}
	refreshFreeSpace();
	FsDateTime::setCallback(dateTimeEx);
}

bool formatCard() {
	SdCardFactory cardFactory;
	SdCard* m_card = cardFactory.newCard(SdioConfig(FIFO_SDIO));
	if (!m_card || m_card->errorCode()) return false;
	uint32_t sectors = m_card->sectorCount();
	if (!sectors) return false;

	uint8_t sectorBuffer[512];
	bool ok = false;
	if (sectors > 67108864u) {
		ExFatFormatter exFmt;
		ok = exFmt.format(m_card, sectorBuffer, &Serial);
	} else {
		FatFormatter fatFmt;
		ok = fatFmt.format(m_card, sectorBuffer, &Serial);
	}
	if (ok) {
		beginSD();
		refreshFreeSpace();
	}
	return ok;
}
