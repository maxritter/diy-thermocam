/*
 *
 * LEPTON - Access the FLIR Lepton LWIR module
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
#include <hardware.h>
#include <SPI.h>
#include <i2c_t3.h>
#include <EEPROM.h>
#include <gui.h>
#include <lepton.h>

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Array to store one Lepton frame
static byte leptonFrame[164];

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Start Lepton SPI Transmission */
void lepton_begin()
{
	//Start alternative clock line, except for old HW
	if (mlx90614Version == mlx90614Version_new)
		startAltClockline();

	//For Teensy  3.1 / 3.2 and Lepton3 use this one
	if ((teensyVersion == teensyVersion_old)
			&& ((leptonVersion == leptonVersion_3_0_shutter)
					|| (leptonVersion == leptonVersion_3_5_shutter)))
		SPI.beginTransaction(SPISettings(30000000, MSBFIRST, SPI_MODE0));

	//Otherwise use 20 Mhz maximum and SPI mode 1
	else
		SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE1));

	//Start transfer  - CS LOW
	digitalWrite(pin_lepton_cs, LOW);
}

/* End Lepton SPI Transmission */
void lepton_end()
{
	//End transfer - CS HIGH
	digitalWriteFast(pin_lepton_cs, HIGH);

	//End SPI Transaction
	SPI.endTransaction();

	//End alternative clock line, except for old HW
	if (mlx90614Version == mlx90614Version_new)
		endAltClockline();
}

/* Reset the SPI bus to re-initiate Lepton communication */
void lepton_reset()
{
	lepton_end();
	delay(186);
	lepton_begin();
}

/* Store one package of 80 columns into RAM */
bool savePackage(byte line, byte segment)
{
	//Go through the video pixels for one video line
	for (int column = 0; column < 80; column++)
	{
		//Apply horizontal mirroring
		if (rotationHorizont)
			column = 79 - column;

		//Make a 16-bit rawvalue from the lepton frame
		uint16_t result = (uint16_t) (leptonFrame[2 * column + 4] << 8
				| leptonFrame[2 * column + 5]);

		//Discard horizontal mirroring
		if (rotationHorizont)
			column = 79 - column;

		//Invalid value, return
		if (result == 0)
		{
			return 0;
		}

		//Lepton2.x
		if ((leptonVersion != leptonVersion_3_0_shutter)
				&& (leptonVersion != leptonVersion_3_5_shutter))
		{
			//Rotated or old hardware version
			if (((mlx90614Version == mlx90614Version_old) && (!rotationVert))
					|| ((mlx90614Version == mlx90614Version_new)
							&& (rotationVert)))
			{
				smallBuffer[(line * 2 * 160) + (column * 2)] = result;
				smallBuffer[(line * 2 * 160) + (column * 2) + 1] = result;
				smallBuffer[(line * 2 * 160) + 160 + (column * 2)] = result;
				smallBuffer[(line * 2 * 160) + 160 + (column * 2) + 1] = result;
			}
			//Non-rotated
			else
			{
				smallBuffer[19199 - ((line * 2 * 160) + (column * 2))] = result;
				smallBuffer[19199 - ((line * 2 * 160) + (column * 2) + 1)] =
						result;
				smallBuffer[19199 - ((line * 2 * 160) + 160 + (column * 2))] =
						result;
				smallBuffer[19199 - ((line * 2 * 160) + 160 + (column * 2) + 1)] =
						result;
			}
		}

		//Lepton3
		else
		{
			//Non-rotated
			if (!rotationVert)
			{
				switch (segment)
				{
				case 1:
					if (rotationHorizont)
						smallBuffer[19199
								- (((line / 2) * 160) + ((1 - (line % 2)) * 80)
										+ (column))] = result;
					else
						smallBuffer[19199
								- (((line / 2) * 160) + ((line % 2) * 80)
										+ (column))] = result;
					break;
				case 2:
					if (rotationHorizont)
						smallBuffer[14399
								- (((line / 2) * 160) + ((1 - (line % 2)) * 80)
										+ (column))] = result;
					else
						smallBuffer[14399
								- (((line / 2) * 160) + ((line % 2) * 80)
										+ (column))] = result;
					break;
				case 3:
					if (rotationHorizont)
						smallBuffer[9599
								- (((line / 2) * 160) + ((1 - (line % 2)) * 80)
										+ (column))] = result;
					else
						smallBuffer[9599
								- (((line / 2) * 160) + ((line % 2) * 80)
										+ (column))] = result;
					break;
				case 4:
					if (rotationHorizont)
						smallBuffer[4799
								- (((line / 2) * 160) + ((1 - (line % 2)) * 80)
										+ (column))] = result;
					else
						smallBuffer[4799
								- (((line / 2) * 160) + ((line % 2) * 80)
										+ (column))] = result;
					break;
				}
			}
			//Rotated
			else
			{
				switch (segment)
				{
				case 1:
					smallBuffer[((line / 2) * 160) + ((line % 2) * 80)
							+ (column)] = result;
					break;
				case 2:
					smallBuffer[4800
							+ (((line / 2) * 160) + ((line % 2) * 80) + (column))] =
							result;
					break;
				case 3:
					smallBuffer[9600
							+ (((line / 2) * 160) + ((line % 2) * 80) + (column))] =
							result;
					break;
				case 4:
					smallBuffer[14400
							+ (((line / 2) * 160) + ((line % 2) * 80) + (column))] =
							result;
					break;
				}
			}
		}

	}

	//Everything worked
	return 1;
}

/* Get one line package from the Lepton */
LeptonReadError lepton_getPackage(byte line, byte seg)
{
	//Receive one frame over SPI
	SPI.transfer(leptonFrame, 164);

	//Repeat as long as the frame is not valid, equals sync
	if ((leptonFrame[0] & 0x0F) == 0x0F)
		return DISCARD;

	//Check if the line number matches the expected line
	if (leptonFrame[1] != line)
		return ROW_ERROR;

	//For the Lepton3.x, check if the segment number matches
	if ((line == 20)
			&& ((leptonVersion == leptonVersion_3_0_shutter)
					|| (leptonVersion == leptonVersion_3_5_shutter)))
	{
		byte segment = (leptonFrame[0] >> 4);
		if (segment == 0)
			return SEGMENT_INVALID;
		if (segment != seg)
			return SEGMENT_ERROR;
	}

	//Everything worked
	return NONE;
}

/* Get one frame of raw values from the lepton */
void lepton_getRawValues()
{
	byte line, error, segmentNumbers;

	//Determine number of segments
	if ((leptonVersion == leptonVersion_3_0_shutter)
			|| (leptonVersion == leptonVersion_3_5_shutter))
		segmentNumbers = 4;
	else
		segmentNumbers = 1;

	//Begin SPI Transmission
	lepton_begin();

	//Go through the segments
	for (byte segment = 1; segment <= segmentNumbers; segment++)
	{
		//Reset error counter for each segment
		error = 0;

		//Go through one segment, equals 60 lines of 80 values
		do
		{
			for (line = 0; line < 60; line++)
			{
				//Maximum error count
				if (error == 255)
				{
					//If main menu should be entreed
					if (showMenu == showMenu_desired)
					{
						lepton_end();
						return;
					}

					//Reset segment
					segment = 1;
					//Reset error
					error = 0;
					//Reset Lepton SPI
					lepton_reset();
					//Restart at line 0
					break;
				}

				//Get a package from the lepton
				LeptonReadError retVal = lepton_getPackage(line, segment);

				//If everythin worked, continue
				if (retVal == NONE)
					if (savePackage(line, segment))
						continue;

				//Raise lepton error
				error++;

				//Stabilize framerate
				uint32_t time = micros();
				while ((micros() - time) < 800)
					__asm__ volatile ("nop");

				//Restart at line 0
				break;
			}
		} while (line != 60);
	}

	//End SPI Transmission
	lepton_end();
}

/* Select I2C Register on the Lepton */
void lepton_setReg(byte reg)
{
	Wire.beginTransmission(0x2A);
	Wire.write(reg >> 8 & 0xff);
	Wire.write(reg & 0xff);
	Wire.endTransmission();
}

/* Read I2C Register on the lepton */
int lepton_readReg(byte reg)
{
	uint16_t reading;
	lepton_setReg(reg);
	Wire.requestFrom(0x2A, 2);
	reading = Wire.read();
	reading = reading << 8;
	reading |= Wire.read();
	return reading;
}

/*
 First reads the DATA Length Register (0x006)
 Then reads the acutal DATA Registers:
 DATA 0 Register, DATA 1 Register etc.

 If the request length is smaller that the DATA Length it returns -1
 */
int lepton_i2c_read_data_register(byte *data, int data_length_request)
{

	int data_length_recv;
	int data_read;
	// Wait for execution of the command
	while (lepton_readReg(0x2) & 0x01)
		;

	// Read the data length (should be 4)
	data_length_recv = lepton_readReg(0x6);

	if (data_length_recv < data_length_request)
	{
		return -1;
	}

	Wire.requestFrom(0x2A, data_length_request);
	data_read = Wire.readBytes(data, data_length_request);
	Wire.endTransmission();
	return data_read;
}

/*
 First writes the actual DATA Registers (0x0008).
 Then writes the DATA Length Register (0x006)
 */
byte lepton_i2c_write_data_register(byte *data, int length)
{

	// Wait for execution of the command
	while (lepton_readReg(0x2) & 0x01)
		;

	Wire.beginTransmission(0x2A);
	// CCI/TWI Data Registers is at 0x0008
	Wire.write(0x00);
	Wire.write(0x08);
	for (int i = 0; i < length; i++)
	{
		Wire.write(data[i]);
	}
	Wire.endTransmission();

	// CCI/TWI Data Length Register is at 0x0006
	Wire.beginTransmission(0x2A);
	Wire.write(0x00);
	Wire.write(0x06);
	//Data length bytes
	Wire.write((length >> 8) & 0xFF);
	Wire.write(length & 0xFF);
	return Wire.endTransmission();
}

/*
 Write the command words (16-bit) via I2C
 */
byte lepton_i2c_execute_command(byte cmdbyte0, byte cmdbyte1)
{
	// Wait for execution of the command
	while (lepton_readReg(0x2) & 0x01)
		;

	Wire.beginTransmission(0x2A);
	Wire.write(0x00);
	Wire.write(0x04); //COMMANDID_REG
	Wire.write(cmdbyte0);
	Wire.write(cmdbyte1);
	return Wire.endTransmission();
}

/* Trigger a flat-field-correction on the Lepton */
bool lepton_ffc(bool message, bool switch_gain)
{
	//Show a message for main menu
	if (message)
	{
		//When in manual temperature mode, a FFC is not possible
		if ((!autoMode) && (!switch_gain))
		{
			showFullMessage((char*) "No FFC in manual mode", true);
			delay(1000);
			return false;
		}
		showFullMessage((char*) "Performing FFC..", true);
	}

	byte error;

	if ((leptonVersion == leptonVersion_2_5_shutter)
			|| (leptonVersion == leptonVersion_3_5_shutter))
	{
		// For radiometric Lepton, send RAD FFC command
		// RAD FFC Normalization Command
		// 0x0E00 (SDK Module ID) + 0x2C (SDK Command ID) + 0x2 (RUN operation) + 0x4000 (Protection Bit) = 0x4E2E
		error = lepton_i2c_execute_command(0x4E, 0x2E);
	}
	else
	{
		//For all others, send normal FFC command
		// SYS Run FFC Normalization
		// 0x0200 (SDK Module ID) + 0x40 (SDK Command ID) + 0x2 (RUN operation) + 0x0000 (Protection Bit) = 0x0242
		error = lepton_i2c_execute_command(0x02, 0x42);
	}

	//Wait some time when in main menu
	if (message)
		delay(2000);

	return error;
}

/* Get the spotmeter value on a radiometric lepton */
float lepton_spotTemp()
{
	//Get RAD spotmeter value
	Wire.beginTransmission(0x2A);
	Wire.write(0x00);
	Wire.write(0x04);
	Wire.write(0x4E);
	Wire.write(0xD0);
	byte error = Wire.endTransmission();

	//Lepton I2C error, set diagnostic
	if (error != 0)
	{
		setDiagnostic(diag_spot);
		return 0;
	}

	//Transfer the new package
	Wire.beginTransmission(0x2A);
	while (lepton_readReg(0x2) & 0x01)
		;
	Wire.requestFrom(0x2A, lepton_readReg(0x6));
	byte response[8];
	Wire.readBytes(response, 8);
	Wire.endTransmission();

	//Calculate spot temperature in Kelvin
	float spotTemp = (response[0] * 256.0) + response[1];
	//Multiply by correction factor
	if(gainMode == lepton_3_5_gain_high)
	{
		spotTemp *= 0.01;
	}
	else
	{
		spotTemp *= 0.1;
	}
	//Convert to celsius
	spotTemp -= 273.15;

	return spotTemp;
}

/* Set the shutter operation to manual/auto */
void lepton_ffcMode(bool automatic)
{
	//If there is no shutter, return
	if (leptonShutter == leptonShutter_none)
		return;

	//When enabling auto FFC, check for some factors
	if ((automatic)
			&& ((hotColdMode != hotColdMode_disabled) || (autoMode == false)
					|| (limitsLocked == true)))
		return;

	//Contains the standard values for the FFC mode
	byte package[] =
	{ automatic, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 224,
			147, 4, 0, 0, 0, 0, 0, 44, 1, 52, 0 };

	lepton_i2c_write_data_register(package, sizeof(package));

	// SYS FFC Mode Control Command
	// 0x0200 (SDK Module ID) + 0x3C (SDK Command ID) + 0x1 (SET operation) + 0x0000 (Protection Bit) = 0x023D
	lepton_i2c_execute_command(0x02, 0x3D);

	//Set shutter mode
	if (automatic)
		leptonShutter = leptonShutter_auto;
	else
		leptonShutter = leptonShutter_manual;
}

/* Checks the Lepton hardware revision */
void lepton_version()
{

	// OEM FLIR Systems Part Number
	// 0x0800 (SDK Module ID) + 0x1C (SDK Command ID) + 0x0 (GET operation) + 0x4000 (Protection Bit) = 0x481C
	byte error = lepton_i2c_execute_command(0x48, 0x1C);

	//Lepton I2C error, set diagnostic
	if (error != 0)
	{
		setDiagnostic(diag_lep_conf);
		leptonVersion = leptonVersion_2_0_noShutter;
		return;
	}

	char leptonhw[33];
	lepton_i2c_read_data_register((byte*) leptonhw, 32);

	//Detected Lepton2 Non-Shuttered
	if (strstr(leptonhw, "05-060340") != NULL)
	{
		leptonVersion = leptonVersion_2_0_noShutter;
		leptonShutter = leptonShutter_none;
	}

	//Detected Lepton2 Shuttered
	else if (strstr(leptonhw, "05-060950") != NULL)
	{
		leptonVersion = leptonVersion_2_0_shutter;
		leptonShutter = leptonShutter_auto;
	}

	//Detected Lepton2.5 Shuttered (Radiometric)
	else if (strstr(leptonhw, "05-070360") != NULL)
	{
		leptonVersion = leptonVersion_2_5_shutter;
		leptonShutter = leptonShutter_autoRAD;
	}

	//Detected Lepton3 Shuttered
	else if (strstr(leptonhw, "05-070620") != NULL)
	{
		leptonVersion = leptonVersion_3_0_shutter;
		leptonShutter = leptonShutter_auto;
	}

	//Detected Lepton3.5 Shuttered (Radiometric)
	else if (strstr(leptonhw, "05-070170") != NULL)
	{
		leptonVersion = leptonVersion_3_5_shutter;
		leptonShutter = leptonShutter_autoRAD;
	}

	//Detected unknown Lepton2 No-Shutter
	else
	{
		leptonVersion = leptonVersion_2_0_noShutter;
		leptonShutter = leptonShutter_none;
	}

	//Write to EEPROM
	EEPROM.write(eeprom_leptonVersion, leptonVersion);
}

/*
 * Set the SYS Gain Mode
 * 0: high mode (hardware default),
 * 1: low mode,
 * 2: auto mode
 * The measurement range for the Lepton 3.5 is (see datasheet for details):
 * High Gain Mode: 	-10 to +140 deg C
 * Low Gain Mode: 	-10 to +450 deg C
 *
 */
void lepton_set_sys_gain_mode(byte mode)
{
	if (mode > 2)
	{
		return;
	}

	//The enum value is the LSB of DATA0
	byte data[4] =
	{ 0x00, mode, 0x00, 0x00 };
	lepton_i2c_write_data_register(data, 4);

	// Execute the SYS Gain Mode Set Command, so that the module applies the values
	// 0x0200 (SDK Module ID) + 0x48 (SDK Command ID) + 0x1 (SET operation) + 0x0000 (Protection Bit) = 0x0249.
	lepton_i2c_execute_command(0x02, 0x49);
}

/*
 * Sets the SYS Gain Mode to high gain mode
 */
void lepton_set_sys_gain_high()
{
	lepton_set_sys_gain_mode(0x00);
}

/*
 * Sets the SYS Gain Mode to low gain mode
 */
void lepton_set_sys_gain_low()
{
	lepton_set_sys_gain_mode(0x01);
}

/*
 * Sets the SYS Gain Mode to auto mode
 */
void lepton_set_sys_gain_auto()
{
	lepton_set_sys_gain_mode(0x02);
}

/*
 * Returns the SYS Gain Mode
 * 0: high mode (hardware default),
 * 1: low mode,
 * 2: auto mode
 * The measurement range for the Lepton 3.5 is (see datasheet for details):
 * High Gain Mode: 	-10 to +140 deg C
 * Low Gain Mode: 	-10 to +450 deg C
 *
 * Returns -1 if the gain mode could not be read
 */
int lepton_get_sys_gain_mode()
{

	byte data[4];

	//SYS Gain Mode Get Command
	// 0x0200 (SDK Module ID) + 0x48 (SDK Command ID) + 0x0 (GET operation) + 0x0000 (Protection Bit) = 0x0248.
	lepton_i2c_execute_command(0x02, 0x48);
	lepton_i2c_read_data_register(data, 4);

	//The enum value is the LSB of DATA0
	return data[1];
}

/*
 * Returns the  RAD T-Linear Resolution
 * 0: 100
 * 1: 10
 */
byte lepton_get_rad_tlinear_resolution()
{

	byte data[4];

	// RAD T-Linear Resolution Get Command
	// 0x0E00 (SDK Module ID) + 0xC4 (SDK Command ID) + 0x0 (GET operation) + 0x4000 (Protection Bit) = 0x4EC4.
	lepton_i2c_execute_command(0x4E, 0xC4);
	lepton_i2c_read_data_register(data, 4);

	//The enum value is the LSB of DATA0
	return data[1];
}

/*
 * Returns the RAD T-Linear Resolution Factor as a float
 * Returns -1 if the value could not be read
 */
float lepton_get_resolution()
{
	byte resolution = lepton_get_rad_tlinear_resolution();
	if (resolution == 0)
	{
		return 0.1;
	}
	else
	{
		return 0.01;
	}
}

/*
 * Sets the RAD T-Linear Resolution
 * resolution: 0 = factor 100
 * resolution: 1 = factor 10
 *
 * You need to set this to factor 10 for temperature over 382.2 deg C
 * The maximum temperature value of the 16-bit is 65535.
 * For factor 100: The maximum is 655.35 Kelvin which equals to 655.35 - 273.15 = 382.2 deg C
 * For factor 10:  The maximum is 6553.5 Kelvin which equals to 6553.5 - 273.15 = 6280.35 deg C
 */
void lepton_set_rad_tlinear_resolution(byte resolution)
{
	if (resolution > 1)
	{
		return;
	}

	//The enum value is the LSB of DATA0
	byte data[4] =
	{ 0x00, resolution, 0x00, 0x00 };
	lepton_i2c_write_data_register(data, 4);

	// RAD T-Linear Resolution Set Command
	// 0x0E00 (SDK Module ID) + 0xC4 (SDK Command ID) + 0x1 (SET operation) + 0x4000 (Protection Bit) = 0x4EC5.
	lepton_i2c_execute_command(0x4E, 0xC5);
}

/*
 * Sets the RAD T-Linear Resolution to 10 (factor 0.1)
 */
void lepton_set_rad_tlinear_10()
{
	lepton_set_rad_tlinear_resolution(0);
}

/*
 * Sets the RAD T-Linear Resolution to 100 (factor 0.01)
 */
void lepton_set_rad_tlinear_100()
{
	lepton_set_rad_tlinear_resolution(1);
}

/* Switch to low gain on Lepton3.5 (-10 to +450 deg C) */
void lepton_3_5_set_low_gain()
{
	lepton_set_sys_gain_low();
	lepton_set_rad_tlinear_10();
	calSlope = 0.1;
	gainMode = lepton_3_5_gain_low;
}

/* Switch to high gain on Lepton3.5 (-10 to +140 deg C) */
void lepton_3_5_set_high_gain()
{
	lepton_set_sys_gain_high();
	lepton_set_rad_tlinear_100();
	calSlope = 0.01;
	gainMode = lepton_3_5_gain_high;
}

/* Init the FLIR Lepton LWIR sensor */
void lepton_init()
{
	//Check the Lepton HW Revision
	lepton_version();

	//For radiometric Lepton, set calibration to done
	if ((leptonVersion == leptonVersion_2_5_shutter)
			|| (leptonVersion == leptonVersion_3_5_shutter))
		calStatus = cal_standard;

	//Otherwise init warmup timer
	else
	{
		//Set the calibration timer
		calTimer = millis();
		//Set calibration status to warmup if not coming from mass storage
		calStatus = cal_warmup;
	}

	//Set the compensation value to zero
	calComp = 0;

	//Activate auto mode
	autoMode = true;
	//Deactivate limits Locked
	limitsLocked = false;

	//Check if SPI works
	lepton_begin();
	do
	{
		SPI.transfer(leptonFrame, 164);
	}
	//Repeat as long as the frame is not valid, equals sync
	while (((leptonFrame[0] & 0x0F) == 0x0F) && ((millis() - calTimer) < 1000));
	lepton_end();

	//If sync not received after a second, set diagnostic
	if ((leptonFrame[0] & 0x0F) == 0x0F)
		setDiagnostic(diag_lep_data);
}
