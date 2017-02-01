/*
*
* CALIBRATION - Functions to convert Lepton raw values to absolute values
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

/* Methods*/

/* Converts a given Temperature in Celcius to Fahrenheit */
float celciusToFahrenheit(float Tc) {
	float Tf = ((float) 9.0 / 5.0) * Tc + 32.0;
	return (Tf);
}

/* Converts a given temperature in Fahrenheit to Celcius */
float fahrenheitToCelcius(float Tf) {
	float Tc = (Tf - 32.0) * ((float) 9.0 / 5.0);
	return Tc;
}

/* Function to calculate temperature out of Lepton value */
float calFunction(uint16_t rawValue) {
	//Calculate offset out of ambient temp
	if ((calStatus != cal_manual) && (autoMode) && (!limitsLocked))
		calOffset = mlx90614_amb - (calSlope * 8192) + calComp;
	//Calculate the temperature in Celcius out of the coefficients
	float temp = (calSlope * rawValue) + calOffset;
	//Convert to Fahrenheit if needed
	if (tempFormat == tempFormat_fahrenheit)
		temp = celciusToFahrenheit(temp);
	return temp;
}

/* Calculate the lepton value out of an absolute temperature */
uint16_t tempToRaw(float temp) {
	//Convert to Celcius if needed
	if (tempFormat == tempFormat_fahrenheit)
		temp = fahrenheitToCelcius(temp);
	//Calculate offset out of ambient temp
	if ((calStatus != cal_manual) && (autoMode) && (!limitsLocked))
		calOffset = mlx90614_amb - (calSlope * 8192) + calComp;
	uint16_t rawValue = (temp - calOffset) / calSlope;
	return rawValue;
}

/* Calculates the average of the 196 (14x14) pixels in the middle */
uint16_t calcAverage() {
	int sum = 0;
	for (byte vert = 52; vert < 66; vert++) {
		for (byte horiz = 72; horiz < 86; horiz++) {
			uint16_t val = smallBuffer[(vert * 160) + horiz];
			//If one of the values contains hotter or colder values than the lepton can handle
			if ((val == 16383) || (val == 0))
				//Do not use that calibration set!
				return 0;
			sum += val;
		}
	}
	uint16_t avg = (uint16_t)(sum / 196.0);
	return avg;
}

/* Compensate the calibration with object temp */
void compensateCalib() {
	//Refresh MLX90614 ambient temp
	mlx90614_getAmb();
	///Refresh object temperature
	mlx90614_getTemp();
	//Convert to Fahrenheit if needed
	if (tempFormat == tempFormat_fahrenheit)
		mlx90614_temp = celciusToFahrenheit(mlx90614_temp);

	//Apply compensation if auto mode enabled, no limited locked and standard calib
	if ((autoMode) && (!limitsLocked) && (calStatus != cal_warmup)) {
		//Calculate min & max
		int16_t min = round(calFunction(minValue));
		int16_t max = round(calFunction(maxValue));
		//If spot temp is lower than current minimum by one degree, lower minimum
		if (mlx90614_temp < (min - 1))
			calComp = mlx90614_temp - min;
		//If spot temp is higher than current maximum by one degree, raise maximum
		else if (mlx90614_temp > (max + 1))
			calComp = mlx90614_temp - max;
	}
	//Calculate offset out of ambient temp
	if ((calStatus != cal_manual) && (autoMode) && (!limitsLocked))
		calOffset = mlx90614_amb - (calSlope * 8192) + calComp;
}

/* Checks if the calibration warmup is done */
void checkWarmup() {
	//Activate the calibration after a warmup time of 30s
	if (((calStatus == cal_warmup) && (millis() - calTimer > 30000))) {
		//Set calibration status to standard
		calStatus = cal_standard;

		//Disable auto FFC when isotherm mode
		if (hotColdMode != hotColdMode_disabled)
			lepton_ffcMode(false);
	}
}

/* Help function for least suqare fit */
inline static double sqr(double x) {
	return x*x;
}

/* Least square fit */
int linreg(int n, const uint16_t x[], const float y[], float* m, float* b, float* r)
{
	double   sumx = 0.0;
	double   sumx2 = 0.0;
	double   sumxy = 0.0;
	double   sumy = 0.0;
	double   sumy2 = 0.0;
	for (int i = 0; i < n; i++)
	{
		sumx += x[i];
		sumx2 += sqr(x[i]);
		sumxy += x[i] * y[i];
		sumy += y[i];
		sumy2 += sqr(y[i]);
	}
	double denom = (n * sumx2 - sqr(sumx));
	if (denom == 0) {
		//singular matrix. can't solve the problem.
		*m = 0;
		*b = 0;
		*r = 0;
		return 1;
	}
	*m = (n * sumxy - sumx * sumy) / denom;
	*b = (sumy * sumx2 - sumx * sumxy) / denom;
	if (r != NULL) {
		*r = (sumxy - sumx * sumy / n) /
			sqrt((sumx2 - sqr(sumx) / n) *
			(sumy2 - sqr(sumy) / n));
	}
	return 0;
}

/* Run the calibration process */
void calibrationProcess(bool serial, bool firstStart) {
	//Variables
	float calMLX90614[100];
	uint16_t calLepton[100];
	float calCorrelation;
	char result[30];
	uint16_t average;
	uint16_t average_old = 0;
	float mlx90614_old = 0;
	maxValue = 0;
	minValue = 65535;

	//Repeat as long as there is a good calibration
	do {
		//Show the screen when not using serial mode
		if (!serial)
			calibrationScreen(firstStart);

		//Reset counter to zero
		int counter = 0;

		//Perform FFC and set shutter mode to manual afterwards
		if (leptonShutter != leptonShutter_none) {
			lepton_ffc();
			lepton_ffcMode(false);
		}

		//Get 100 different calibration samples
		while (counter < 100) {
			//Store time elapsed
			long timeElapsed = millis();

			//Repeat measurement as long as there is no new average
			do {
				//Safe delay for bad PCB routing
				delay(10);
				//Get temperatures
				lepton_getRawValues();
				//Calculate the average
				average = calcAverage();
			} while ((average == average_old) || (average == 0));

			//Store old average
			average_old = average;

			//If the temperature changes too much, do not take that measurement
			if (abs(mlx90614_getTemp() - mlx90614_old) < 10) {
				//Store values
				calLepton[counter] = average;
				calMLX90614[counter] = mlx90614_temp;

				//Find minimum and maximum value
				if (average > maxValue)
					maxValue = average;
				if (average < minValue)
					minValue = average;

				//Refresh status on screen in steps of 10, not for serial
				if (((counter % 10) == 0) && !serial) {
					char buffer[20];
					sprintf(buffer, "Status: %2d%%", counter);
					display_print(buffer, CENTER, 140);
				}

				//When doing this in serial mode, print counter
				if (serial)
					Serial.write(counter);

				//Raise counter
				counter++;
			}

			//Store old spot temperature
			mlx90614_old = mlx90614_temp;

			//Wait at least 111ms between two measurements (9Hz)
			while ((millis() - timeElapsed) < 111);

			//If the user wants to abort and is not in first start or serial mode
			if (touch_touched() && !firstStart && !serial) {
				int pressedButton = buttons_checkButtons(true);
				//Abort
				if (pressedButton == 0)
					return;
			}
		}

		//Calculate the calibration formula with least square fit
		linreg(100, calLepton, calMLX90614, &calSlope, &calOffset, &calCorrelation);

		//Set calibration to manual
		calStatus = cal_manual;

		//Set compensation to zero
		calComp = 0;

		//When in serial mode, store and send ACK
		if (serial)
		{
			//Set shutter mode back to auto
			lepton_ffcMode(true);

			//Save calibration to EEPROM
			storeCalibration();

			//Send ACK
			Serial.write(CMD_SET_CALIBRATION);

			//Leave 
			return;
		}

		//In case the calibration was not good, ask to repeat
		if (calCorrelation < 0.5) {

			//When in first start mode
			if (firstStart) {
				showFullMessage((char*) "Bad calibration, try again!", true);
				delay(1000);
			}

			//If the user does not want to repeat, discard
			else if (!calibrationRepeat()) {
				calSlope = cal_stdSlope;
				calStatus = cal_standard;
				break;
			}
		}
	} while (calCorrelation < 0.5);

	//Show the result
	sprintf(result, "Slope: %1.4f, offset: %.1f", calSlope, calOffset);
	showFullMessage(result);
	delay(2000);
	
	//Set shutter mode back to auto
	lepton_ffcMode(true);

	//Save calibration to EEPROM
	storeCalibration();

	//Show message if not in first start menu
	if (firstStart == false) {
		showFullMessage((char*) "Calibration written to EEPROM!", true);
		delay(1000);
	}

	//Restore old font
	display_setFont(smallFont);
}

/* Calibration */
bool calibration() {
	//Still in warmup
	if (calStatus == cal_warmup) {
		showFullMessage((char*) "Please wait for sensor warmup!", true);
		delay(1000);
		return false;
	}

	//If there is a calibration
	else if (calStatus == cal_manual)
		return calibrationChooser();

	//If there is none, do a new one
	calibrationProcess();

	return true;
}