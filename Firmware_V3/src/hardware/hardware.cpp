/*
 *
 * HARDWARE - Main hardware functions
 *
 * DIY-Thermocam Firmware
 *
 * GNU General Public License v3.0
 *
 * Copyright by Max Ritter
 *
 * http://www.diy-thermocam.net
 * https://github.com/maxritter/DIY-Thermocam
 *
 */

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <SdFat.h>
#include <sdcard.h>
#include <gui.h>
#include <settings_defines.h>
#include <EEPROM.h>
#include <display.h>
#include <massstorage.h>
#include <TimeLib.h>
#include <touchscreen.h>
#include <Wire.h>
#include <battery.h>
#include <temperature.h>
#include <lepton.h>
#include <firststart.h>
#include <hardware.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

void t4_direct_write_low(volatile uint32_t *base, uint32_t mask)
{
	*(base + 34) = mask;
}

void t4_direct_write_high(volatile uint32_t *base, uint32_t mask)
{
	*(base + 33) = mask;
}

bool isUSBConnected()
{
	return (analogRead(pin_usb_measure) > 50);
}

/* Converts a float to four bytes */
void floatToBytes(uint8_t *farray, float val)
{
	union
	{
		float f;
		unsigned long ul;
	} u;
	u.f = val;
	farray[0] = u.ul & 0x00FF;
	farray[1] = (u.ul & 0xFF00) >> 8;
	farray[2] = (u.ul & 0xFF0000) >> 16;
	farray[3] = (u.ul & 0xFF000000) >> 24;
}

/* Converts four bytes back to float */
float bytesToFloat(uint8_t *farray)
{
	union
	{
		float f;
		unsigned long ul;
	} u;
	u.ul = (farray[3] << 24) | (farray[2] << 16) | (farray[1] << 8) | (farray[0]);
	return u.f;
}

/* Clears the whole EEPROM */
void clearEEPROM()
{
	for (unsigned int i = 100; i < 250; i++)
		EEPROM.write(i, 0);
}

/* Checks if a FW upgrade has been done */
void checkFWUpgrade()
{
	//If the first start setup has not been completed, skip
	if (checkFirstStart())
		return;

	//Read current FW version from EEPROM
	uint16_t eepromVersion = ((EEPROM.read(eeprom_fwVersionHigh) << 8) + EEPROM.read(eeprom_fwVersionLow));

	//Show message after firmware upgrade
	if (eepromVersion != fwVersion)
	{
		//Set EEPROM firmware version to current one
		EEPROM.write(eeprom_fwVersionHigh, (fwVersion & 0xFF00) >> 8);
		EEPROM.write(eeprom_fwVersionLow, fwVersion & 0x00FF);

		//Show downgrade completed message
		showFullMessage((char *)"Firmware update completed!");
		delay(1000);
	}
}

/* Reads the old settings from EEPROM */
void readEEPROM()
{
	byte read;
	//Temperature format
	read = EEPROM.read(eeprom_tempFormat);
	if ((read == tempFormat_celcius) || (read == tempFormat_fahrenheit))
		tempFormat = read;
	else
		tempFormat = tempFormat_celcius;

	//Color scheme
	read = EEPROM.read(eeprom_colorScheme);
	if ((read >= 0) && (read <= (colorSchemeTotal - 1)))
		colorScheme = read;
	else
		colorScheme = colorScheme_rainbow;

	//Convert Enabled
	read = EEPROM.read(eeprom_convertEnabled);
	if ((read == false) || (read == true))
		convertEnabled = read;
	else
		convertEnabled = false;

	//Battery Enabled
	read = EEPROM.read(eeprom_batteryEnabled);
	if ((read == false) || (read == true))
		batteryEnabled = read;
	else
		batteryEnabled = false;

	//Time Enabled
	read = EEPROM.read(eeprom_timeEnabled);
	if ((read == false) || (read == true))
		timeEnabled = read;
	else
		timeEnabled = false;

	//Date Enabled
	read = EEPROM.read(eeprom_dateEnabled);
	if ((read == false) || (read == true))
		dateEnabled = read;
	else
		dateEnabled = false;

	//Storage Enabled
	read = EEPROM.read(eeprom_storageEnabled);
	if ((read == false) || (read == true))
		storageEnabled = read;
	else
		storageEnabled = false;

	//Spot Enabled, only load when spot sensor is working
	read = EEPROM.read(eeprom_spotEnabled);
	if ((read == false) || (read == true))
		spotEnabled = read;
	else
		spotEnabled = false;
		
	//Filter Type
	read = EEPROM.read(eeprom_filterType);
	if ((read == filterType_none) || (read == filterType_box) || (read == filterType_gaussian))
		filterType = read;
	else
		filterType = filterType_gaussian;

	//Colorbar Enabled
	read = EEPROM.read(eeprom_colorbarEnabled);
	if ((read == false) || (read == true))
		colorbarEnabled = read;
	else
		colorbarEnabled = true;

	//Text color
	read = EEPROM.read(eeprom_textColor);
	if ((read >= textColor_white) && (read <= textColor_blue))
		textColor = read;
	else
		textColor = textColor_white;

	//Horizontal mirroring
	read = EEPROM.read(eeprom_rotationHorizont);
	if ((read == false) || (read == true))
		rotationHorizont = read;
	else
		rotationHorizont = false;

	//Hot / cold mode
	read = EEPROM.read(eeprom_hotColdMode);
	if ((read >= hotColdMode_disabled) && (read <= hotColdMode_hot))
		hotColdMode = read;
	else
		hotColdMode = hotColdMode_disabled;

	//Hot / cold level and color
	if (hotColdMode != hotColdMode_disabled)
	{
		hotColdLevel = ((EEPROM.read(eeprom_hotColdLevelHigh) << 8) + EEPROM.read(eeprom_hotColdLevelLow));
		hotColdColor = EEPROM.read(eeprom_hotColdColor);
	}

	//Min/Max Points
	read = EEPROM.read(eeprom_minMaxPoints);
	if ((read == minMaxPoints_disabled) || (read == minMaxPoints_min) || (read == minMaxPoints_max) || (read == minMaxPoints_both))
		minMaxPoints = read;
	else
		minMaxPoints = minMaxPoints_disabled;

	//Gain Mode
	read = EEPROM.read(eeprom_lepton_gain);
	if (read == lepton_gain_high)
	{
		lepton_setHighGain();
	}
	else if (read == lepton_gain_low)
	{
		lepton_setLowGain();
	}
	else
	{
		lepton_setHighGain();
	}
}

/* Checks the specific device from the diagnostic variable */
bool checkDiagnostic(byte device)
{
	//Returns false if the device does not work
	return (diagnostic >> device) & 1;
}

/* Sets the status of a specific device from the diagnostic variable */
void setDiagnostic(byte device)
{
	diagnostic &= ~(1 << device);
}

/* Checks for hardware issues */
void checkHardware()
{
	//If the diagnostic is not okay show info
	if (diagnostic != diag_ok)
		showDiagnostic();
}

/* A method to check if the touch screen is pressed */
boolean touchScreenPressed()
{
	//Check button status with debounce
	touchDebouncer.update();
	return touchDebouncer.read();
}

/* A method to check if the external button is pressed */
boolean extButtonPressed()
{
	//Check button status with debounce
	buttonDebouncer.update();
	return buttonDebouncer.read();
}

/* Initialize the GPIO pins */
void initGPIO()
{
	pinMode(pin_touch_irq, INPUT);
	pinMode(pin_button, INPUT_PULLDOWN);
	pinMode(pin_lcd_backlight, OUTPUT);
	digitalWrite(pin_lcd_backlight, HIGH);
}

/* Disables all Chip-Select lines on the SPI bus */
void initSPI()
{
	pinMode(pin_lcd_dc, OUTPUT);
	pinMode(pin_touch_cs, OUTPUT);
	pinMode(pin_lepton_cs, OUTPUT);
	pinMode(pin_lcd_cs, OUTPUT);
	digitalWrite(pin_lcd_dc, HIGH);
	digitalWrite(pin_touch_cs, HIGH);
	digitalWrite(pin_lepton_cs, HIGH);
	digitalWrite(pin_lcd_cs, HIGH);

	SPI.setMOSI(pin_mosi);
	SPI.setMISO(pin_miso);
	SPI.setSCK(pin_sck);
	SPI.setCS(pin_lcd_dc);
	SPI.usingInterrupt(pin_lepton_vsync);
	SPI.begin();

	SPI1.setMOSI(pin_mosi1);
	SPI1.setMISO(pin_miso1);
	SPI1.setSCK(pin_sck1);
	SPI1.setCS(pin_lepton_cs);
	SPI1.begin();
}

/* Inits the I2C Bus */
void initI2C()
{
	Wire.begin();
}

/* Init the Analog-Digital-Converter for the battery measure */
void initADC()
{
	//Init ADC
	batMeasure = new ADC();
	//set number of averages
	batMeasure->adc0->setAveraging(4);
	//set bits of resolution
	batMeasure->adc0->setResolution(12);
	//change the conversion speed
	batMeasure->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED);
	//change the sampling speed
	batMeasure->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);
	//set battery pin as input
	pinMode(pin_bat_measure, INPUT);
}

/* Init the buffer(s) */
void initBuffer()
{
	//Init 320x240 buffer
	bigBuffer = (uint16_t *)malloc(153600);

	//Init 160x120 buffer
	smallBuffer = (uint16_t *)malloc(38400);
}

/* Display the content of the small/big buffer on the screen */
void displayBuffer()
{
	display_writeScreen(bigBuffer, 0);
}

/* Sets the display rotation depending on the setting */
void setDisplayRotation()
{
	if (rotationVert)
	{
		display_setRotation(135);
		touch_setRotation(true);
	}
	else
	{
		display_setRotation(45);
		touch_setRotation(false);
	}
}

/* Reads the temperature limits from EEPROM */
void readTempLimits()
{
	//Some variables to get started
	byte minValueHigh, minValueLow, maxValueHigh, maxValueLow, minMaxComp;
	bool found = false;

	//Min / max selection
	byte minMaxPreset;
	byte read = EEPROM.read(eeprom_minMaxPreset);
	if ((read >= minMax_preset1) && (read <= minMax_preset3))
		minMaxPreset = read;
	else
		minMaxPreset = minMax_temporary;

	//Min / max preset 1
	if ((minMaxPreset == minMax_preset1) && (EEPROM.read(eeprom_minMax1Set) == eeprom_setValue))
	{
		minValueHigh = eeprom_minValue1High;
		minValueLow = eeprom_minValue1Low;
		maxValueHigh = eeprom_maxValue1High;
		maxValueLow = eeprom_maxValue1Low;
		minMaxComp = eeprom_minMax1Comp;
		found = true;
	}
	//Min / max preset 2
	else if ((minMaxPreset == minMax_preset2) && (EEPROM.read(eeprom_minMax2Set) == eeprom_setValue))
	{
		minValueHigh = eeprom_minValue2High;
		minValueLow = eeprom_minValue2Low;
		maxValueHigh = eeprom_maxValue2High;
		maxValueLow = eeprom_maxValue2Low;
		minMaxComp = eeprom_minMax2Comp;
		found = true;
	}
	//Min / max preset 3
	else if ((minMaxPreset == minMax_preset3) && (EEPROM.read(eeprom_minMax3Set) == eeprom_setValue))
	{
		minValueHigh = eeprom_minValue3High;
		minValueLow = eeprom_minValue3Low;
		maxValueHigh = eeprom_maxValue3High;
		maxValueLow = eeprom_maxValue3Low;
		minMaxComp = eeprom_minMax3Comp;
		found = true;
	}

	//Apply settings
	if (found)
	{
		minValue =
			((EEPROM.read(minValueHigh) << 8) + EEPROM.read(minValueLow));
		maxValue =
			((EEPROM.read(maxValueHigh) << 8) + EEPROM.read(maxValueLow));
		for (int i = 0; i < 4; i++)
			EEPROM.read(minMaxComp + i);
		autoMode = false;
	}
}

/* Init the screen off timer */
void initScreenOffTimer()
{
	byte read = EEPROM.read(eeprom_screenOffTime);
	//Try to read from EEPROM
	if ((read == screenOffTime_disabled) || (read == screenOffTime_5min) || read == screenOffTime_20min)
	{
		screenOffTime = read;
		//10 Minutes
		if (screenOffTime == screenOffTime_5min)
			screenOff.begin(300000, false);
		//30 Minutes
		else if (screenOffTime == screenOffTime_20min)
			screenOff.begin(1200000, false);
		//Disable marker
		screenPressed = false;
	}
	else
		screenOffTime = screenOffTime_disabled;
}

/* Get time from the RTC */
time_t getTeensy3Time()
{
	return Teensy3Clock.get();
}

/* Init the time and correct it if required */
void initRTC()
{
	//Get the time from the Teensy
	setSyncProvider(getTeensy3Time);

	//Check if year is lower than 2021
	if ((year() < 2021) && (EEPROM.read(eeprom_firstStart) == eeprom_setValue))
	{
		showFullMessage((char *)"Empty coin cell battery");
		delay(1000);
		setTime(0, 0, 0, 1, 1, 2021);
		Teensy3Clock.set(now());
	}
}

/* Disable the screen backlight */
void disableScreenLight()
{
	digitalWrite(pin_lcd_backlight, LOW);
}

/* Enables the screen backlight */
void enableScreenLight()
{
	digitalWrite(pin_lcd_backlight, HIGH);
}

/* Checks if the screen backlight is on or off*/
bool checkScreenLight()
{
	return digitalRead(pin_lcd_backlight);
}

//Get the spot temperature from Lepton or MLX90614
void getSpotTemp()
{
	//Get spot value from radiometric Lepton
	if ((leptonVersion == leptonVersion_2_5_shutter) || (leptonVersion == leptonVersion_3_5_shutter))
		spotTemp = lepton_spotTemp();

	//Convert to Fahrenheit if required
	if (tempFormat == tempFormat_fahrenheit)
		spotTemp = celciusToFahrenheit(spotTemp);
}

/* Toggle the display*/
void toggleDisplay()
{
	showFullMessage((char *)"Screen goes off, touch to continue", true);
	delay(1000);
	disableScreenLight();
	//Wait for touch press
	while (!touch_touched())
		;
	//Turning screen on
	drawMainMenuBorder();
	showFullMessage((char *)"Turning screen on..", true);
	enableScreenLight();
	delay(1000);
}

/* Check if the screen was pressed in the time period */
bool screenOffCheck()
{
	//Timer exceeded
	if ((screenOff.check()) && (screenOffTime != screenOffTime_disabled))
	{
		//No touch press in the last interval
		if (screenPressed == false)
		{
			toggleDisplay();
			screenOff.reset();
			return true;
		}
		//Touch pressed, restart timer
		screenPressed = false;
		screenOff.reset();
		return false;
	}
	return false;
}

/* Init the hardware */
void initHardware()
{
	//Init UART
	Serial.begin(115200);

	//Init GPIO
	initGPIO();

	//Init SPI
	initSPI();

	//Init I2C
	initI2C();

	//Init ADC
	initADC();

	//Init display
	display_init();

	//Show bootscreen
	bootScreen();

	//Init touch
	touch_init();

	//Eventually enter MTP mode
	enterMassStorage();

	//Init lepton
	lepton_init();

	//Init SD card
	initSD();

	//Init screen off timer
	initScreenOffTimer();

	//Init the buffer(s)
	initBuffer();

	//Check battery for the first time
	checkBattery(true);

	//Init the realtime clock
	initRTC();

	//Wait some time for the Lepton to do the FFC
	delay(2000);
}
