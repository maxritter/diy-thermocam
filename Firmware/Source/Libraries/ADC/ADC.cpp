/* Teensy 3.x, LC ADC library
 * https://github.com/pedvide/ADC
 * Copyright (c) 2015 Pedro Villanueva
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* ADC.cpp: Implements the control of one or more ADC modules of Teensy 3.x, LC
 *
 */

#include "ADC.h"



// translate pin number to SC1A nomenclature and viceversa
// we need to create this static const arrays so that we can assign the "normal arrays" to the correct one
// depending on which ADC module we will be.
/* channel2sc1aADCx converts a pin number to their value for the SC1A register, for the ADC0 and ADC1
*  numbers with +ADC_SC1A_PIN_MUX (128) means those pins use mux a, the rest use mux b.
*  numbers with +ADC_SC1A_PIN_DIFF (64) means it's also a differential pin (treated also in the channel2sc1a_diff_ADCx)
*  For diff_table_ADCx, +ADC_SC1A_PIN_PGA means the pin can use PGA on that ADC
*/

///////// ADC0
#if defined(ADC_TEENSY_3_0)
const uint8_t ADC::channel2sc1aADC0[]= { // new version, gives directly the sc1a number. 0x1F=31 deactivates the ADC.
    5, 14, 8, 9, 13, 12, 6, 7, 15, 4, 0, 19, 3, 21, // 0-13, we treat them as A0-A13
    5, 14, 8, 9, 13, 12, 6, 7, 15, 4, // 14-23 (A0-A9)
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, // 24-33
    0+ADC_SC1A_PIN_DIFF, 19+ADC_SC1A_PIN_DIFF, 3+ADC_SC1A_PIN_DIFF, 21+ADC_SC1A_PIN_DIFF, // 34-37 (A10-A13)
    26, 22, 23, 27, 29, 30 // 38-43: temp. sensor, VREF_OUT, A14, bandgap, VREFH, VREFL. A14 isn't connected to anything in Teensy 3.0.
};
#elif defined(ADC_TEENSY_3_1) // the only difference with 3.0 is that A13 is not connected to ADC0 and that T3.1 has PGA.
const uint8_t ADC::channel2sc1aADC0[]= { // new version, gives directly the sc1a number. 0x1F=31 deactivates the ADC.
    5, 14, 8, 9, 13, 12, 6, 7, 15, 4, 0, 19, 3, 31, // 0-13, we treat them as A0-A13
    5, 14, 8, 9, 13, 12, 6, 7, 15, 4, // 14-23 (A0-A9)
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, // 24-33
    0+ADC_SC1A_PIN_DIFF, 19+ADC_SC1A_PIN_DIFF, 3+ADC_SC1A_PIN_DIFF, 31+ADC_SC1A_PIN_DIFF, // 34-37 (A10-A13)
    26, 22, 23, 27, 29, 30 // 38-43: temp. sensor, VREF_OUT, A14, bandgap, VREFH, VREFL. A14 isn't connected to anything in Teensy 3.0.
};
#elif defined(ADC_TEENSY_LC)
// Teensy LC
const uint8_t ADC::channel2sc1aADC0[]= { // new version, gives directly the sc1a number. 0x1F=31 deactivates the ADC.
    5, 14, 8, 9, 13, 12, 6, 7, 15, 11, 0, 4+ADC_SC1A_PIN_MUX, 23, 31, // 0-13, we treat them as A0-A12 + A13= doesn't exist
    5, 14, 8, 9, 13, 12, 6, 7, 15, 11, // 14-23 (A0-A9)
    0+ADC_SC1A_PIN_DIFF, 4+ADC_SC1A_PIN_MUX+ADC_SC1A_PIN_DIFF, 23, 31, 31, 31, 31, 31, 31, 31, // 24-33 ((A10-A12) + nothing), A11 uses mux a
    31, 31, 31, 31, // 34-37 nothing
    26, 31, 31, 27, 29, 30 // 38-43: temp. sensor, , , bandgap, VREFH, VREFL.
};
#elif defined(ADC_TEENSY_3_5) || defined(ADC_TEENSY_3_6)
const uint8_t ADC::channel2sc1aADC0[]= { // new version, gives directly the sc1a number. 0x1F=31 deactivates the ADC.
    5, 14, 8, 9, 13, 12, 6, 7, 15, 4, 3, 31, 31, 31, // 0-13, we treat them as A0-A13
    5, 14, 8, 9, 13, 12, 6, 7, 15, 4, // 14-23 (A0-A9)
    31, 31, 31, 31, 31, 31, 31, // 24-30
    31, 31, 17, 18,// 31-34 A12, A13, A14, A15
    31, 31, 31, 31, 31,// 33-39
    3+ADC_SC1A_PIN_DIFF, 31+ADC_SC1A_PIN_DIFF, 23, 31 // 40-43: A10, A11 (cannot be read by ADC0), A21, A22
};
#endif // defined

///////// ADC1
#if defined(ADC_TEENSY_3_1)
const uint8_t ADC::channel2sc1aADC1[]= { // new version, gives directly the sc1a number. 0x1F=31 deactivates the ADC.
    31, 31, 8, 9, 31, 31, 31, 31, 31, 31, 3, 31, 0, 19, // 0-13, we treat them as A0-A13
    31, 31, 8, 9, 31, 31, 31, 31, 31, 31, // 14-23 (A0-A9)
    31, 31,  // 24,25 are digital only pins
    5+ADC_SC1A_PIN_MUX, 5, 4, 6, 7, 4+ADC_SC1A_PIN_MUX, 31, 31, // 26-33 26=5a, 27=5b, 28=4b, 29=6b, 30=7b, 31=4a, 32,33 are digital only
    3+ADC_SC1A_PIN_DIFF, 31+ADC_SC1A_PIN_DIFF, 0+ADC_SC1A_PIN_DIFF, 19+ADC_SC1A_PIN_DIFF, // 34-37 (A10-A13) A11 isn't connected.
    26, 18, 31, 27, 29, 30 // 38-43: temp. sensor, VREF_OUT, A14 (not connected), bandgap, VREFH, VREFL.
};
#elif defined(ADC_TEENSY_3_5) || defined(ADC_TEENSY_3_6)
const uint8_t ADC::channel2sc1aADC1[]= { // new version, gives directly the sc1a number. 0x1F=31 deactivates the ADC.
    31, 31, 8, 9, 31, 31, 31, 31, 31, 31, 31, 19, 14, 15, // 0-13, we treat them as A0-A13
    31, 31, 8, 9, 31, 31, 31, 31, 31, 31, // 14-23 (A0-A9)
    31, 31, 31, 31, 31, 31, 31,  // 24-30
    14, 15, 31, 31, 4, 5, 6, 7, 17, // 31-39 A12-A20
    31+ADC_SC1A_PIN_DIFF, 19+ADC_SC1A_PIN_DIFF, 31, 23 // 40-43: A10, A11, A21, A22
};
#endif

#if defined(ADC_TEENSY_3_1) // Teensy 3.1
    const ADC_Module::ADC_NLIST ADC::diff_table_ADC0[]= {
        {A10, 0+ADC_SC1A_PIN_PGA}, {A12, 3}
    };
    const ADC_Module::ADC_NLIST ADC::diff_table_ADC1[]= {
        {A10, 3}, {A12, 0+ADC_SC1A_PIN_PGA}
    };
#elif defined(ADC_TEENSY_3_0) // Teensy 3.0
    const ADC_Module::ADC_NLIST ADC::diff_table_ADC0[]= {
        {A10, 0}, {A12, 3}
    };
#elif defined(ADC_TEENSY_LC) // Teensy LC
    const ADC_Module::ADC_NLIST ADC::diff_table_ADC0[]= {
        {A10, 0}
    };
#elif defined(ADC_TEENSY_3_5) || defined(ADC_TEENSY_3_6) // Teensy 3.5// Teensy 3.6
    const ADC_Module::ADC_NLIST ADC::diff_table_ADC0[]= {
        {A10, 3}
    };
    const ADC_Module::ADC_NLIST ADC::diff_table_ADC1[]= {
        {A10, 0}
    };
#endif



// translate SC1A to pin number
///////// ADC0
#if defined(ADC_TEENSY_3_0) || defined(ADC_TEENSY_3_1)
const uint8_t ADC::sc1a2channelADC0[]= { // new version, gives directly the pin number
    34, 0, 0, 36, 23, 14, 20, 21, 16, 17, 0, 0, 19, 18, // 0-13
    15, 22, 23, 0, 0, 35, 0, 37, // 14-21
    39, 40, 0, 0, 38, 41, 42, 43, // VREF_OUT, A14, temp. sensor, bandgap, VREFH, VREFL.
    0 // 31 means disabled, but just in case
};
#elif defined(ADC_TEENSY_LC)
// Teensy LC
const uint8_t ADC::sc1a2channelADC0[]= { // new version, gives directly the pin number
    24, 0, 0, 0, 25, 14, 20, 21, 16, 17, 0, 23, 19, 18, // 0-13
    15, 22, 23, 0, 0, 0, 0, 0, // 14-21
    26, 0, 0, 0, 38, 41, 0, 42, 43, // A12, temp. sensor, bandgap, VREFH, VREFL.
    0 // 31 means disabled, but just in case
};
#elif defined(ADC_TEENSY_3_5) || defined(ADC_TEENSY_3_6)
const uint8_t ADC::sc1a2channelADC0[]= { // new version, gives directly the pin number
    34, 0, 0, 36, 23, 14, 20, 21, 16, 17, 0, 0, 19, 18, // 0-13
    15, 22, 23, 0, 0, 35, 0, 37, // 14-21
    39, 40, 0, 0, 38, 41, 42, 43, // VREF_OUT, A14, temp. sensor, bandgap, VREFH, VREFL.
    0 // 31 means disabled, but just in case
};
#endif // defined

///////// ADC1
#if defined(ADC_TEENSY_3_1)
const uint8_t ADC::sc1a2channelADC1[]= { // new version, gives directly the pin number
    36, 0, 0, 34, 28, 26, 29, 30, 16, 17, 0, 0, 0, 0, // 0-13. 5a=26, 5b=27, 4b=28, 4a=31
    0, 0, 0, 0, 39, 37, 0, 0, // 14-21
    0, 0, 0, 0, 38, 41, 0, 42, // 22-29. VREF_OUT, A14, temp. sensor, bandgap, VREFH, VREFL.
    43
};
#elif defined(ADC_TEENSY_3_5) || defined(ADC_TEENSY_3_6)
const uint8_t ADC::sc1a2channelADC1[]= { // new version, gives directly the pin number
    36, 0, 0, 34, 28, 26, 29, 30, 16, 17, 0, 0, 0, 0, // 0-13. 5a=26, 5b=27, 4b=28, 4a=31
    0, 0, 0, 0, 39, 37, 0, 0, // 14-21
    0, 0, 0, 0, 38, 41, 0, 42, // 22-29. VREF_OUT, A14, temp. sensor, bandgap, VREFH, VREFL.
    43
};
#endif


// Constructor
ADC::ADC() : // awkward initialization  so there are no -Wreorder warnings
    adc0_obj(0, channel2sc1aADC0, diff_table_ADC0)
    #if ADC_NUM_ADCS>1
    , adc1_obj(1, channel2sc1aADC1, diff_table_ADC1)
    #endif
    , adc0(&adc0_obj)
    #if ADC_NUM_ADCS>1
    , adc1(&adc1_obj)
    #endif
    {
    //ctor

    //digitalWriteFast(LED_BUILTIN, HIGH);

    // make sure the clocks to the ADC are on
    SIM_SCGC6 |= SIM_SCGC6_ADC0;
    #if ADC_NUM_ADCS>1
    SIM_SCGC3 |= SIM_SCGC3_ADC1;
    #endif

}



/* Set the voltage reference you prefer,
*  type can be ADC_REF_3V3, ADC_REF_1V2 (not for Teensy LC) or ADC_REF_EXT
*/
void ADC::setReference(uint8_t type, int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->setReference(type);
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->setReference(type); // adc_num isn't changed or has selected ADC0
    return;
}


// Change the resolution of the measurement.
/*
*  \param bits is the number of bits of resolution.
*  For single-ended measurements: 8, 10, 12 or 16 bits.
*  For differential measurements: 9, 11, 13 or 16 bits.
*  If you want something in between (11 bits single-ended for example) select the inmediate higher
*  and shift the result one to the right.
*  If you select, for example, 9 bits and then do a single-ended reading, the resolution will be adjusted to 8 bits
*  In this case the comparison values will still be correct for analogRead and analogReadDifferential, but not
*  for startSingle* or startContinous*, so whenever you change the resolution, change also the comparison values.
*/
void ADC::setResolution(uint8_t bits, int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->setResolution(bits);
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->setResolution(bits); // adc_num isn't changed or has selected ADC0
    return;
}

//! Returns the resolution of the ADC_Module.
uint8_t ADC::getResolution(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        return adc1->getResolution();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return 0;
    }
    return adc0->getResolution(); // adc_num isn't changed or has selected ADC0

}

//! Returns the maximum value for a measurement.
uint32_t ADC::getMaxValue(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        return adc1->getMaxValue();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return 1;
    }
    return adc0->getMaxValue();
}


// Sets the conversion speed
/*
* \param speed can be ADC_LOW_SPEED, ADC_MED_SPEED or ADC_HIGH_SPEED
*
*  It recalibrates at the end.
*/
void ADC::setConversionSpeed(uint8_t speed, int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->setConversionSpeed(speed);
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->setConversionSpeed(speed); // adc_num isn't changed or has selected ADC0
    return;

}


// Sets the sampling speed
/*
* \param speed can be ADC_LOW_SPEED, ADC_MED_SPEED or ADC_HIGH_SPEED
*
*  It recalibrates at the end.
*/
void ADC::setSamplingSpeed(uint8_t speed, int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->setSamplingSpeed(speed);
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->setSamplingSpeed(speed); // adc_num isn't changed or has selected ADC0
    return;

}


// Set the number of averages
/*
* \param num can be 0, 4, 8, 16 or 32.
*/
void ADC::setAveraging(uint8_t num, int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->setAveraging(num);
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->setAveraging(num); // adc_num isn't changed or has selected ADC0
    return;
}


//! Enable interrupts
/** An IRQ_ADC0 Interrupt will be raised when the conversion is completed
*  (including hardware averages and if the comparison (if any) is true).
*/
void ADC::enableInterrupts(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->enableInterrupts();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->enableInterrupts();
    return;
}

//! Disable interrupts
void ADC::disableInterrupts(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->disableInterrupts();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->disableInterrupts();
    return;
}


//! Enable DMA request
/** An ADC DMA request will be raised when the conversion is completed
*  (including hardware averages and if the comparison (if any) is true).
*/
void ADC::enableDMA(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->enableDMA();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->enableDMA();
    return;
}

//! Disable ADC DMA request
void ADC::disableDMA(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->disableDMA();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->disableDMA();
    return;
}


// Enable the compare function to a single value
/* A conversion will be completed only when the ADC value
*  is >= compValue (greaterThan=true) or < compValue (greaterThan=false)
*  Call it after changing the resolution
*  Use with interrupts or poll conversion completion with isComplete()
*/
void ADC::enableCompare(int16_t compValue, bool greaterThan, int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->enableCompare(compValue, greaterThan);
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->enableCompare(compValue, greaterThan);
    return;
}

// Enable the compare function to a range
/* A conversion will be completed only when the ADC value is inside (insideRange=1) or outside (=0)
*  the range given by (lowerLimit, upperLimit),including (inclusive=1) the limits or not (inclusive=0).
*  See Table 31-78, p. 617 of the freescale manual.
*  Call it after changing the resolution
*  Use with interrupts or poll conversion completion with isComplete()
*/
void ADC::enableCompareRange(int16_t lowerLimit, int16_t upperLimit, bool insideRange, bool inclusive, int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->enableCompareRange(lowerLimit, upperLimit, insideRange, inclusive);
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->enableCompareRange(lowerLimit, upperLimit, insideRange, inclusive);
    return;
}

//! Disable the compare function
void ADC::disableCompare(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->disableCompare();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->disableCompare();
    return;
}


// Enable and set PGA
/* Enables the PGA and sets the gain
*   Use only for signals lower than 1.2 V
*   \param gain can be 1, 2, 4, 8, 16, 32 or 64
*
*/
void ADC::enablePGA(uint8_t gain, int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->enablePGA(gain);
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->enablePGA(gain);
    return;
}

//! Returns the PGA level
/** PGA level = 2^gain, from 0 to 64
*/
uint8_t ADC::getPGA(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        return adc1->getPGA();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        return 1;
        #endif
    }
    return adc0->getPGA();
}

//! Disable PGA
void ADC::disablePGA(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->disablePGA();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->disablePGA();
    return;
}

//! Is the ADC converting at the moment?
bool ADC::isConverting(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        return adc1->isConverting();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return false;
    }
    return adc0->isConverting();
}

// Is an ADC conversion ready?
/*
*  \return 1 if yes, 0 if not.
*  When a value is read this function returns 0 until a new value exists
*  So it only makes sense to call it before analogReadContinuous() or readSingle()
*/
bool ADC::isComplete(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        return adc1->isComplete();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return false;
    }
    return adc0->isComplete();;
}

//! Is the ADC in differential mode?
bool ADC::isDifferential(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        return adc1->isDifferential();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return false;
    }
    return adc0->isDifferential();
}

//! Is the ADC in continuous mode?
bool ADC::isContinuous(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        return adc1->isContinuous();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return false;
    }
    return adc0->isContinuous();
}


/* Returns the analog value of the pin.
* It waits until the value is read and then returns the result.
* If a comparison has been set up and fails, it will return ADC_ERROR_VALUE.
* This function is interrupt safe, so it will restore the adc to the state it was before being called
* If more than one ADC exists, it will select the module with less workload, you can force a selection using
* adc_num. If you select ADC1 in Teensy 3.0 it will return ADC_ERROR_VALUE.
*/
int ADC::analogRead(uint8_t pin, int8_t adc_num) {
    #if ADC_NUM_ADCS==1
    /* Teensy 3.0, LC
    */
    if( adc_num==1 ) { // If asked to use ADC1, return error
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        return ADC_ERROR_VALUE;
    }
    return adc0->analogRead(pin); // use ADC0
    #elif ADC_NUM_ADCS==2
    /* Teensy 3.1
    */
    if( adc_num==-1 ) { // use no ADC in particular
        // check which ADC can read the pin
        bool adc0Pin = adc0->checkPin(pin);
        bool adc1Pin = adc1->checkPin(pin);

        if(adc0Pin && adc1Pin)  { // Both ADCs
            if( (adc0->num_measurements) > (adc1->num_measurements)) { // use the ADC with less workload
                return adc1->analogRead(pin);
            } else {
                return adc0->analogRead(pin);
            }
        } else if(adc0Pin) { // ADC0
            return adc0->analogRead(pin);
        } else if(adc1Pin) { // ADC1
            return adc1->analogRead(pin);
        } else { // pin not valid in any ADC
            adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
            adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
            return ADC_ERROR_VALUE;   // all others are invalid
        }
    }
    else if( adc_num==0 ) { // user wants ADC0
        return adc0->analogRead(pin);
    }
    else if( adc_num==1 ){ // user wants ADC 1
        return adc1->analogRead(pin);
    }
    adc0->fail_flag |= ADC_ERROR_OTHER;
    return ADC_ERROR_VALUE;
    #endif
}

/* Reads the differential analog value of two pins (pinP - pinN).
* It waits until the value is read and then returns the result.
* If a comparison has been set up and fails, it will return ADC_ERROR_VALUE.
* \param pinP must be A10 or A12.
* \param pinN must be A11 (if pinP=A10) or A13 (if pinP=A12).
* Other pins will return ADC_ERROR_VALUE.
* This function is interrupt safe, so it will restore the adc to the state it was before being called
* If more than one ADC exists, it will select the module with less workload, you can force a selection using
* adc_num. If you select ADC1 in Teensy 3.0 it will return ADC_ERROR_VALUE.
*/
int ADC::analogReadDifferential(uint8_t pinP, uint8_t pinN, int8_t adc_num) {

    #if ADC_NUM_ADCS==1
    /* Teensy 3.0, LC
    */
    if( adc_num==1 ) { // If asked to use ADC1, return error
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        return ADC_ERROR_VALUE;
    }
    return adc0->analogReadDifferential(pinP, pinN); // use ADC0
    #elif ADC_NUM_ADCS==2
    /* Teensy 3.1
    */
    if( adc_num==-1 ) { // use no ADC in particular
        // check which ADC can read the pin
        bool adc0Pin = adc0->checkDifferentialPins(pinP, pinN);
        bool adc1Pin = adc1->checkDifferentialPins(pinP, pinN);

        if(adc0Pin && adc1Pin)  { // Both ADCs
            if( (adc0->num_measurements) > (adc1->num_measurements)) { // use the ADC with less workload
                return adc1->analogReadDifferential(pinP, pinN);
            } else {
                return adc0->analogReadDifferential(pinP, pinN);
            }
        } else if(adc0Pin) { // ADC0
            return adc0->analogReadDifferential(pinP, pinN);
        } else if(adc1Pin) { // ADC1
            return adc1->analogReadDifferential(pinP, pinN);
        } else { // pins not valid in any ADC
            adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
            adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
            return ADC_ERROR_VALUE;   // all others are invalid
        }
    }
    else if( adc_num==0 ) { // user wants ADC0
        return adc0->analogReadDifferential(pinP, pinN);
    }
    else if( adc_num==1 ){ // user wants ADC 1
        return adc1->analogReadDifferential(pinP, pinN);
    }
    adc0->fail_flag |= ADC_ERROR_OTHER;
    return ADC_ERROR_VALUE;
    #endif
}


// Starts an analog measurement on the pin and enables interrupts.
/* It returns immediately, get value with readSingle().
*   If the pin is incorrect it returns ADC_ERROR_VALUE
*   This function is interrupt safe. The ADC interrupt will restore the adc to its previous settings and
*   restart the adc if it stopped a measurement. If you modify the adc_isr then this won't happen.
*/
bool ADC::startSingleRead(uint8_t pin, int8_t adc_num) {
    #if ADC_NUM_ADCS==1
    /* Teensy 3.0, LC
    */
    if( adc_num==1 ) { // If asked to use ADC1, return error
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        return false;
    }
    return adc0->startSingleRead(pin); // use ADC0
    #elif ADC_NUM_ADCS==2
    /* Teensy 3.1
    */
    if( adc_num==-1 ) { // use no ADC in particular
        // check which ADC can read the pin
        bool adc0Pin = adc0->checkPin(pin);
        bool adc1Pin = adc1->checkPin(pin);

        if(adc0Pin && adc1Pin)  { // Both ADCs

            if( (adc0->num_measurements) > (adc1->num_measurements)) { // use the ADC with less workload
                return adc1->startSingleRead(pin);
            } else {
                return adc0->startSingleRead(pin);
            }
        } else if(adc0Pin) { // ADC0
            return adc0->startSingleRead(pin);
        } else if(adc1Pin) { // ADC1
            return adc1->startSingleRead(pin);
        } else { // pin not valid in any ADC
            adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
            adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
            return false;   // all others are invalid
        }
    }
    else if( adc_num==0 ) { // user wants ADC0
        return adc0->startSingleRead(pin);
    }
    else if( adc_num==1 ){ // user wants ADC 1
        return adc1->startSingleRead(pin);
    }
    adc0->fail_flag |= ADC_ERROR_OTHER;
    return false;
    #endif
}

// Start a differential conversion between two pins (pinP - pinN) and enables interrupts.
/* It returns inmediately, get value with readSingle().
*   \param pinP must be A10 or A12.
*   \param pinN must be A11 (if pinP=A10) or A13 (if pinP=A12).
*   Other pins will return ADC_ERROR_DIFF_VALUE.
*   This function is interrupt safe. The ADC interrupt will restore the adc to its previous settings and
*   restart the adc if it stopped a measurement. If you modify the adc_isr then this won't happen.
*/
bool ADC::startSingleDifferential(uint8_t pinP, uint8_t pinN, int8_t adc_num) {
    #if ADC_NUM_ADCS==1
    /* Teensy 3.0, LC
    */
    if( adc_num==1 ) { // If asked to use ADC1, return error
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        return false;
    }
    return adc0->startSingleDifferential(pinP, pinN); // use ADC0
    #elif ADC_NUM_ADCS==2
    /* Teensy 3.1
    */
    if( adc_num==-1 ) { // use no ADC in particular
        // check which ADC can read the pin
        bool adc0Pin = adc0->checkDifferentialPins(pinP, pinN);
        bool adc1Pin = adc1->checkDifferentialPins(pinP, pinN);

        if(adc0Pin && adc1Pin)  { // Both ADCs
            if( (adc0->num_measurements) > (adc1->num_measurements)) { // use the ADC with less workload
                return adc1->startSingleDifferential(pinP, pinN);
            } else {
                return adc0->startSingleDifferential(pinP, pinN);
            }
        } else if(adc0Pin) { // ADC0
            return adc0->startSingleDifferential(pinP, pinN);
        } else if(adc1Pin) { // ADC1
            return adc1->startSingleDifferential(pinP, pinN);
        } else { // pins not valid in any ADC
            adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
            adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
            return false;   // all others are invalid
        }
    }
    else if( adc_num==0 ) { // user wants ADC0
        return adc0->startSingleDifferential(pinP, pinN);
    }
    else if( adc_num==1 ){ // user wants ADC 1
        return adc1->startSingleDifferential(pinP, pinN);
    }
    adc0->fail_flag |= ADC_ERROR_OTHER;
    return false;
    #endif
}

// Reads the analog value of a single conversion.
/* Set the conversion with with startSingleRead(pin) or startSingleDifferential(pinP, pinN).
*   \return the converted value.
*/
int ADC::readSingle(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        return adc1->readSingle();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        return ADC_ERROR_VALUE;
        #endif
    }
    return adc0->readSingle();
}


// Starts continuous conversion on the pin.
/* It returns as soon as the ADC is set, use analogReadContinuous() to read the value.
*/
bool ADC::startContinuous(uint8_t pin, int8_t adc_num) {

    #if ADC_NUM_ADCS==1
    /* Teensy 3.0, LC
    */
    if( adc_num==1 ) { // If asked to use ADC1, return error
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        return false;
    }
    return adc0->startContinuous(pin); // use ADC0
    #elif ADC_NUM_ADCS==2
    /* Teensy 3.1
    */
    if( adc_num==-1 ) { // use no ADC in particular
        // check which ADC can read the pin
        bool adc0Pin = adc0->checkPin(pin);
        bool adc1Pin = adc1->checkPin(pin);

        if(adc0Pin && adc1Pin)  { // Both ADCs
            if( (adc0->num_measurements) > (adc1->num_measurements)) { // use the ADC with less workload
                return adc1->startContinuous(pin);
            } else {
                return adc0->startContinuous(pin);
            }
        } else if(adc0Pin) { // ADC0
            return adc0->startContinuous(pin);
        } else if(adc1Pin) { // ADC1
            return adc1->startContinuous(pin);
        } else { // pin not valid in any ADC
            adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
            adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
            return false;   // all others are invalid
        }
    }
    else if( adc_num==0 ) { // user wants ADC0
        return adc0->startContinuous(pin);
    }
    else if( adc_num==1 ){ // user wants ADC 1
        return adc1->startContinuous(pin);
    }
    adc0->fail_flag |= ADC_ERROR_OTHER;
    return false;
    #endif
}

// Starts continuous conversion between the pins (pinP-pinN).
/* It returns as soon as the ADC is set, use analogReadContinuous() to read the value.
* \param pinP must be A10 or A12.
* \param pinN must be A11 (if pinP=A10) or A13 (if pinP=A12).
* Other pins will return ADC_ERROR_DIFF_VALUE.
*/
bool ADC::startContinuousDifferential(uint8_t pinP, uint8_t pinN, int8_t adc_num) {
    #if ADC_NUM_ADCS==1
    /* Teensy 3.0, LC
    */
    if( adc_num==1 ) { // If asked to use ADC1, return error
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        return false;
    }
    return adc0->startContinuousDifferential(pinP, pinN); // use ADC0
    #elif ADC_NUM_ADCS==2
    /* Teensy 3.1
    */
    if( adc_num==-1 ) { // use no ADC in particular
        // check which ADC can read the pin
        bool adc0Pin = adc0->checkDifferentialPins(pinP, pinN);
        bool adc1Pin = adc1->checkDifferentialPins(pinP, pinN);

        if(adc0Pin && adc1Pin)  { // Both ADCs
            if( (adc0->num_measurements) > (adc1->num_measurements)) { // use the ADC with less workload
                return adc1->startContinuousDifferential(pinP, pinN);
            } else {
                return adc0->startContinuousDifferential(pinP, pinN);
            }
        } else if(adc0Pin) { // ADC0
            return adc0->startContinuousDifferential(pinP, pinN);
        } else if(adc1Pin) { // ADC1
            return adc1->startContinuousDifferential(pinP, pinN);
        } else { // pins not valid in any ADC
            adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
            adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
            return false;   // all others are invalid
        }
    }
    else if( adc_num==0 ) { // user wants ADC0
        return adc0->startContinuousDifferential(pinP, pinN);
    }
    else if( adc_num==1 ){ // user wants ADC 1
        return adc1->startContinuousDifferential(pinP, pinN);
    }
    adc0->fail_flag |= ADC_ERROR_OTHER;
    return false;
    #endif
}

//! Reads the analog value of a continuous conversion.
/** Set the continuous conversion with with analogStartContinuous(pin) or startContinuousDifferential(pinP, pinN).
*   \return the last converted value.
*   If single-ended and 16 bits it's necessary to typecast it to an unsigned type (like uint16_t),
*   otherwise values larger than 3.3/2 V are interpreted as negative!
*/
int ADC::analogReadContinuous(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2
        return adc1->analogReadContinuous();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return false;
    }
    return adc0->analogReadContinuous();
}

//! Stops continuous conversion
void ADC::stopContinuous(int8_t adc_num) {
    if(adc_num==1){ // user wants ADC 1, do nothing if it's a Teensy 3.0
        #if ADC_NUM_ADCS>=2 // Teensy 3.1
        adc1->stopContinuous();
        #else
        adc0->fail_flag |= ADC_ERROR_WRONG_ADC;
        #endif
        return;
    }
    adc0->stopContinuous();
    return;
}



//////////////// SYNCHRONIZED BLOCKING METHODS //////////////////
///// IF THE BOARD HAS ONLY ONE ADC, THEY ARE EMPYT METHODS /////
/////////////////////////////////////////////////////////////////

#if ADC_NUM_ADCS>1

/*Returns the analog values of both pins, measured at the same time by the two ADC modules.
* It waits until the value is read and then returns the result as a struct Sync_result,
* use Sync_result.result_adc0 and Sync_result.result_adc1.
* If a comparison has been set up and fails, it will return ADC_ERROR_VALUE in both fields of the struct.
*/
ADC::Sync_result ADC::analogSynchronizedRead(uint8_t pin0, uint8_t pin1) {

    Sync_result res = {ADC_ERROR_VALUE, ADC_ERROR_VALUE};

    // check pins
    if ( !adc0->checkPin(pin0) ) {
        adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
        return res;
    }
    if ( !adc1->checkPin(pin1) ) {
        adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
        return res;
    }


    // check if we are interrupting a measurement, store setting if so.
    // vars to save the current state of the ADC in case it's in use
    ADC_Module::ADC_Config old_adc0_config = {0};
    uint8_t wasADC0InUse = adc0->isConverting(); // is the ADC running now?
    if(wasADC0InUse) { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc0->saveConfig(&old_adc0_config);
        __enable_irq();
    }
    ADC_Module::ADC_Config old_adc1_config = {0};
    uint8_t wasADC1InUse = adc1->isConverting(); // is the ADC running now?
    if(wasADC1InUse) { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc1->saveConfig(&old_adc1_config);
        __enable_irq();
    }

    // no continuous mode
    adc0->singleMode();
    adc1->singleMode();

    // start both measurements
    adc0->startReadFast(pin0);
    adc1->startReadFast(pin1);

    // wait for both ADCs to finish
    while( (adc0->isConverting()) || (adc1->isConverting()) ) { // wait for both to finish
        yield();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
    }


    __disable_irq(); // make sure nothing interrupts this part
    if ( adc0->isComplete() ) { // conversion succeded
        res.result_adc0 = adc0->readSingle();
    } else { // comparison was false
        adc0->fail_flag |= ADC_ERROR_COMPARISON;
    }
    if ( adc1->isComplete() ) { // conversion succeded
        res.result_adc1 = adc1->readSingle();
    } else { // comparison was false
        adc1->fail_flag |= ADC_ERROR_COMPARISON;
    }
    __enable_irq();


    // if we interrupted a conversion, set it again
    if (wasADC0InUse) {
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc0->loadConfig(&old_adc0_config);
    }
    if (wasADC1InUse) {
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc1->loadConfig(&old_adc1_config);
    }

    //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );

    return res;
}

/*Returns the diff analog values of both sets of pins, measured at the same time by the two ADC modules.
* It waits until the value is read and then returns the result as a struct Sync_result,
* use Sync_result.result_adc0 and Sync_result.result_adc1.
* If a comparison has been set up and fails, it will return ADC_ERROR_VALUE in both fields of the struct.
*/
ADC::Sync_result ADC::analogSynchronizedReadDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) {

    Sync_result res = {ADC_ERROR_VALUE, ADC_ERROR_VALUE};;

    // check pins
    if(!adc0->checkDifferentialPins(pin0P, pin0N)) {
        adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
        return res;   // all others are invalid
    }
    if(!adc1->checkDifferentialPins(pin1P, pin1N)) {
        adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
        return res;   // all others are invalid
    }

    uint8_t resolution0 = adc0->getResolution();
    uint8_t resolution1 = adc1->getResolution();

    // check if we are interrupting a measurement, store setting if so.
    // vars to save the current state of the ADC in case it's in use
    ADC_Module::ADC_Config old_adc0_config = {0};
    uint8_t wasADC0InUse = adc0->isConverting(); // is the ADC running now?
    if(wasADC0InUse) { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc0->saveConfig(&old_adc0_config);
        __enable_irq();
    }
    ADC_Module::ADC_Config old_adc1_config = {0};
    uint8_t wasADC1InUse = adc1->isConverting(); // is the ADC running now?
    if(wasADC1InUse) { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc1->saveConfig(&old_adc1_config);
        __enable_irq();
    }

    // no continuous mode
    adc0->singleMode();
    adc1->singleMode();

    // start both measurements
    adc0->startDifferentialFast(pin0P, pin0N);
    adc1->startDifferentialFast(pin1P, pin1N);

    // wait for both ADCs to finish
    while( (adc0->isConverting()) || (adc1->isConverting()) ) {
        yield();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
    }
    __disable_irq(); // make sure nothing interrupts this part
    if (adc0->isComplete()) { // conversion succeded
        res.result_adc0 = adc0->readSingle();
        if(resolution0==16) { // 16 bit differential is actually 15 bit + 1 bit sign
            res.result_adc0 *= 2; // multiply by 2 as if it were really 16 bits, so that getMaxValue gives a correct value.
        }
    } else { // comparison was false
        adc0->fail_flag |= ADC_ERROR_COMPARISON;
    }
    if (adc1->isComplete()) { // conversion succeded
        res.result_adc1 = adc1->readSingle();
        if(resolution1==16) { // 16 bit differential is actually 15 bit + 1 bit sign
            res.result_adc1 *= 2; // multiply by 2 as if it were really 16 bits, so that getMaxValue gives a correct value.
        }
    } else { // comparison was false
        adc1->fail_flag |= ADC_ERROR_COMPARISON;
    }
    __enable_irq();


    // if we interrupted a conversion, set it again
    if (wasADC0InUse) {
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc0->loadConfig(&old_adc0_config);
    }
    if (wasADC1InUse) {
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc1->loadConfig(&old_adc1_config);
    }

    //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );

    return res;
}

/////////////// SYNCHRONIZED NON-BLOCKING METHODS //////////////

// Starts an analog measurement at the same time on the two ADC modules
/* It returns inmediately, get value with readSynchronizedSingle().
*   If the pin is incorrect it returns false
*   If this function interrupts a measurement, it stores the settings in adc_config
*/
bool ADC::startSynchronizedSingleRead(uint8_t pin0, uint8_t pin1) {

    // check pins
    if ( !adc0->checkPin(pin0) ) {
        adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
        return false;
    }
    if ( !adc1->checkPin(pin1) ) {
        adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
        return false;
    }

    // check if we are interrupting a measurement, store setting if so.
    adc0->adcWasInUse = adc0->isConverting(); // is the ADC running now?
    if(adc0->adcWasInUse) { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc0->saveConfig(&adc0->adc_config);
        __enable_irq();
    }
    adc1->adcWasInUse = adc1->isConverting(); // is the ADC running now?
    if(adc1->adcWasInUse) { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc1->saveConfig(&adc1->adc_config);
        __enable_irq();
    }

    // no continuous mode
    adc0->singleMode();
    adc1->singleMode();

    // start both measurements
    adc0->startReadFast(pin0);
    adc1->startReadFast(pin1);


    //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
    return true;

}

// Start a differential conversion between two pins (pin0P - pin0N) and (pin1P - pin1N)
/* It returns inmediately, get value with readSynchronizedSingle().
*   \param pinP must be A10 or A12.
*   \param pinN must be A11 (if pinP=A10) or A13 (if pinP=A12).
*   Other pins will return false.
*   If this function interrupts a measurement, it stores the settings in adc_config
*/
bool ADC::startSynchronizedSingleDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) {

    // check pins
    if(!adc0->checkDifferentialPins(pin0P, pin0N)) {
        adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
        return false;   // all others are invalid
    }
    if(!adc1->checkDifferentialPins(pin1P, pin1N)) {
        adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
        return false;   // all others are invalid
    }

    // check if we are interrupting a measurement, store setting if so.
    adc0->adcWasInUse = adc0->isConverting(); // is the ADC running now?
    if(adc0->adcWasInUse) { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc0->saveConfig(&adc0->adc_config);
        __enable_irq();
    }
    adc1->adcWasInUse = adc1->isConverting(); // is the ADC running now?
    if(adc1->adcWasInUse) { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        adc1->saveConfig(&adc1->adc_config);
        __enable_irq();
    }

    // no continuous mode
    adc0->singleMode();
    adc1->singleMode();

    // start both measurements
    adc0->startDifferentialFast(pin0P, pin0N);
    adc1->startDifferentialFast(pin1P, pin1N);

    //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );

    return true;
}

// Reads the analog value of a single conversion.
/*
*   \return the converted value.
*/
ADC::Sync_result ADC::readSynchronizedSingle() {
    ADC::Sync_result res;

    res.result_adc0 = adc0->readSingle();
    res.result_adc1 = adc1->readSingle();

    return res;
}


///////////// SYNCHRONIZED CONTINUOUS CONVERSION METHODS ////////////

//! Starts a continuous conversion in both ADCs simultaneously
/** Use readSynchronizedContinuous to get the values
*
*/
bool ADC::startSynchronizedContinuous(uint8_t pin0, uint8_t pin1) {

    // check pins
    if ( !adc0->checkPin(pin0) ) {
        adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
        return false;
    }
    if ( !adc1->checkPin(pin1) ) {
        adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
        return false;
    }

    adc0->startContinuous(pin0);
    adc1->startContinuous(pin1);

    // setup the conversions the usual way, but to make sure that they are
    // as synchronized as possible we stop and restart them one after the other.
    const uint32_t temp_ADC0_SC1A = ADC0_SC1A; ADC0_SC1A = 0x1F;
    const uint32_t temp_ADC1_SC1A = ADC1_SC1A; ADC1_SC1A = 0x1F;

    __disable_irq(); // both measurements should have a maximum delay of an instruction time
    ADC0_SC1A = temp_ADC0_SC1A;
    ADC1_SC1A = temp_ADC1_SC1A;
    __enable_irq();

    return true;
}

//! Starts a continuous differential conversion in both ADCs simultaneously
/** Use readSynchronizedContinuous to get the values
*
*/
bool ADC::startSynchronizedContinuousDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) {

    // check pins
    if(!adc0->checkDifferentialPins(pin0P, pin0N)) {
        adc0->fail_flag |= ADC_ERROR_WRONG_PIN;
        return false;   // all others are invalid
    }
    if(!adc1->checkDifferentialPins(pin1P, pin1N)) {
        adc1->fail_flag |= ADC_ERROR_WRONG_PIN;
        return false;   // all others are invalid
    }

    adc0->startContinuousDifferential(pin0P, pin0N);
    adc1->startContinuousDifferential(pin1P, pin1N);

    // setup the conversions the usual way, but to make sure that they are
    // as synchronized as possible we stop and restart them one after the other.
    const uint32_t temp_ADC0_SC1A = ADC0_SC1A; ADC0_SC1A = 0x1F;
    const uint32_t temp_ADC1_SC1A = ADC1_SC1A; ADC1_SC1A = 0x1F;

    __disable_irq();
    ADC0_SC1A = temp_ADC0_SC1A;
    ADC1_SC1A = temp_ADC1_SC1A;
    __enable_irq();


    return true;
}

//! Returns the values of both ADCs.
ADC::Sync_result ADC::readSynchronizedContinuous() {
    ADC::Sync_result res;

    res.result_adc0 = adc0->analogReadContinuous();
    res.result_adc1 = adc1->analogReadContinuous();

    return res;
}

//! Stops synchronous continuous conversion
void ADC::stopSynchronizedContinuous() {

    adc0->stopContinuous();
    adc1->stopContinuous();
}

#else // ADC_NUM_ADCS=1
// Empty definitions so code written for all Teensy will compile

ADC::Sync_result ADC::analogSynchronizedRead(uint8_t pin0, uint8_t pin1) {ADC::Sync_result res={0}; return res;}
ADC::Sync_result ADC::analogSynchronizedReadDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) {
        ADC::Sync_result res={0};
        return res;
}

bool ADC::startSynchronizedSingleRead(uint8_t pin0, uint8_t pin1) { return false; }
bool ADC::startSynchronizedSingleDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) { return false; }

ADC::Sync_result ADC::readSynchronizedSingle() {ADC::Sync_result res={0}; return res;}

bool ADC::startSynchronizedContinuous(uint8_t pin0, uint8_t pin1) {return false;}
bool ADC::startSynchronizedContinuousDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) {return false;}
ADC::Sync_result ADC::readSynchronizedContinuous() {ADC::Sync_result res={0}; return res;}
void ADC::stopSynchronizedContinuous() {}

#endif
