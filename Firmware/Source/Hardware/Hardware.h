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

/* Includes */

#include "Camera/Camera.h"
#include "Touchscreen/Touchscreen.h"
#include "Display/Display.h"
#include "Battery.h"
#include "MLX90614.h"
#include "Lepton.h"
#include "SD.h"
#include "MassStorage.h"
#include "Connection.h"

/* Methods */

/* Converts a float to four bytes */
void floatToBytes(uint8_t* farray, float val)
{
	union {
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
float bytesToFloat(uint8_t* farray)
{
	union {
		float f;
		unsigned long ul;
	} u;
	u.ul = (farray[3] << 24) | (farray[2] << 16)
		| (farray[1] << 8) | (farray[0]);
	return u.f;
}

/* Switch the SPI clockline to pin 14 */
void startAltClockline(boolean sdStart) {
	CORE_PIN13_CONFIG = PORT_PCR_MUX(1);
	CORE_PIN14_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
	if (sdStart)
		sd.begin(pin_sd_cs, SPI_FULL_SPEED);
}

/* Switch the SPI clockline back to pin 13 */
void endAltClockline() {
	CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
	CORE_PIN14_CONFIG = PORT_PCR_MUX(1);
}

/* Checks if the sd card is inserted for ThermocamV4 */
boolean checkSDCard() {
	//ThermocamV4 or DIY-Thermocam V2 - try init SD card
	if ((mlx90614Version == mlx90614Version_old) ||
		(teensyVersion == teensyVersion_new))
		return beginSD();

	//All other do not need the check
	return true;
}

/* Reads the adjust combined settings from EEPROM */
void readAdjustCombined() {
	//Adjust combined selection
	byte adjCombPreset;
	byte read = EEPROM.read(eeprom_adjCombPreset);
	if ((read >= adjComb_preset1) && (read <= adjComb_preset3))
		adjCombPreset = read;
	else
		adjCombPreset = adjComb_temporary;
	//Adjust combined preset 1
	if ((adjCombPreset == adjComb_preset1) && (EEPROM.read(eeprom_adjComb1Set) == eeprom_setValue)) {
		adjCombDown = EEPROM.read(eeprom_adjComb1Down);
		adjCombLeft = EEPROM.read(eeprom_adjComb1Left);
		adjCombRight = EEPROM.read(eeprom_adjComb1Right);
		adjCombUp = EEPROM.read(eeprom_adjComb1Up);
		adjCombAlpha = EEPROM.read(eeprom_adjComb1Alpha) / 100.0;
		adjCombFactor = EEPROM.read(eeprom_adjComb1Factor) / 100.0;
	}
	//Adjust combined preset 2
	else if ((adjCombPreset == adjComb_preset2) && (EEPROM.read(eeprom_adjComb2Set) == eeprom_setValue)) {
		adjCombDown = EEPROM.read(eeprom_adjComb2Down);
		adjCombLeft = EEPROM.read(eeprom_adjComb2Left);
		adjCombRight = EEPROM.read(eeprom_adjComb2Right);
		adjCombUp = EEPROM.read(eeprom_adjComb2Up);
		adjCombAlpha = EEPROM.read(eeprom_adjComb2Alpha) / 100.0;
		adjCombFactor = EEPROM.read(eeprom_adjComb2Factor) / 100.0;
	}
	//Adjust combined preset 3
	else if ((adjCombPreset == adjComb_preset3) && (EEPROM.read(eeprom_adjComb3Set) == eeprom_setValue)) {
		adjCombDown = EEPROM.read(eeprom_adjComb3Down);
		adjCombLeft = EEPROM.read(eeprom_adjComb3Left);
		adjCombRight = EEPROM.read(eeprom_adjComb3Right);
		adjCombUp = EEPROM.read(eeprom_adjComb3Up);
		adjCombAlpha = EEPROM.read(eeprom_adjComb3Alpha) / 100.0;
		adjCombFactor = EEPROM.read(eeprom_adjComb3Factor) / 100.0;
	}
	//Load defaults
	else {
		adjCombDown = 0;
		adjCombUp = 0;
		adjCombLeft = 0;
		adjCombRight = 0;
		adjCombAlpha = 0.5;
		adjCombFactor = 1.0;
	}
	//Set factor to standard if invalid
	if ((adjCombFactor < 0.7) || (adjCombFactor > 1.0))
		adjCombFactor = 1.0;
}


/* Clears the whole EEPROM */
void clearEEPROM() {
	for (unsigned int i = 0; i < EEPROM.length(); i++)
		EEPROM.write(i, 0);
}

/* Checks if a FW upgrade has been done */
void checkFWUpgrade() {
	//If the first start setup has not been completed, skip
	if (checkFirstStart())
		return;

	//Read current FW version from EEPROM 
	byte eepromVersion = EEPROM.read(eeprom_fwVersion);

	//Show message after firmware upgrade
	if (eepromVersion != fwVersion) {
		//Upgrade from old Thermocam-V4 firmware
		if ((mlx90614Version == mlx90614Version_old) && (EEPROM.read(eeprom_liveHelper) != eeprom_setValue)) {
			//Clear EEPROM
			clearEEPROM();
			//Show message and wait
			showFullMessage((char*)"FW update completed, pls restart!");
			while (true);
		}
		//Upgrade
		if (fwVersion > eepromVersion) {
			//If coming from a firmware version smaller than 2.00, clear EEPROM
			if (eepromVersion < 200)
				clearEEPROM();

			//Clear adjust combined settings when coming from FW smaller than 2.13
			if (eepromVersion < 213) {
				EEPROM.write(eeprom_adjCombPreset, adjComb_temporary);
				EEPROM.write(eeprom_adjComb1Set, 0);
				EEPROM.write(eeprom_adjComb2Set, 0);
				EEPROM.write(eeprom_adjComb3Set, 0);
			}
			//Show upgrade completed message
			showFullMessage((char*)"Update completed, restart device!");
			//Set EEPROM firmware version to current one
			EEPROM.write(eeprom_fwVersion, fwVersion);
			//Wait for hard-reset
			while (true);
		}

		//Show downgrade completed message
		showFullMessage((char*)"Downgrade completed, restart device!");
		//Set EEPROM firmware version to current one
		EEPROM.write(eeprom_fwVersion, fwVersion);
		//Wait for hard-reset
		while (true);
	}
}

/* Reads the calibration slope from EEPROM */
void readCalibration() {
	uint8_t farray[4];
	//Read slope
	for (int i = 0; i < 4; i++)
		farray[i] = EEPROM.read(eeprom_calSlopeBase + i);
	calSlope = bytesToFloat(farray);
}

/* Reads the old settings from EEPROM */
void readEEPROM() {
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
	//Visual Enabled, only enable if camera is connected
	read = EEPROM.read(eeprom_visualEnabled);
	if (((read == false) || (read == true)) && checkDiagnostic(diag_camera))
		visualEnabled = read;
	else
		visualEnabled = false;
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
	if (((read == false) || (read == true)) && checkDiagnostic(diag_spot))
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
	//Display Mode, only load when camera is connected
	read = EEPROM.read(eeprom_displayMode);
	if (((read == displayMode_thermal) || (read == displayMode_visual) || (read == displayMode_combined)) && checkDiagnostic(diag_camera))
		displayMode = read;
	else
		displayMode = displayMode_thermal;
	//Text color
	read = EEPROM.read(eeprom_textColor);
	if ((read >= textColor_white) && (read <= textColor_blue))
		textColor = read;
	else
		textColor = textColor_white;
	//Hot / cold mode
	read = EEPROM.read(eeprom_hotColdMode);
	if ((read >= hotColdMode_disabled) && (read <= hotColdMode_hot))
		hotColdMode = read;
	else
		hotColdMode = hotColdMode_disabled;
	//Hot / cold level and color
	if (hotColdMode != hotColdMode_disabled) {
		hotColdLevel = ((EEPROM.read(eeprom_hotColdLevelHigh) << 8) + EEPROM.read(eeprom_hotColdLevelLow));
		hotColdColor = EEPROM.read(eeprom_hotColdColor);
	}
	//Calibration slope
	read = EEPROM.read(eeprom_calSlopeSet);
	if (leptonVersion == leptonVersion_2_5_shutter)
		calSlope = 0.01;
	else if (read == eeprom_setValue)
		readCalibration();
	else
		calSlope = cal_stdSlope;
	//Min/Max Points
	read = EEPROM.read(eeprom_minMaxPoints);
	if ((read == minMaxPoints_disabled) || (read == minMaxPoints_min) || (read == minMaxPoints_max) || (read == minMaxPoints_both))
		minMaxPoints = read;
	else
		minMaxPoints = minMaxPoints_disabled;
	//HQ res, V2 only
	if(teensyVersion == teensyVersion_new)
	{
		read = EEPROM.read(eeprom_hqRes);
		if ((read == false) || (read == true))
			hqRes = read;
		else
			hqRes = true;

	}
	//Align combined settings
	readAdjustCombined();
}


/* Checks the specific device from the diagnostic variable */
bool checkDiagnostic(byte device) {
	//Returns false if the device does not work
	return (diagnostic >> device) & 1;
}

/* Sets the status of a specific device from the diagnostic variable */
void setDiagnostic(byte device) {
	diagnostic &= ~(1 << device);
}

/* Checks for hardware issues */
void checkDiagnostic() {
	//When returning from mass storage, do not check
	if (EEPROM.read(eeprom_massStorage) == eeprom_setValue)
	{
		EEPROM.write(eeprom_massStorage, 0);
		diagnostic = diag_ok;
	}

	//If the diagnostic is not okay show info
	if (diagnostic != diag_ok) 
		showDiagnostic();

	//If there is a problem with the visual camera, switch to thermal
	if (!checkDiagnostic(diag_camera)) {
		//We switch to thermal mode
		displayMode = displayMode_thermal;
		//And disable the visual image save
		visualEnabled = false;
	}
}

/* Stores the current calibration to EEPROM */
void storeCalibration() {
	uint8_t farray[4];
	//Store slope
	floatToBytes(farray, (float)calSlope);
	for (int i = 0; i < 4; i++)
		EEPROM.write(eeprom_calSlopeBase + i, (farray[i]));
	EEPROM.write(eeprom_calSlopeSet, eeprom_setValue);
	//Set calibration to manual
	calStatus = cal_manual;
}

/* A method to check if the touch screen is pressed */
boolean touchScreenPressed() {
	//Check button status with debounce
	touchDebouncer.update();
	return touchDebouncer.read();
}

/* A method to check if the external button is pressed */
boolean extButtonPressed() {
	//Check button status with debounce
	buttonDebouncer.update();
	return buttonDebouncer.read();
}

/* Initialize the GPIO pins */
void initGPIO() {
	//Deactivate the laser for old HW
	if (teensyVersion == teensyVersion_old) {
		pinMode(pin_laser, OUTPUT);
		digitalWrite(pin_laser, LOW);
		laserEnabled = false;
	}
	//Set the touch IRQ pin to input
	pinMode(pin_touch_irq, INPUT);
	//For Teensy 3.6, activate internal pulldown for button
	if (teensyVersion == teensyVersion_new)
		pinMode(pin_button, INPUT_PULLDOWN);
	//Set button as input for Teensy 3.1/3.2
	else
		pinMode(pin_button, INPUT);
	//Activate display backlight
	pinMode(pin_lcd_backlight, OUTPUT);
	digitalWrite(pin_lcd_backlight, HIGH);
}

/* Disables all Chip-Select lines on the SPI bus */
void initSPI() {
	pinMode(pin_lcd_dc, OUTPUT);
	pinMode(pin_touch_cs, OUTPUT);
	pinMode(pin_lepton_cs, OUTPUT);
	pinMode(pin_sd_cs, OUTPUT);
	pinMode(pin_lcd_cs, OUTPUT);
	pinMode(pin_cam_cs, OUTPUT);
	digitalWrite(pin_lcd_dc, HIGH);
	digitalWrite(pin_touch_cs, HIGH);
	digitalWrite(pin_lepton_cs, HIGH);
	digitalWrite(pin_sd_cs, HIGH);
	digitalWrite(pin_lcd_cs, HIGH);
	digitalWrite(pin_cam_cs, HIGH);
	SPI.begin();
}

/* Inits the I2C Bus */
void initI2C() {
	//Start the Bus
	Wire.begin();
	//Enable Timeout for Hardware start
	Wire.setDefaultTimeout(1000);
	//Use external pullups for new HW
	if (teensyVersion == teensyVersion_new)
		Wire.pinConfigure(I2C_PINS_18_19, I2C_PULLUP_EXT);
	//Use internal pullups for old HW
	else
		Wire.pinConfigure(I2C_PINS_18_19, I2C_PULLUP_INT);
}

/* Init the Analog-Digital-Converter for the battery measure */
void initADC() {
	//Init ADC
	batMeasure = new ADC();
	//set number of averages
	batMeasure->setAveraging(4);
	//set bits of resolution
	batMeasure->setResolution(12);
	//change the conversion speed
	batMeasure->setConversionSpeed(ADC_MED_SPEED);
	//change the sampling speed
	batMeasure->setSamplingSpeed(ADC_MED_SPEED);
	//set battery pin as input
	pinMode(pin_bat_measure, INPUT);
}

/* Init the buffer(s) */
void initBuffer()
{
	//For Teensy 3.6, init 320x240 buffer
	if (teensyVersion == teensyVersion_new)
		bigBuffer = (uint16_t*)malloc(153600);

	//Init 160x120 buffer for all devices
	smallBuffer = (uint16_t*)malloc(38400);
}

/* Display the content of the small/big buffer on the screen */
void displayBuffer()
{
	//Display 320x240 for Teensy 3.6
	if ((teensyVersion == teensyVersion_new) && (hqRes))
		display_writeScreen(bigBuffer, 0);
	//160x120 for Teensy 3.1 / 3.2
	else
		display_writeScreen(smallBuffer, 1);
}

/* Detect which teensy version is used */
void detectTeensyVersion()
{
	//Teensy 3.6 used in the DIY-Thermocam V2
#if defined(__MK66FX1M0__)
	teensyVersion = teensyVersion_new;
	//Teensy 3.1 / 3.2 used in the old hardware generations
#elif defined(__MK20DX256__)
	teensyVersion = teensyVersion_old;
#endif

	//Set hardware version to detect V1/V2 over EEPROM
	EEPROM.write(eeprom_teensyVersion, teensyVersion);
}

/* Sets the display rotation depending on the setting */
void setDisplayRotation() {
	if (rotationEnabled) {
		display_setRotation(135);
		touch_setRotation(true);
	}
	else {
		display_setRotation(45);
		touch_setRotation(false);
	}
}

/* Reads the temperature limits from EEPROM */
void readTempLimits() {
	int16_t min, max;
	//Min / max selection
	byte minMaxPreset;
	byte read = EEPROM.read(eeprom_minMaxPreset);
	if ((read >= minMax_preset1) && (read <= minMax_preset3))
		minMaxPreset = read;
	else
		minMaxPreset = minMax_temporary;
	//Min / max preset 1
	if ((minMaxPreset == minMax_preset1) && (EEPROM.read(eeprom_minMax1Set) == eeprom_setValue)) {
		min = ((EEPROM.read(eeprom_minValue1High) << 8) + EEPROM.read(eeprom_minValue1Low));
		max = ((EEPROM.read(eeprom_maxValue1High) << 8) + EEPROM.read(eeprom_maxValue1Low));
		minValue = tempToRaw(min);
		maxValue = tempToRaw(max);
	}
	//Min / max preset 2
	else if ((minMaxPreset == minMax_preset2) && (EEPROM.read(eeprom_minMax2Set) == eeprom_setValue)) {
		min = ((EEPROM.read(eeprom_minValue2High) << 8) + EEPROM.read(eeprom_minValue2Low));
		max = ((EEPROM.read(eeprom_maxValue2High) << 8) + EEPROM.read(eeprom_maxValue2Low));
		minValue = tempToRaw(min);
		maxValue = tempToRaw(max);

	}
	//Min / max preset 3
	else if ((minMaxPreset == minMax_preset3) && (EEPROM.read(eeprom_minMax3Set) == eeprom_setValue)) {
		min = ((EEPROM.read(eeprom_minValue3High) << 8) + EEPROM.read(eeprom_minValue3Low));
		max = ((EEPROM.read(eeprom_maxValue3High) << 8) + EEPROM.read(eeprom_maxValue3Low));
		minValue = tempToRaw(min);
		maxValue = tempToRaw(max);
	}
}

/* Init the screen off timer */
void initScreenOffTimer() {
	byte read = EEPROM.read(eeprom_screenOffTime);
	//Try to read from EEPROM
	if ((read == screenOffTime_disabled) || (read == screenOffTime_5min) || read == screenOffTime_20min) {
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
void initRTC() {
	//Get the time from the Teensy
	setSyncProvider(getTeensy3Time);

	//Check if year is lower than 2017
	if ((year() < 2017) && (EEPROM.read(eeprom_firstStart) == eeprom_setValue)) {
		showFullMessage((char*) "Empty coin cell battery!");
		delay(1000);
		setTime(0, 0, 0, 1, 1, 2017);
		Teensy3Clock.set(now());
	}
}

/* Disable the screen backlight */
void disableScreenLight() {
	digitalWrite(pin_lcd_backlight, LOW);
}

/* Enables the screen backlight */
void enableScreenLight() {
	digitalWrite(pin_lcd_backlight, HIGH);
}

/* Checks if the screen backlight is on or off*/
bool checkScreenLight() {
	return digitalRead(pin_lcd_backlight);
}

/* Disable automatic FFC when saved in EEPROM */
void checkNoFFC()
{
	//Set value found
	if (EEPROM.read(eeprom_noShutter) == eeprom_setValue){
		//Disable auto FFC mode
		lepton_ffcMode(false);

		//Set lepton shutter to none
		leptonShutter = leptonShutter_none;
	}
}

//Get the spot temperature from Lepton or MLX90614
void getSpotTemp() {
	//Get spot value from radiometric Lepton
	if (leptonVersion == leptonVersion_2_5_shutter)
		spotTemp = lepton_spotTemp();

	//Get temperature from MLX90614
	else
		spotTemp = mlx90614_getTemp();

	//Convert to Fahrenheit if required
	if (tempFormat == tempFormat_fahrenheit)
		spotTemp = celciusToFahrenheit(spotTemp);
}

/* Switches the laser on or off*/
void toggleLaser(bool message) {
	//Thermocam V4 or DIY-Thermocam V2 does not support this
	if ((mlx90614Version == mlx90614Version_old) || (teensyVersion == teensyVersion_new))
		return;

	//Laser enabled, switch off
	if (laserEnabled) {
		digitalWrite(pin_laser, LOW);
		laserEnabled = false;
		if (message) {
			showFullMessage((char*) "Laser is now off!", true);
			delay(1000);
		}
	}
	//Laser disabled, switch on
	else {
		digitalWrite(pin_laser, HIGH);
		laserEnabled = true;
		if (message) {
			showFullMessage((char*) "Laser is now on!", true);
			delay(1000);
		}
	}
}

/* Toggle the display*/
void toggleDisplay() {
	showFullMessage((char*) "Screen goes off, touch to continue!", true);
	delay(1000);
	disableScreenLight();
	//Wait for touch press
	while (!touch_touched());
	//Turning screen on
	drawMainMenuBorder();
	showFullMessage((char*) "Turning screen on..", true);
	enableScreenLight();
	delay(1000);
}

/* Check if the screen was pressed in the time period */
bool screenOffCheck() {
	//Timer exceeded
	if ((screenOff.check()) && (screenOffTime != screenOffTime_disabled)) {
		//No touch press in the last interval
		if (screenPressed == false) {
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
	//Detect teensy version
	detectTeensyVersion();
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
	//Init camera
	camera_init();
	//Init lepton
	lepton_init();
	//Init spot sensor
	mlx90614_init();
	//Init SD card
	initSD();
	//Disable I2C timeout
	Wire.setDefaultTimeout(0);
	//Init screen off timer
	initScreenOffTimer();
	//Init the buffer(s)
	initBuffer();
	//Wait two more seconds for FFC to complete
	bootFFC();
	//Check battery for the first time
	checkBattery(true);
	//Init the realtime clock
	initRTC();
}