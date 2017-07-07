/*
*
* Camera - Visual camera module
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

//JPEG Decompressor
#include "tjpgd.h"
//Arducam - OV2640 sensor
#include "OV2640.h"
//PTC-06 or PTC-08 - VC0706 sensor
#include "VC0706.h"

/* Variables */

//JPEG Decompressor structure
typedef struct {
	const byte* jpic;
	unsigned short jsize;
	unsigned short joffset;
} IODEV;

//JPEG Decompressor
void* camera_jdwork;
JDEC camera_jd;
IODEV camera_iodev;

//Buffer to store the JPEG data
uint8_t* camera_jpegData;

//The current camera resolution
byte camera_resolution;


/* Methods */

/* Capture an image on the camera */
void camera_capture(void)
{
	//Arducam-Mini
	if (teensyVersion == teensyVersion_new)
		ov2640_capture();
	//PTC-06 or PTC-08
	else
		vc0706_capture();
}

/* Change the resolution of the camera */
void camera_changeRes(byte camRes)
{
	//If camera not working, skip
	if (!checkDiagnostic(diag_camera))
		return;

	//Change resolution
	camera_resolution = camRes;

	//According to camera resolution set
	switch (camera_resolution)
	{
		//Low resolution (160x120)
	case camera_resLow:
		//Arducam
		if (teensyVersion == teensyVersion_new)
			ov2640_setJPEGSize(OV2640_160x120);
		//PTC-06 or PTC-08
		else
			vc0706_changeCamRes(VC0706_160x120);
		break;
		//Middle resolution (320x240)
	case camera_resMiddle:
		// Arducam
		if (teensyVersion == teensyVersion_new)
			ov2640_setJPEGSize(OV2640_320x240);
		//PTC-06 or PTC-08
		else
			vc0706_changeCamRes(VC0706_320x240);
		break;
		//High resolution (640x480)
	case camera_resHigh:
		// Arducam
		if (teensyVersion == teensyVersion_new)
			ov2640_setJPEGSize(OV2640_640x480);
		//PTC-06 or PTC-08
		else
			vc0706_changeCamRes(VC0706_640x480);
		break;
	}

	//For OV2640, capture one frame and discard
	if (teensyVersion == teensyVersion_new)
		camera_capture();
}

/* Set the resolution to saving */
void camera_setSaveRes()
{
	camera_changeRes(camera_resHigh);
}

/* Set the resolution to streaming over serial */
void camera_setStreamRes()
{
	camera_changeRes(camera_resMiddle);
}

/* Set the resolution to display */
void camera_setDisplayRes()
{
	//Set camera resolution to low
	if ((teensyVersion == teensyVersion_old) || (!hqRes))
		camera_changeRes(camera_resLow);

	//Set to middle for HQ on Teensy 3.6
	else
		camera_changeRes(camera_resMiddle);
}

/* Init the camera module */
void camera_init(void)
{
	boolean init;

	//Allocate space for the decompressor
	camera_jdwork = malloc(3100);

	//Init Arducam-Mini
	if (teensyVersion == teensyVersion_new)
		init = ov2640_init();
	//Init PTC-06 or PTC-08
	else
		init = vc0706_init();

	//If init failed, set diagnostic
	if (!init) {
		setDiagnostic(diag_camera);
		return;
	}

	//Change resolution to save
	camera_setSaveRes();
}

/* Normal output function for the JPEG Decompressor - Teensy 3.6 */
unsigned int camera_decompOutNormal(JDEC * jd, void * bitmap, JRECT * rect)
{
	unsigned short * bmp = (unsigned short *)bitmap;
	unsigned short x, y;
	uint32_t count = 0;
	uint32_t imagepos;

	//Go through the image vertically
	for (y = rect->top; y <= rect->bottom; y++) {

		//Check if we draw inside the screen on y position
		if (((y + (5 * adjCombDown) <= 239) && (y - (5 * adjCombUp) >= 0))) {

			//Go through the image horizontally
			for (x = rect->left; x <= rect->right; x++) {

				//Check if we draw inside the screen on x position
				if (((x + (5 * adjCombRight) <= 319) && (x - (5 * adjCombLeft) >= 0))) {

					//Get the image position
					imagepos = x + (y * 320);

					//Do the visual alignment
					imagepos += 5 * adjCombRight;
					imagepos -= 5 * adjCombLeft;
					imagepos += 5 * adjCombDown * 320;
					imagepos -= 5 * adjCombUp * 320;

					//Do not use zero
					if (bmp[count] == 0)
						bmp[count] = 1;

					//Write to image buffer, rotated
					if (rotationVert)
						bigBuffer[76799 - imagepos] = bmp[count];
					//Write to image buffer, normal
					else
						bigBuffer[imagepos] = bmp[count];;
				}

				//Raise counter
				count++;
			}
		}
	}
	return 1;
}

/* Combined output function for the JPEG Decompressor - Teensy 3.1 / 3.2 only */
unsigned int camera_decompOutCombined(JDEC * jd, void * bitmap, JRECT * rect) {
	//Help Variables
	byte redV, greenV, blueV, redT, greenT, blueT, red, green, blue, xPos;
	unsigned short pixel, x, y, imagepos, count;
	unsigned short * bmp = (unsigned short *)bitmap;
	count = 0;
	//Go through the visual image
	for (y = rect->top; y <= rect->bottom; y++) {

		//Check if we draw inside the screen on y position
		if (((y + (5 * adjCombDown) <= 119) && (y - (5 * adjCombUp) >= 0))) {

			for (x = rect->left; x <= rect->right; x++) {

				//Mirror visual image for old HW
				if (mlx90614Version == mlx90614Version_old)
					xPos = (159 - x);
				else
					xPos = x;

				//Check if we draw inside the screen on x position
				if (((xPos + (5 * adjCombRight) <= 159) && (xPos - (5 * adjCombLeft) >= 0))) {

					//Get the image position
					imagepos = xPos + (y * 160);
					//Do the visual alignment
					imagepos += 5 * adjCombRight;
					imagepos -= 5 * adjCombLeft;
					imagepos += 5 * adjCombDown * 160;
					imagepos -= 5 * adjCombUp * 160;

					//Create combined pixel out of thermal and visual
					if (displayMode == displayMode_combined) {
						//Get the visual image color
						pixel = bmp[count++];
						//And extract the RGB values out of it
						redV = (pixel & 0xF800) >> 8;
						greenV = (pixel & 0x7E0) >> 3;
						blueV = (pixel & 0x1F) << 3;

						//Get the thermal image color, rotated
						if (rotationVert)
							pixel = smallBuffer[19199 - imagepos];
						//Get the thermal image color, normal
						else
							pixel = smallBuffer[imagepos];
						//And extract the RGB values out of it
						redT = (pixel & 0xF800) >> 8;
						greenT = (pixel & 0x7E0) >> 3;
						blueT = (pixel & 0x1F) << 3;

						//Mix both
						red = redT * (1 - adjCombAlpha) + redV * adjCombAlpha;
						green = greenT * (1 - adjCombAlpha) + greenV * adjCombAlpha;
						blue = blueT * (1 - adjCombAlpha) + blueV * adjCombAlpha;

						//Set the pixel to the calculated RGB565 value
						pixel = (((red & 248) | green >> 5) << 8)
							| ((green & 28) << 3 | blue >> 3);

					}

					//Set pixel to visual image only
					else
						pixel = bmp[count++];

					//Write to image buffer, rotated
					if (rotationVert)
						smallBuffer[19199 - imagepos] = pixel;
					//Write to image buffer, normal
					else
						smallBuffer[imagepos] = pixel;
				}
				//Raise counter if it is not inside the image
				else
					count++;
			}
		}
	}
	return 1;
}

/* Help function to insert the JPEG data into the decompressor */
unsigned int camera_decompIn(JDEC * jd, byte* buff, unsigned int ndata) {
	IODEV * dev = (IODEV *)jd->device;
	ndata = (unsigned int)dev->jsize - dev->joffset > ndata ?
		ndata : dev->jsize - dev->joffset;
	if (buff)
		memcpy(buff, dev->jpic + dev->joffset, ndata);
	dev->joffset += ndata;
	return ndata;
}

/* Transfer, decompress or save the visual image */
void camera_get(byte mode, char* dirname = NULL)
{
	uint32_t jpegLen;

	//Get length from Arducam for streaming
	if (teensyVersion == teensyVersion_new) {
		//Wait for image to be there
		while (!ov2640_getBit(ARDUCHIP_TRIG, CAP_DONE_MASK));
		//Get JPEG length
		jpegLen = ov2640_readFifoLength();
	}

	//Get length from PTC-06 or PTC-08
	else
		jpegLen = vc0706_frameLength();

	//Buffer for Teensy 3.1 / 3.2
	if (teensyVersion == teensyVersion_old)
	{
		//When streaming, allocate bytes
		if (mode == camera_stream)
			camera_jpegData = (uint8_t*)malloc(jpegLen);
		//For saving and serial, do write directly
		else
			camera_jpegData = NULL;
	}
	//Buffer for Teensy 3.6
	else
	{
		//If rotated and not streaming, add EXIF header
		if ((rotationVert) && (mode != camera_stream))
			camera_jpegData = (uint8_t*)malloc(jpegLen + 100);
		//Otherwise allocate byte for JPEG data only
		else
			camera_jpegData = (uint8_t*)malloc(jpegLen);
	}

	//Arducam
	if (teensyVersion == teensyVersion_new) {
		//Stream
		if (mode == camera_stream)
			ov2640_transfer(camera_jpegData, 1, &jpegLen);

		//Save
		else if (mode == camera_save) {
			//Transfer from camera
			ov2640_transfer(camera_jpegData, 0, &jpegLen);

			//Start SD transmission
			startAltClockline();

			//Create JPEG file
			createJPEGFile(dirname);

			//Write to SD file, EXIF included if rotated
			sdFile.write(camera_jpegData, jpegLen);

			//Close file
			sdFile.close();
			endAltClockline();

			//Free buffer
			free(camera_jpegData);
		}

		//Serial transfer
		else if (mode == camera_serial)
		{
			//Transfer from camera
			ov2640_transfer(camera_jpegData, 0, &jpegLen);
			
			//Send length
			Serial.write((jpegLen & 0xFF00) >> 8);
			Serial.write(jpegLen & 0x00FF);

			//Send JPEG bytestream to serial port
			Serial.write(camera_jpegData, jpegLen);

			//Free buffer
			free(camera_jpegData);
		}
	}
	//PTC-06 or PTC-08
	else
		vc0706_transfer(camera_jpegData, jpegLen, mode, dirname);

	//For streaming, decompress data and capture next frame
	if (mode == camera_stream) {

		//Decompress the image if not saving
		camera_iodev.jpic = camera_jpegData;
		camera_iodev.jsize = jpegLen;

		//the offset is zero
		camera_iodev.joffset = 0;

		//Prepare the image for convertion to RGB565
		jd_prepare(&camera_jd, camera_decompIn, camera_jdwork, 3100, &camera_iodev);

		//Decompress into 320x240 buffer
		if ((teensyVersion == teensyVersion_new) && (hqRes))
			jd_decomp(&camera_jd, camera_decompOutNormal, 0);

		//Decompress into 160x120 buffer, also with transparency
		else
			jd_decomp(&camera_jd, camera_decompOutCombined, 0);

		//Free the jpeg data array
		free(camera_jpegData);
	}
}
