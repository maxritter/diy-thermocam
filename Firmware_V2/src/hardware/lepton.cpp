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
#include <v2_core.h>
#include <tc_proto_v2.h>
#include <lepton.h>

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Array to store one Lepton frame
static byte leptonFrame[164];
//Auto-bypass VoSPI CRC only if sensor persistently sends non-CRC packets
static uint8_t s_crcConsecutiveFails = 0;
static bool s_crcBypass = false;
//Preserve last good radiometric spot temperature across transient I2C glitches
static float s_lastRadSpotC = 25.0f;

static constexpr uint32_t LEPTON_I2C_TIMEOUT_MS = 250u;
static constexpr uint32_t LEPTON_FRAME_TIMEOUT_MS = 1200u;

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

	//Verify FLIR VoSPI CCITT CRC-16 to reject corrupted SPI pixels (prevents white/black spikes)
	if (!s_crcBypass)
	{
		if (!v2_core::vospi_packet_crc_ok(leptonFrame))
		{
			vospiCrcErrorCount++;
			if (++s_crcConsecutiveFails >= 32u)
				s_crcBypass = true;
			return ROW_ERROR;
		}
		s_crcConsecutiveFails = 0;
	}

	//Check if the line number matches the expected line
	if (leptonFrame[1] != line)
		return ROW_ERROR;

	//For the Lepton3.x, check if the segment number matches
	if ((line == 20)
			&& ((leptonVersion == leptonVersion_3_0_shutter)
					|| (leptonVersion == leptonVersion_3_5_shutter)))
	{
		byte segment = (leptonFrame[0] >> 4) & 0x07;
		if (segment == 0)
			return SEGMENT_INVALID;
		if (segment != seg)
			return SEGMENT_ERROR;
	}

	//Everything worked
	return NONE;
}

/* Helper predicates for sensor family */
bool lepton_isLepton3()
{
	return (leptonVersion == leptonVersion_3_0_shutter)
			|| (leptonVersion == leptonVersion_3_5_shutter);
}

bool lepton_isRadiometric()
{
	return (leptonVersion == leptonVersion_2_5_shutter)
			|| (leptonVersion == leptonVersion_3_5_shutter);
}

/* Get one frame of raw values from the lepton with configurable timeout & serial yield */
bool lepton_getRawValuesEx(uint32_t timeoutMs, bool abortOnSerial)
{
	byte line, error, segmentNumbers;
	uint32_t frameStartMs = millis();

	//Determine number of segments
	segmentNumbers = lepton_isLepton3() ? 4 : 1;

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
				//Wall-clock guard so a missing/stalled sensor cannot hang the UI indefinitely
				if ((millis() - frameStartMs) > timeoutMs)
				{
					lepton_end();
					return false;
				}

				//Yield promptly to incoming USB v2 control traffic while idle before line 0
				if (abortOnSerial && (segment == 1) && (line == 0) && (Serial.available() > 0))
				{
					lepton_end();
					return false;
				}

				//Maximum error count
				if (error == 255)
				{
					//If main menu should be entered
					if (showMenu == showMenu_desired)
					{
						lepton_end();
						return false;
					}

					//Reset segment
					segment = 1;
					//Reset error
					error = 0;
					//Track resync
					vospiResyncCount++;
					//Reset Lepton SPI
					lepton_reset();
					//Restart at line 0
					break;
				}

				//Get a package from the lepton
				LeptonReadError retVal = lepton_getPackage(line, segment);

				//If everything worked, continue
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
	return true;
}

/* Get one frame of raw values from the lepton */
void lepton_getRawValues()
{
	lepton_getRawValuesEx(LEPTON_FRAME_TIMEOUT_MS, false);
}

/* Select I2C Register on the Lepton */
void lepton_setReg(byte reg)
{
	Wire.beginTransmission(0x2A);
	Wire.write(reg >> 8 & 0xff);
	Wire.write(reg & 0xff);
	Wire.endTransmission();
}

/* Read I2C Register on the lepton (returns -1 on I2C failure) */
int lepton_readReg(byte reg)
{
	lepton_setReg(reg);
	if (Wire.requestFrom(0x2A, 2) < 2)
		return -1;
	uint16_t reading = static_cast<uint16_t>(Wire.read()) << 8;
	reading |= static_cast<uint16_t>(Wire.read());
	return reading;
}

/* Bounded wait for Lepton CCI command ready (STATUS bit 0 == 0) */
static bool lepton_waitReady(uint32_t timeoutMs = LEPTON_I2C_TIMEOUT_MS)
{
	uint32_t t0 = millis();
	while (true)
	{
		int status = lepton_readReg(0x2);
		if (status < 0)
			return false;
		if ((status & 0x01) == 0)
			return true;
		if ((millis() - t0) >= timeoutMs)
			return false;
		delay(1);
	}
}

/*
 First reads the DATA Length Register (0x006, in 16-bit words)
 Then points to DATA0 (0x0008) and reads data_length_request bytes.
 Returns -1 on I2C timeout or error.
 */
int lepton_i2c_read_data_register(byte *data, int data_length_request)
{
	if (!lepton_waitReady())
		return -1;

	int words_avail = lepton_readReg(0x6);
	if (words_avail < 0)
		return -1;

	lepton_setReg(0x08);
	if (static_cast<int>(Wire.requestFrom(0x2A, data_length_request)) < data_length_request)
		return -1;
	int data_read = Wire.readBytes(data, data_length_request);
	return data_read;
}

/*
 First writes the actual DATA Registers (0x0008).
 Then writes the DATA Length Register (0x006, in 16-bit words per FLIR Lepton CCI IDD).
 */
byte lepton_i2c_write_data_register(byte *data, int length)
{
	if (!lepton_waitReady())
		return 4;

	Wire.beginTransmission(0x2A);
	// CCI/TWI Data Registers is at 0x0008
	Wire.write(0x00);
	Wire.write(0x08);
	for (int i = 0; i < length; i++)
	{
		Wire.write(data[i]);
	}
	byte err = Wire.endTransmission();
	if (err != 0)
		return err;

	// CCI/TWI Data Length Register is at 0x0006 (count of 16-bit words)
	uint16_t wordCount = static_cast<uint16_t>((length + 1) / 2);
	Wire.beginTransmission(0x2A);
	Wire.write(0x00);
	Wire.write(0x06);
	Wire.write((wordCount >> 8) & 0xFF);
	Wire.write(wordCount & 0xFF);
	return Wire.endTransmission();
}

/*
 Write the command words (16-bit) via I2C
 */
byte lepton_i2c_execute_command(byte cmdbyte0, byte cmdbyte1)
{
	if (!lepton_waitReady())
		return 4;

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
	//Get RAD spotmeter value (0x4ED0)
	byte error = lepton_i2c_execute_command(0x4E, 0xD0);
	if (error != 0)
		return s_lastRadSpotC;

	byte response[8] = {0};
	if (lepton_i2c_read_data_register(response, 8) < 8)
		return s_lastRadSpotC;

	//Calculate spot temperature in Kelvin
	uint16_t rawK = (static_cast<uint16_t>(response[0]) << 8) | response[1];
	if (rawK == 0u)
		return s_lastRadSpotC;

	float spotC = static_cast<float>(rawK);
	//Only Lepton 3.5 in low gain uses 0.1 K/LSB; Lepton 2.5 and 3.5 high gain use 0.01 K/LSB
	if ((leptonVersion == leptonVersion_3_5_shutter) && (gainMode == lepton_3_5_gain_low))
	{
		spotC *= 0.1f;
	}
	else
	{
		spotC *= 0.01f;
	}
	spotC -= 273.15f;

	if ((spotC >= -60.0f) && (spotC <= 600.0f))
		s_lastRadSpotC = spotC;

	return s_lastRadSpotC;
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
	memset(leptonhw, 0, sizeof(leptonhw));
	if (lepton_i2c_read_data_register((byte*) leptonhw, 32) < 16)
	{
		setDiagnostic(diag_lep_conf);
		leptonVersion = leptonVersion_2_0_noShutter;
		return;
	}
	leptonhw[32] = '\0';
	memset(leptonPartNum, 0, sizeof(leptonPartNum));
	memcpy(leptonPartNum, leptonhw, sizeof(leptonPartNum) - 1);

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

	//Detected Lepton3.5 Shuttered (Radiometric, including 3.1R / newer batches)
	else if ((strstr(leptonhw, "05-070170") != NULL) || (strstr(leptonhw, "05-070850") != NULL))
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
	byte data[4] = {0};

	//SYS Gain Mode Get Command
	// 0x0200 (SDK Module ID) + 0x48 (SDK Command ID) + 0x0 (GET operation) + 0x0000 (Protection Bit) = 0x0248.
	if (lepton_i2c_execute_command(0x02, 0x48) != 0)
		return -1;
	if (lepton_i2c_read_data_register(data, 4) < 4)
		return -1;

	//The enum value is the LSB of DATA0
	return data[1];
}

/*
 * Returns the  RAD T-Linear Resolution
 * 0: 10 (0.1 K/LSB)
 * 1: 100 (0.01 K/LSB)
 */
byte lepton_get_rad_tlinear_resolution()
{
	byte data[4] = {0, 1, 0, 0};

	// RAD T-Linear Resolution Get Command
	// 0x0E00 (SDK Module ID) + 0xC4 (SDK Command ID) + 0x0 (GET operation) + 0x4000 (Protection Bit) = 0x4EC4.
	if (lepton_i2c_execute_command(0x4E, 0xC4) != 0)
		return 1;
	if (lepton_i2c_read_data_register(data, 4) < 4)
		return 1;

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

/* Read FPA temperature in Celsius via SYS FPA Temperature Kelvin GET (0x0210) */
float lepton_getFpaTempC()
{
	if (!checkDiagnostic(diag_lep_conf))
		return leptonFpaTempC;
	if (lepton_i2c_execute_command(0x02, 0x10) != 0)
		return leptonFpaTempC;
	byte response[2] = {0};
	if (lepton_i2c_read_data_register(response, 2) < 2)
		return leptonFpaTempC;
	uint16_t ck = (static_cast<uint16_t>(response[0]) << 8) | response[1];
	if (ck == 0)
		return leptonFpaTempC;
	leptonFpaTempC = static_cast<float>(ck) * 0.01f - 273.15f;
	return leptonFpaTempC;
}

/* FLIR Lepton CCI RAD Flux Linear Parameters (0x4EBC GET / 0x4EBD SET) - radiometric modules only */
bool lepton_getRadParams(tc_v2::RadParams& out)
{
	if (!lepton_isRadiometric() || !checkDiagnostic(diag_lep_conf))
		return false;
	if (lepton_i2c_execute_command(0x4E, 0xBC) != 0)
		return false;
	byte raw[16] = {0};
	if (lepton_i2c_read_data_register(raw, 16) < 16)
		return false;
	auto rd_be16 = [&](int idx) -> uint16_t {
		return static_cast<uint16_t>((static_cast<uint16_t>(raw[idx * 2]) << 8) | raw[idx * 2 + 1]);
	};
	out.scene_emissivity = rd_be16(0);
	out.t_bkg_ck         = rd_be16(1);
	out.tau_window       = rd_be16(2);
	out.t_window_ck      = rd_be16(3);
	out.tau_atm          = rd_be16(4);
	out.t_atm_ck         = rd_be16(5);
	out.refl_window      = rd_be16(6);
	out.t_refl_ck        = rd_be16(7);
	out.flags = 0x01u | ((EEPROM.read(eeprom_radParamsSet) == eeprom_setValue) ? 0x02u : 0x00u);
	out.reserved = 0;
	return true;
}

bool lepton_setRadParams(const tc_v2::RadParams& in)
{
	if (!lepton_isRadiometric() || !checkDiagnostic(diag_lep_conf))
		return false;
	tc_v2::RadParams eff = in;
	if (in.flags & 0x02u)
		eff = tc_v2::RadParams{};
	byte raw[16];
	auto wr_be16 = [&](int idx, uint16_t v) {
		raw[idx * 2] = static_cast<byte>((v >> 8) & 0xFFu);
		raw[idx * 2 + 1] = static_cast<byte>(v & 0xFFu);
	};
	wr_be16(0, eff.scene_emissivity);
	wr_be16(1, eff.t_bkg_ck);
	wr_be16(2, eff.tau_window);
	wr_be16(3, eff.t_window_ck);
	wr_be16(4, eff.tau_atm);
	wr_be16(5, eff.t_atm_ck);
	wr_be16(6, eff.refl_window);
	wr_be16(7, eff.t_refl_ck);

	if (lepton_i2c_write_data_register(raw, 16) != 0)
		return false;
	if (lepton_i2c_execute_command(0x4E, 0xBD) != 0)
		return false;
	if (!lepton_waitReady())
		return false;

	if (in.flags & 0x02u) {
		EEPROM.write(eeprom_radParamsSet, 0);
	} else if (in.flags & 0x01u) {
		uint8_t le[18];
		tc_v2::encode_rad_params(le, eff);
		for (int i = 0; i < 16; ++i)
			EEPROM.write(eeprom_radParamsData + i, le[i]);
		EEPROM.write(eeprom_radParamsSet, eeprom_setValue);
	}
	return true;
}

void lepton_loadRadParamsFromEEPROM()
{
	if (!lepton_isRadiometric() || !checkDiagnostic(diag_lep_conf))
		return;
	if (EEPROM.read(eeprom_radParamsSet) != eeprom_setValue)
		return;
	uint8_t le[18] = {0};
	for (int i = 0; i < 16; ++i)
		le[i] = EEPROM.read(eeprom_radParamsData + i);
	tc_v2::RadParams rp{};
	if (tc_v2::decode_rad_params(le, 18, rp) && tc_v2::validate_rad_params(rp)) {
		rp.flags = 0;
		lepton_setRadParams(rp);
	}
}

/* Init the FLIR Lepton LWIR sensor */
void lepton_init()
{
	//Check the Lepton HW Revision
	lepton_version();
	leptonFpaTempC = NAN;
	lepton_getFpaTempC();

	//For radiometric Lepton, set calibration to done and apply saved flux-linear params
	if ((leptonVersion == leptonVersion_2_5_shutter)
			|| (leptonVersion == leptonVersion_3_5_shutter))
	{
		calStatus = cal_standard;
		lepton_loadRadParamsFromEEPROM();
	}

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

	//Check if SPI works (use dedicated syncStart so radiometric Leptons also get the full 1 s window)
	uint32_t syncStart = millis();
	lepton_begin();
	do
	{
		SPI.transfer(leptonFrame, 164);
	}
	//Repeat as long as the frame is not valid, equals sync
	while (((leptonFrame[0] & 0x0F) == 0x0F) && ((millis() - syncStart) < 1000u));
	lepton_end();

	//If sync not received after a second, set diagnostic
	if ((leptonFrame[0] & 0x0F) == 0x0F)
		setDiagnostic(diag_lep_data);
}
