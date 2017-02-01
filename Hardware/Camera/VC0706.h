/*
*
* VC0706 - Driver for the PTC-06 or PTC-08 camera module
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

/* Defines*/

#define VC0706_640x480 0x00
#define VC0706_320x240 0x11
#define VC0706_160x120 0x22

/* Variables */

uint8_t  serialNum = 0;
uint8_t  camerabuff[101];
uint8_t  bufferLen = 0;
uint16_t frameptr = 0;

//EXIF header for horizontal mirror in ThermocamV4
const uint8_t exifHeader_mirror[] =
{ 0xFF, 0xE1, 0x00, 0x62, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00, 0x4D, 0x4D, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x08,
0x00, 0x05, 0x01, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x01, 0x1A, 0x00, 0x05,
0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4A, 0x01, 0x1B, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
0x00, 0x52, 0x01, 0x28, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x02, 0x13, 0x00, 0x03,
0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00,
0x00, 0x01, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x01 };

/* Methods */

/* Send a specific command */
void vc0706_sendCommand(uint8_t cmd, uint8_t args[] = 0, uint8_t argn = 0) {
	Serial1.print(0x56, BYTE);
	Serial1.print(serialNum, BYTE);
	Serial1.print(cmd, BYTE);

	for (uint8_t i = 0; i < argn; i++) {
		Serial1.print(args[i], BYTE);
	}
}

/* Read the response from the camera */
uint8_t vc0706_readResponse(uint8_t numbytes, uint8_t timeout) {
	uint8_t counter = 0;
	bufferLen = 0;
	int avail;
	while ((timeout != counter) && (bufferLen != numbytes)) {
		avail = Serial1.available();
		if (avail <= 0) {
			delay(1);
			counter++;
			continue;
		}
		counter = 0;
		camerabuff[bufferLen++] = Serial1.read();
	}
	return bufferLen;
}

/* Verify the response from the camera */
boolean vc0706_verifyResponse(uint8_t command) {
	if ((camerabuff[0] != 0x76) ||
		(camerabuff[1] != serialNum) ||
		(camerabuff[2] != command) ||
		(camerabuff[3] != 0x0))
		return 0;
	return 1;
}

/* Run a specific command */
boolean vc0706_runCommand(uint8_t cmd, uint8_t *args, uint8_t argn,
	uint8_t resplen, boolean flushflag = 1) {
	if (flushflag) {
		vc0706_readResponse(100, 10);
	}
	vc0706_sendCommand(cmd, args, argn);
	if (vc0706_readResponse(resplen, 200) != resplen)
		return 0;
	if (!vc0706_verifyResponse(cmd))
		return 0;
	return 1;
}

/* Reset the connection */
boolean vc0706_reset(void)
{
	uint8_t args[] = { 0x0 };
	return vc0706_runCommand(0x26, args, 1, 5);
}

/* End the connection */
boolean vc0706_end() {
	uint8_t args[] = { 0x01, 0x03 };
	return vc0706_runCommand(0x36, args, sizeof(args), 5);
}


/* Change the baudrate */
boolean vc0706_changeBaudRate() {
	uint8_t args[] = { 0x03, 0x01, 0x0D, 0xA6 };
	return vc0706_runCommand(0x24, args, sizeof(args), 5);
}

/* Set the image size */
boolean vc0706_setImageSize(uint8_t x) {
	uint8_t args[] = { 0x05, 0x04, 0x01, 0x00, 0x19, x };
	return vc0706_runCommand(0x31, args, sizeof(args), 5);
}

/* Get the image size */
uint8_t vc0706_getImageSize() {
	uint8_t args[] = { 0x4, 0x4, 0x1, 0x00, 0x19 };
	if (!vc0706_runCommand(0x30, args, sizeof(args), 6))
		return -1;
	return camerabuff[5];
}

/* Set the compression ratio */
boolean vc0706_setCompression(uint8_t c) {
	uint8_t args[] = { 0x5, 0x1, 0x1, 0x12, 0x04, c };
	return vc0706_runCommand(0x31, args, sizeof(args), 5);
}

/* Transfer a package from the VC0706 camera */
void vc0706_transPackage(byte bytesToRead, uint8_t* buffer)
{
	int avail;
	//Send counter and buffer length to zero
	uint8_t counter = 0;
	uint8_t bufferLen = 0;
	//As long as no timeout and not all bytes read
	while ((10 != counter) && (bufferLen != (bytesToRead + 5))) {
		//Check how many bytes are available
		avail = Serial1.available();
		//If there are none, raise timeout counter
		if (avail <= 0) {
			delay(1);
			counter++;
			continue;
		}
		//Reset timeout counter if there is a packet
		counter = 0;
		//Add the data to the buffer
		buffer[bufferLen++] = Serial1.read();
	}
}

/* Control the framebuffer */
boolean vc0706_cameraFrameBuffCtrl(uint8_t command) {
	uint8_t args[] = { 0x1, command };
	return vc0706_runCommand(0x36, args, sizeof(args), 5);
}

/* Take a picture */
boolean vc0706_capture() {
	frameptr = 0;
	return vc0706_cameraFrameBuffCtrl(0x0);
}

/* Get the JPEG frame length */
uint32_t vc0706_frameLength(void) {
	uint8_t args[] = { 0x01, 0x00 };
	if (!vc0706_runCommand(0x34, args, sizeof(args), 9))
		return 0;
	uint32_t len;
	len = camerabuff[5];
	len <<= 8;
	len |= camerabuff[6];
	len <<= 8;
	len |= camerabuff[7];
	len <<= 8;
	len |= camerabuff[8];
	return len;
}

/* Check if data is available */
uint8_t vc0706_available(void) {
	return bufferLen;
}

/* Read the JPEG picture */
boolean vc0706_readPicture(uint8_t n) {
	uint8_t args[] = { 0x0C, 0x0, 0x0A,
		0, 0, (uint8_t)(frameptr >> 8), (uint8_t)(frameptr & 0xFF),
		0, 0, 0, n,
		10 >> 8, 10 & 0xFF };

	if (!vc0706_runCommand(0x32, args, sizeof(args), 5, 0))
		return 0;
	frameptr += n;
	return 1;
}

/* Transfer the JPEG bytestream*/
void vc0706_transfer(uint8_t* jpegData, uint16_t jpegLen, byte mode, char* dirname)
{
	//Count variable
	uint16_t counter = 0;

	//Create the buffer
	uint8_t* buffer = (uint8_t*)malloc(128 + 5);

	//For serial transfer, send frame length
	if (mode == camera_serial)
	{
		//When rotation is enabled or using the ThermocamV4, send EXIF
		if ((rotationEnabled && (mlx90614Version == mlx90614Version_new)) || (mlx90614Version == mlx90614Version_old))
		{
			Serial.write(((jpegLen + 100) & 0xFF00) >> 8);
			Serial.write((jpegLen + 100) & 0x00FF);
		}
		//Send JPEG Bytestream only
		else
		{
			Serial.write((jpegLen & 0xFF00) >> 8);
			Serial.write(jpegLen & 0x00FF);
		}
	}

	//For saving to SD card
	if (mode == camera_save)
	{
		//Start alternative clock line
		startAltClockline();
		//Create JPEG file
		createJPEGFile(dirname);
	}

	//Transfer data
	while (jpegLen > 0) {
		//Calculate the bytes left to read
		uint8_t bytesToRead = min(jpegLen, 128);

		//Send the read command
		if (!vc0706_readPicture(bytesToRead))
			continue;

		//Transfer a package
		vc0706_transPackage(bytesToRead, buffer);

		//For streaming, add it to the jpeg data buffer
		if (mode == camera_stream)
		{
			for (int i = 0; i < bytesToRead; i++) {
				jpegData[counter] = buffer[i];
				counter++;
			}
		}

		//For serial transfer
		if (mode == camera_serial)
		{
			//Rotation on DIY-Thermocam V1 or V2
			if ((counter == 0) && rotationEnabled && (mlx90614Version == mlx90614Version_new))
			{
				Serial.write(buffer, 40);
				Serial.write(exifHeader_rotated, 100);
				Serial.write(&buffer[40], (bytesToRead - 40));
				counter++;
			}
			//Mirror on ThermocamV4
			else if ((counter == 0) && (mlx90614Version == mlx90614Version_old))
			{
				Serial.write(buffer, 40);
				Serial.write(exifHeader_mirror, sizeof(exifHeader_mirror));
				Serial.write(&buffer[40], (bytesToRead - 40));
				counter++;
			}
			//No EXIF
			else
				Serial.write(buffer, bytesToRead);
		}

		//For saving to SD card
		if (mode == camera_save)
		{
			//Rotation on DIY-Thermocam V1 or V2
			if ((counter == 0) && rotationEnabled && (mlx90614Version == mlx90614Version_new))
			{
				sdFile.write(buffer, 40);
				sdFile.write(exifHeader_rotated, 100);
				sdFile.write(&buffer[40], (bytesToRead - 40));
				counter++;
			}
			//Mirror on ThermocamV4
			else if ((counter == 0) && (mlx90614Version == mlx90614Version_old))
			{
				sdFile.write(buffer, 40);
				sdFile.write(exifHeader_mirror, sizeof(exifHeader_mirror));
				sdFile.write(&buffer[40], (bytesToRead - 40));
				counter++;
			}
			//No EXIF
			else
				sdFile.write(buffer, bytesToRead);
		}

		//Substract transfered bytes from total
		jpegLen -= bytesToRead;
	}

	//Free the buffer
	free(buffer);

	//End transmission
	vc0706_end();

	//For saving to SD, close file
	if (mode == camera_save)
	{
		//Close the file
		sdFile.close();
		//End SD Transmission
		endAltClockline();
	}
}

/* Start connecting to the camera */
void vc0706_begin(void) {
	//Start with 38400 to send new baudrate
	Serial1.begin(38400);
	//Wait
	delay(15);
	//Change baudrate
	vc0706_changeBaudRate();
	//Reconnect using 115.2k
	Serial1.begin(115200);
	//Wait
	delay(15);
}

/* Init the camera module */
boolean vc0706_init(void)
{
	//Start connection at 115.2k Baud
	vc0706_begin();
	//Test if the camera works
	if (!vc0706_capture()) {
		//Try it again after 100ms
		delay(100);
		//Reset
		vc0706_reset();
		//Restart connection at 115.2k Baud
		vc0706_begin();
		//Try to take a picture again
		if (!vc0706_capture())
			return 0;
	}
	//Set compression
	vc0706_setCompression(95);
	//Skip the picture
	vc0706_end();
	//Everything working
	return 1;
}

/* Change the resolution of the device */
void vc0706_changeCamRes(uint8_t size) {
	//Change resolution
	vc0706_setImageSize(size);
	//Reset the device to change the resolution
	vc0706_reset();
	//Wait some time
	delay(300);
	//Re-establish the connection to the device
	vc0706_begin();
}