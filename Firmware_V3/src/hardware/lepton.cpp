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
#include <display.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <gui.h>
#include <lepton.h>

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Array to store one Lepton frame
static byte lepton_packet[164];

volatile int lepton_curSeg = 1;
volatile bool lepton_validSegRegion = false;
volatile bool irqAttached = false;

/*######################## PUBLIC FUNCTION BODIES #############################*/

uint32_t AbsDiff32u(uint32_t n1, uint32_t n2)
{
	if (n2 >= n1)
	{
		return (n2 - n1);
	}
	else
	{
		return (n2 - n1 + 0xFFFFFFFF);
	}
}

void lepton_startFrame()
{
	leptonBufferValid = false;
	if (!irqAttached)
	{
		attachInterrupt(pin_lepton_vsync, lepton_getFrameAsync, RISING);
		irqAttached = true;
	}
}

void lepton_endFrame()
{
	if (irqAttached)
	{
		detachInterrupt(pin_lepton_vsync);
		irqAttached = false;
	}
}

/* Start Lepton SPI Transmission */
void lepton_begin()
{
	SPI1.beginTransaction(SPISettings(25000000, MSBFIRST, SPI_MODE1));
	digitalWriteFast(pin_lepton_cs, LOW);
}

/* Reset the SPI bus to re-initiate Lepton communication */
void lepton_reset()
{
	lepton_end();
	delay(186);
	lepton_begin();
}

/* End Lepton SPI Transmission */
void lepton_end()
{
	digitalWriteFast(pin_lepton_cs, HIGH);
	SPI1.endTransaction();
}

/* Store one package of 80 columns into RAM */
void lepton_savePacket(uint8_t line, uint8_t segment)
{
	//Go through the video pixels for one video line
	for (int column = 0; column < 80; column++)
	{
		//Apply horizontal mirroring
		if (rotationHorizont)
			column = 79 - column;

		//Make a 16-bit rawvalue from the lepton frame
		uint16_t result = (uint16_t)(lepton_packet[2 * column + 4] << 8 | lepton_packet[2 * column + 5]);

		//Discard horizontal mirroring
		if (rotationHorizont)
			column = 79 - column;

		//Lepton2.5
		if (leptonVersion == leptonVersion_2_5_shutter)
		{
			//Non-rotated
			if (!rotationVert)
			{
				smallBuffer[(line * 2 * 160) + (column * 2)] = result;
				smallBuffer[(line * 2 * 160) + (column * 2) + 1] = result;
				smallBuffer[(line * 2 * 160) + 160 + (column * 2)] = result;
				smallBuffer[(line * 2 * 160) + 160 + (column * 2) + 1] = result;
			}
			//Rotated
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

		//Lepton3.x
		else
		{
			//Non-Rotated
			if (!rotationVert)
			{
				switch (segment)
				{
				case 1:
					smallBuffer[((line / 2) * 160) + ((line % 2) * 80) + (column)] = result;
					break;
				case 2:
					smallBuffer[4800 + (((line / 2) * 160) + ((line % 2) * 80) + (column))] =
						result;
					break;
				case 3:
					smallBuffer[9600 + (((line / 2) * 160) + ((line % 2) * 80) + (column))] =
						result;
					break;
				case 4:
					smallBuffer[14400 + (((line / 2) * 160) + ((line % 2) * 80) + (column))] =
						result;
					break;
				}
			}

			//Rotated
			else
			{
				switch (segment)
				{
				case 1:
					if (rotationHorizont)
						smallBuffer[19199 - (((line / 2) * 160) + ((1 - (line % 2)) * 80) + (column))] = result;
					else
						smallBuffer[19199 - (((line / 2) * 160) + ((line % 2) * 80) + (column))] = result;
					break;
				case 2:
					if (rotationHorizont)
						smallBuffer[14399 - (((line / 2) * 160) + ((1 - (line % 2)) * 80) + (column))] = result;
					else
						smallBuffer[14399 - (((line / 2) * 160) + ((line % 2) * 80) + (column))] = result;
					break;
				case 3:
					if (rotationHorizont)
						smallBuffer[9599 - (((line / 2) * 160) + ((1 - (line % 2)) * 80) + (column))] = result;
					else
						smallBuffer[9599 - (((line / 2) * 160) + ((line % 2) * 80) + (column))] = result;
					break;
				case 4:
					if (rotationHorizont)
						smallBuffer[4799 - (((line / 2) * 160) + ((1 - (line % 2)) * 80) + (column))] = result;
					else
						smallBuffer[4799 - (((line / 2) * 160) + ((line % 2) * 80) + (column))] = result;
					break;
				}
			}
		}
	}
}

/* Fetch frame asynchronously over IRQ */
void lepton_getFrame()
{
	long timer = millis();
	while (!leptonBufferValid)
	{
		if (millis() - timer > 3000)
		{
			showFullMessage((char *)"Lepton error, try to reset..");
			lepton_reset();
			lepton_startFrame();
			timer = millis();
		}
	}
}

/* Get one line package from the Lepton */
LeptonReadError lepton_getPacketSync(uint8_t line, uint8_t seg)
{
	//Receive one frame over SPI
	SPI1.transfer(lepton_packet, 164);

	//Repeat as long as the frame is not valid, equals sync
	if ((lepton_packet[0] & 0x0F) == 0x0F)
		return DISCARD;

	//Check if the line number matches the expected line
	if (lepton_packet[1] != line)
		return ROW_ERROR;

	//For the Lepton3.5, check if the segment number matches
	if ((line == 20) && (leptonVersion == leptonVersion_3_5_shutter))
	{
		byte segment = (lepton_packet[0] >> 4);
		if (segment == 0)
			return SEGMENT_INVALID;
		if (segment != seg)
			return SEGMENT_ERROR;
	}

	//Everything worked
	return NONE;
}

/* Get one line package from the Lepton */
bool lepton_getPacketAsync(uint8_t *line, uint8_t *seg)
{
	bool valid = false;
	*seg = 0;

	//Start transfer and get one package
	lepton_begin();
	SPI1.transfer(lepton_packet, 164);

	//Repeat as long as the frame is not valid, equals sync
	if ((lepton_packet[0] & 0x0F) == 0x0F)
	{
		valid = false;
	}
	else
	{
		*line = lepton_packet[1];

		//Get segment when possible
		if (*line == 20)
		{
			*seg = (lepton_packet[0] >> 4);
		}

		valid = true;
	}

	lepton_end();
	return valid;
}

/* Get one frame of raw values from the Lepton asynchronously */
void lepton_getFrameAsync()
{

	uint32_t startUsec;
	uint8_t line, prevLine;
	uint8_t segment;
	bool done = false;
	bool beforeValidData = true;

	startUsec = micros();
	prevLine = 255;

	while (!done)
	{
		//Try to get a new packet over SPI
		if (lepton_getPacketAsync(&line, &segment))
		{
			//Saw a valid packet
			if (line == prevLine)
			{
				//This is garbage data since line numbers should always increment
				done = true;
			}
			else
			{
				//For the Lepton3.5, check if the segment number matches
				if ((line == 20) && (leptonVersion == leptonVersion_3_5_shutter))
				{
					//Check segment
					if (!lepton_validSegRegion)
					{
						//Look for start of valid segment data
						if (segment == 1)
						{
							beforeValidData = false;
							lepton_validSegRegion = true;
						}
					}
					//Hold / Reset in starting position (always collecting in segment 1 buffer locations)
					else if ((segment < 2) || (segment > 4))
					{

						lepton_validSegRegion = false;
						lepton_curSeg = 1;
					}
				}

				//Save one packet consisting of 164 bytes
				if (((leptonVersion == leptonVersion_2_5_shutter) || ((beforeValidData || lepton_validSegRegion))) && (line <= 59))
					lepton_savePacket(line, lepton_curSeg);

				//Last line
				if (line == 59)
				{
					//For 160x120, check segment
					if (leptonVersion == leptonVersion_3_5_shutter)
					{
						if (lepton_validSegRegion)
						{
							if (lepton_curSeg < 4)
							{
								//Setup to get next segment
								lepton_curSeg++;
							}
							else
							{
								if (!leptonBufferValid)
								{
									leptonBufferValid = true;
									lepton_endFrame();
								}

								//Setup to get the next frame
								lepton_curSeg = 1;
								lepton_validSegRegion = false;
							}
						}
					}

					//For 80x60, do not check segment
					else
					{
						if (!leptonBufferValid)
						{
							leptonBufferValid = true;
							lepton_endFrame();
						}
					}

					done = true;
				}
			}

			prevLine = line;
		}
		//Did not see a valid packet within this segment interval
		else if (AbsDiff32u(startUsec, micros()) > 9450)
		{
			done = true;
		}
	}
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
int lepton_i2cReadDataRegister(byte *data, int data_length_request)
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
byte lepton_i2cWriteDataRegister(byte *data, int length)
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
			showFullMessage((char *)"No FFC in manual mode", true);
			delay(1000);
			return false;
		}
		showFullMessage((char *)"Performing FFC..", true);
	}

	byte error;

	if ((leptonVersion == leptonVersion_2_5_shutter) || (leptonVersion == leptonVersion_3_5_shutter))
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
		setDiagnostic(diag_lep_conf);
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
	if (leptonGainMode == lepton_gain_high)
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
	//When enabling auto FFC, check for some factors
	if ((automatic) && ((hotColdMode != hotColdMode_disabled) || (autoMode == false) || (limitsLocked == true)))
		return;

	//Contains the standard values for the FFC mode
	byte package[] =
		{automatic, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 224,
		 147, 4, 0, 0, 0, 0, 0, 44, 1, 52, 0};

	lepton_i2cWriteDataRegister(package, sizeof(package));

	// SYS FFC Mode Control Command
	// 0x0200 (SDK Module ID) + 0x3C (SDK Command ID) + 0x1 (SET operation) + 0x0000 (Protection Bit) = 0x023D
	lepton_i2c_execute_command(0x02, 0x3D);
}

/*
 * Sets the GPIO pins mode
 * gpio_mode: 0 = GPIO Mode (default)
 * gpio_mode: 5 = VSync Enabled (DIY-Thermocam V3 only)
 *
 * The new Lepton Breakout V2 exposes the VSync pin on its 20 pin header
 * The VSync signal is used to achieve maximum performance and avoid synchronization issues
 */
void lepton_setGpioMode(bool vsync_enabled)
{
	byte gpio_mode = 0x00;
	if (vsync_enabled)
		gpio_mode = 0x05;

	//The enum value is the LSB of DATA0
	byte data[4] =
		{0x00, gpio_mode, 0x00, 0x00};
	lepton_i2cWriteDataRegister(data, 4);

	// OEM GPIO Mode Select Set Command
	// 0x0800 (SDK Module ID) + 0x54 (SDK Command ID) + 0x1 (SET operation) + 0x4000 (Protection Bit) = 0x4855.
	lepton_i2c_execute_command(0x48, 0x55);
}

/* Checks the Lepton hardware revision */
bool lepton_version()
{

	// OEM FLIR Systems Part Number
	// 0x0800 (SDK Module ID) + 0x1C (SDK Command ID) + 0x0 (GET operation) + 0x4000 (Protection Bit) = 0x481C
	byte error = lepton_i2c_execute_command(0x48, 0x1C);

	//Lepton I2C error, set diagnostic
	if (error != 0)
	{
		setDiagnostic(diag_lep_conf);
		return false;
	}

	char leptonhw[33];
	lepton_i2cReadDataRegister((byte *)leptonhw, 32);

	//Detected Lepton2.5 Shuttered (Radiometric)
	if (strstr(leptonhw, "05-070360") != NULL)
	{
		leptonVersion = leptonVersion_2_5_shutter;
	}

	//Detected Lepton3.5 Shuttered (Radiometric)
	else if (strstr(leptonhw, "05-070170") != NULL)
	{
		leptonVersion = leptonVersion_3_5_shutter;
	}

	//Unsupported Lepton
	else
	{
		return false;
	}

	//Write to EEPROM
	EEPROM.write(eeprom_leptonVersion, leptonVersion);
	return true;
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
void lepton_setSysGainMode(byte mode)
{
	if (mode > 2)
	{
		return;
	}

	//The enum value is the LSB of DATA0
	byte data[4] =
		{0x00, mode, 0x00, 0x00};
	lepton_i2cWriteDataRegister(data, 4);

	// Execute the SYS Gain Mode Set Command, so that the module applies the values
	// 0x0200 (SDK Module ID) + 0x48 (SDK Command ID) + 0x1 (SET operation) + 0x0000 (Protection Bit) = 0x0249.
	lepton_i2c_execute_command(0x02, 0x49);
}

/*
 * Sets the SYS Gain Mode to high gain mode
 */
void lepton_setSysGainHigh()
{
	lepton_setSysGainMode(0x00);
}

/*
 * Sets the SYS Gain Mode to low gain mode
 */
void lepton_setSysGainLow()
{
	lepton_setSysGainMode(0x01);
}

/*
 * Sets the SYS Gain Mode to auto mode
 */
void lepton_setSysGainAuto()
{
	lepton_setSysGainMode(0x02);
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
int lepton_getSysGainMode()
{

	byte data[4];

	//SYS Gain Mode Get Command
	// 0x0200 (SDK Module ID) + 0x48 (SDK Command ID) + 0x0 (GET operation) + 0x0000 (Protection Bit) = 0x0248.
	lepton_i2c_execute_command(0x02, 0x48);
	lepton_i2cReadDataRegister(data, 4);

	//The enum value is the LSB of DATA0
	return data[1];
}

/*
 * Returns the  RAD T-Linear Resolution
 * 0: 100
 * 1: 10
 */
byte lepton_getRadTlinearResolution()
{

	byte data[4];

	// RAD T-Linear Resolution Get Command
	// 0x0E00 (SDK Module ID) + 0xC4 (SDK Command ID) + 0x0 (GET operation) + 0x4000 (Protection Bit) = 0x4EC4.
	lepton_i2c_execute_command(0x4E, 0xC4);
	lepton_i2cReadDataRegister(data, 4);

	//The enum value is the LSB of DATA0
	return data[1];
}

/*
 * Returns the RAD T-Linear Resolution Factor as a float
 * Returns -1 if the value could not be read
 */
float lepton_getResolution()
{
	byte resolution = lepton_getRadTlinearResolution();
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
void lepton_setRadTlinearResolution(byte resolution)
{
	if (resolution > 1)
	{
		return;
	}

	//The enum value is the LSB of DATA0
	byte data[4] =
		{0x00, resolution, 0x00, 0x00};
	lepton_i2cWriteDataRegister(data, 4);

	// RAD T-Linear Resolution Set Command
	// 0x0E00 (SDK Module ID) + 0xC4 (SDK Command ID) + 0x1 (SET operation) + 0x4000 (Protection Bit) = 0x4EC5.
	lepton_i2c_execute_command(0x4E, 0xC5);
}

/*
 * Sets the RAD T-Linear Resolution to 10 (factor 0.1)
 */
void lepton_setRadTlinear10()
{
	lepton_setRadTlinearResolution(0);
}

/*
 * Sets the RAD T-Linear Resolution to 100 (factor 0.01)
 */
void lepton_setRadTlinear100()
{
	lepton_setRadTlinearResolution(1);
}

/* Switch to low gain (-10 to +450 deg C) */
void lepton_setLowGain()
{
	lepton_setSysGainLow();
	lepton_setRadTlinear10();
	leptonCalSlope = 0.1;
	leptonGainMode = lepton_gain_low;
}

/* Switch to high gain (-10 to +140 deg C) */
void lepton_setHighGain()
{
	lepton_setSysGainHigh();
	lepton_setRadTlinear100();
	leptonCalSlope = 0.01;
	leptonGainMode = lepton_gain_high;
}

/* Check if Lepton is connected via SPI */
bool lepton_spiCheck()
{
	lepton_begin();
	long timer = millis();
	do
	{
		SPI1.transfer(lepton_packet, 164);
	}
	//Repeat as long as the frame is not valid, equals sync
	while (((lepton_packet[0] & 0x0F) == 0x0F) && ((millis() - timer) < 1000));
	lepton_end();

	//If sync not received after a second, return false
	if ((lepton_packet[0] & 0x0F) == 0x0F)
		return false;

	//Lepton is connected via SPI
	return true;
}

/* Init the FLIR Lepton LWIR sensor */
void lepton_init()
{
	//Check if SPI connection to Lepton works
	if (!lepton_spiCheck())
	{
		setDiagnostic(diag_lep_data);
		setDiagnostic(diag_lep_conf);
		return;
	}

	//Check the Lepton version
	if (!lepton_version())
	{
		setDiagnostic(diag_lep_conf);
		return;
	}

	//Enable VSync IRQ
	lepton_setGpioMode(true);

	//Do FFC
	lepton_ffc();

	autoMode = true;
	limitsLocked = false;
	leptonBufferValid = false;
}
