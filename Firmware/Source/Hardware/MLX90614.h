/*
*
* MLX90614 - Access the MLX90614 spot IR sensor
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

/* Variables */

//Registers
#define mlx90614_EEPROM 0x00
#define mlx90614_RAM 0x5A
#define mlx90614_AmbientTemp 0x06
#define mlx90614_ObjectTemp 0x07
#define mlx90614_toMax 0x20
#define mlx90614_toMin 0x21
#define mlx90614_PWMCTRL 0x22
#define mlx90614_TaRange 0x23
#define mlx90614_Emissivity 0x24
#define mlx90614_Filter 0x25

//CRC Table to calculate I2C PEC
const unsigned char mlx90614_crcTable[] = { 0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E,
0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF,
0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD, 0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD, 0xC7,
0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A, 0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D,
0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80, 0x95, 0x92,
0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4, 0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD,
0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10,
0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34, 0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78,
0x7F, 0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9,
0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83, 0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6,
0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3 };

/* Methods */

/* Calculate CRC8 checksum for PEC */
char mlx90614_crc8(byte buffer[], int len) {
	unsigned char m_crc = 0;
	int m_byte_count = 0;
	for (int i = 0; i < len; i++) {
		m_byte_count++;
		m_crc = mlx90614_crcTable[m_crc ^ buffer[i]];
	}
	return m_crc;
}

/* Receive data from the RAM over I2C */
uint16_t mlx90614_getRawData(boolean TaTo, boolean* check) {
	// Store the two relevant bytes of data for temperature
	byte dataLow, dataHigh;
	Wire.beginTransmission(mlx90614_RAM);
	//Measure Ambient Temp
	if (TaTo)
		Wire.send(mlx90614_AmbientTemp);
	//Measure Object Temp
	else {
		Wire.send(mlx90614_ObjectTemp);
	}
	Wire.endTransmission(I2C_NOSTOP);
	//Receive data	
	Wire.requestFrom(mlx90614_RAM, 3);
	if (Wire.available() != 3)
		*check = 0;
	dataLow = Wire.read();
	if (dataLow == -1)
		*check = 0;
	dataHigh = Wire.read();
	if (dataHigh == -1)
		*check = 0;
	byte pec = Wire.read();
	if (pec == -1)
		*check = 0;
	Wire.endTransmission();
	//Convert data
	uint16_t tempData = (((dataHigh & 0x007F) << 8) + dataLow);
	return tempData;
}

/* Send data to the EEPROM over I2C*/
void mlx90614_send(byte address, byte LSB, byte MSB) {
	Wire.beginTransmission(mlx90614_EEPROM);
	Wire.write(address);
	Wire.write(LSB);
	Wire.write(MSB);
	byte msg[4] = { 0x00, address, LSB, MSB };
	byte PEC = mlx90614_crc8(msg, 4);
	Wire.write(PEC);
	Wire.endTransmission();
}

/* Receive data from the EEPROM over I2C */
uint16_t mlx90614_receive(byte address, byte* error = NULL) {
	Wire.beginTransmission(mlx90614_EEPROM);
	Wire.write(address);
	Wire.endTransmission(I2C_NOSTOP);
	Wire.requestFrom(mlx90614_EEPROM, 3);
	byte LSB = Wire.read();
	byte MSB = Wire.read();
	Wire.read();
	//Check if the transmission worked
	if (error != NULL)
		*error = Wire.endTransmission();
	else
		Wire.endTransmission();
	//Calc 16-bit value and return
	uint16_t regValue = (((MSB) << 8) + LSB);
	return regValue;
}

/* Measures the ambient or object temperature */
float mlx90614_measure(boolean TaTo, boolean* check) {
	//Get the raw value from the sensor
	uint16_t rawData = mlx90614_getRawData(TaTo, check);

	//Convert to Kelvin
	float tempData = (rawData * 0.02) - 0.01;

	//Convert to Celsius
	tempData -= 273.15;

	//TaTo is one, check for ambient borders
	if ((TaTo) && ((tempData < -40) || (tempData > 125)))
		*check = false;

	//TaTo is zero, check for object borders
	else if ((tempData < -70) || (tempData > 380))
		*check = false;

	//Return temperature in Celsius
	return tempData;
}

/* Set the maximum temp to 380°C */
void mlx90614_setMax() {
	byte count = 0;
	do {
		//Set it to zero first
		mlx90614_send(mlx90614_toMax, 0x00, 0x00);
		delay(100);
		//Then write the new value
		mlx90614_send(mlx90614_toMax, 0x23, 0xFF);
		delay(100);
		//If we failed after 10 retries, set error and continue
		if (count == 10) {
			setDiagnostic(diag_spot);
			return;
		}
		count++;
	} while (mlx90614_receive(mlx90614_toMax) != 65315);
}

/* Check if the maximum temp is 380°C */
boolean mlx90614_checkMax() {
	//Check if maximum temp setting is correct
	if (mlx90614_receive(mlx90614_toMax) != 65315)
		return 0;
	//Everything was fine
	return 1;
}

/* Set the minimum temp to -70°C */
void mlx90614_setMin() {
	byte count = 0;
	do {
		//Set it to zero first
		mlx90614_send(mlx90614_toMin, 0x00, 0x00);
		delay(100);
		//Then write the new value
		mlx90614_send(mlx90614_toMin, 0x5B, 0x4F);
		delay(100);
		//If we failed after 10 retries, set error and continue
		if (count == 10) {
			setDiagnostic(diag_spot);
			return;
		}
		count++;
	} while (mlx90614_receive(mlx90614_toMin) != 20315);
}

/* Check if the minimum temp is -70°C */
boolean mlx90614_checkMin() {
	//Check if minimum temp setting is correct
	if (mlx90614_receive(mlx90614_toMin) != 20315)
		return 0;
	//Everything was fine
	return 1;
}

/* Set the emissivity to 0.9 */
void mlx90614_setEmissivity() {
	byte count = 0;
	do {
		//Set it to zero first
		mlx90614_send(mlx90614_Emissivity, 0x00, 0x00);
		delay(100);
		//Then write the new value
		mlx90614_send(mlx90614_Emissivity, 0x66, 0xE6);
		delay(100);
		//If we failed after 10 retries, set error and continue
		if (count == 10) {
			setDiagnostic(diag_spot);
			return;
		}
		count++;
	} while (mlx90614_receive(mlx90614_Emissivity) != 58982);
}

/* Check if the emissivity is set to 0.9 */
boolean mlx90614_checkEmissivity() {
	//Check if emissivity setting is correct
	if (mlx90614_receive(mlx90614_Emissivity) != 58982)
		return 0;
	//Everything was fine
	return 1;
}

/* Check if the filter settings match for noise-reduction (50% IIR, max settling time) */
void mlx90614_setFilter() {
	byte MSB, LSB;
	uint16_t filterSettings;
	//Old MLX90614 with gain factor of 12.5
	if (mlx90614Version == mlx90614Version_old) {
		filterSettings = 40820;
		MSB = 0x74;
		LSB = 0x9F;
	}

	//New MLX90614 with gain factor of 100
	else {
		filterSettings = 46964;
		MSB = 0x74;
		LSB = 0xB7;
	}
	//Try to set the new filter settings
	byte count = 0;
	do {
		//Set it to zero first
		mlx90614_send(mlx90614_Filter, 0x00, 0x00);
		delay(100);
		//Then write the new value
		mlx90614_send(mlx90614_Filter, MSB, LSB);
		delay(100);
		//If we failed after 10 retries, set error and continue
		if (count == 10) {
			setDiagnostic(diag_spot);
			return;
		}
		count++;
	} while (mlx90614_receive(mlx90614_Filter) != filterSettings);
}

/* Check if the filter settings match for fast measurement */
boolean mlx90614_checkFilter() {
	uint16_t filter = mlx90614_receive(mlx90614_Filter);
	//Old MLX90614 with gain factor of 12.5
	if ((mlx90614Version == mlx90614Version_old) && (filter != 40820))
		return 0;
	//New MLX90614 with gain factor of 100
	else if ((mlx90614Version == mlx90614Version_new) && (filter != 46964))
		return 0;
	///Everything was fine
	return 1;
}

/* Read and return the ambient temperature */
float mlx90614_getAmb() {
	float ambTemp;
	boolean check = 1;
	byte count = 0;

	//Repeat until valid value
	do {
		ambTemp = mlx90614_measure(1, &check);
		//If we cannot connect,continue
		if (count == 100)
			return 0;
		count++;
		delay(10);
	} while (check == 0);

	//Return ambient temperature
	return ambTemp;
}

/* Read and return the object temperature */
float mlx90614_getTemp() {
	float objTemp;
	boolean check = 1;
	byte count = 0;

	//Repeat until valid value
	do {
		objTemp = mlx90614_measure(0, &check);
		//If we cannot connect, continue
		if (count == 100)
			return 0;
		count++;
		delay(10);
	} while (check == 0);

	//Return object temperature
	return objTemp;
}

/* Initializes the sensor */
void mlx90614_init() {
	byte error;

	//For radiometric Lepton, do not init the MLX90614
	if ((leptonVersion == leptonVersion_2_5_shutter) || (leptonVersion == leptonVersion_3_5_shutter)) {
		mlx90614Version = mlx90614Version_new;
		return;
	}

	//Get filter settings
	uint16_t filter = mlx90614_receive(mlx90614_Filter, &error);

	//If I2C connection is not working, set diagnostic
	if (error != 0)
	{
		setDiagnostic(diag_spot);
		return;
	}

	//Calc MLX90614 version
	mlx90614Version = (filter >> 13) & 1;

	//If first start has been completed before, do checks
	if (EEPROM.read(eeprom_firstStart) == eeprom_setValue) {
		//Check Filter Temp
		if (mlx90614_checkFilter() == 0) {
			mlx90614_setFilter();
			setDiagnostic(diag_spot);
		}
		//Check Min Temp
		if (mlx90614_checkMin() == 0) {
			mlx90614_setMin();
			setDiagnostic(diag_spot);
		}
		//Check Max Temp
		if (mlx90614_checkMax() == 0) {
			mlx90614_setMax();
			setDiagnostic(diag_spot);
		}
		//Check Emissivity
		if (mlx90614_checkEmissivity() == 0) {
			mlx90614_setEmissivity();
			setDiagnostic(diag_spot);
		}
		//Show message, if one of the settings had to be re-written
		if (!checkDiagnostic(diag_spot)) {
			showFullMessage((char*)"Spot EEPROM updated, restart device");
			while (1);
		}
	}

	//Check if the object temp is valid
	boolean check = 1;
	byte count = 0;
	do {
		mlx90614_measure(0, &check);
		//If we cannot connect, set error and continue
		if (count == 100) {
			setDiagnostic(diag_spot);
			break;
		}
		count++;
		delay(10);
	} while (check == 0);

	//Check if the ambient temp is valid
	check = 1;
	count = 0;
	do {
		mlx90614_measure(1, &check);
		//If we cannot connect, set error and continue
		if (count == 100) {
			setDiagnostic(diag_spot);
			break;
		}
		count++;
		delay(10);
	} while (check == 0);
}