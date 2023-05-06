/* Teensy 4.x, 3.x, LC ADC library
 * https://github.com/pedvide/ADC
 * Copyright (c) 2020 Pedro Villanueva
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

/* ADC_Module.cpp: Implements the fuctions of a Teensy 3.x, LC ADC module
 *
 */

#include "ADC_Module.h"

// include the internal reference
#ifdef ADC_USE_INTERNAL_VREF
#include <VREF.h>
#endif

/* Constructor
*   Point the registers to the correct ADC module
*   Copy the correct channel2sc1a
*   Call init
*/
ADC_Module::ADC_Module(uint8_t ADC_number,
                       const uint8_t *const a_channel2sc1a,
#if ADC_DIFF_PAIRS > 0
                       const ADC_NLIST *const a_diff_table,
#endif
                       ADC_REGS_t &a_adc_regs) : ADC_num(ADC_number), channel2sc1a(a_channel2sc1a)
#if ADC_DIFF_PAIRS > 0
                                                 ,
                                                 diff_table(a_diff_table)
#endif
                                                 ,
                                                 adc_regs(a_adc_regs)
#ifdef ADC_USE_PDB
                                                 ,
                                                 PDB0_CHnC1(ADC_num ? PDB0_CH1C1 : PDB0_CH0C1)
#endif
#if defined(ADC_TEENSY_4)
                                                 ,
                                                 XBAR_IN(ADC_num ? XBARA1_IN_QTIMER4_TIMER3 : XBARA1_IN_QTIMER4_TIMER0), XBAR_OUT(ADC_num ? XBARA1_OUT_ADC_ETC_TRIG10 : XBARA1_OUT_ADC_ETC_TRIG00), QTIMER4_INDEX(ADC_num ? 3 : 0), ADC_ETC_TRIGGER_INDEX(ADC_num ? 4 : 0), IRQ_ADC(ADC_num ? IRQ_NUMBER_t::IRQ_ADC2 : IRQ_NUMBER_t::IRQ_ADC1)
#elif defined(ADC_DUAL_ADCS)
                                                 // IRQ_ADC0 and IRQ_ADC1 aren't consecutive in Teensy 3.6
                                                 // fix by SB, https://github.com/pedvide/ADC/issues/19
                                                 ,
                                                 IRQ_ADC(ADC_num ? IRQ_NUMBER_t::IRQ_ADC1 : IRQ_NUMBER_t::IRQ_ADC0)
#else
                                                 ,
                                                 IRQ_ADC(IRQ_NUMBER_t::IRQ_ADC0)
#endif
{

    // call our init
    analog_init();
}

/* Initialize stuff:
*  - Switch on clock
*  - Clear all fail flags
*  - Internal reference (default: external vcc)
*  - Mux between a and b channels (b channels)
*  - Calibrate with 32 averages and low speed
*  - When first calibration is done it sets:
*     - Resolution (default: 10 bits)
*     - Conversion speed and sampling time (both set to medium speed)
*     - Averaging (set to 4)
*/
void ADC_Module::analog_init()
{

    startClock();

    // default settings:
    /*
        - 10 bits resolution
        - 4 averages
        - vcc reference
        - no interrupts
        - pga gain=1
        - conversion speed = medium
        - sampling speed = medium
    initiate to 0 (or 1) so the corresponding functions change it to the correct value
    */
    analog_res_bits = 0;
    analog_max_val = 0;
    analog_num_average = 0;
    analog_reference_internal = ADC_REF_SOURCE::REF_NONE;
#ifdef ADC_USE_PGA
    pga_value = 1;
#endif
    interrupts_enabled = false;

#ifdef ADC_TEENSY_4
    // overwrite old values if a new conversion ends
    atomic::setBitFlag(adc_regs.CFG, ADC_CFG_OVWREN);
// this is the only option for Teensy 3.x and LC
#endif

    conversion_speed = ADC_CONVERSION_SPEED::HIGH_SPEED; // set to something different from line 139 so it gets changed there
    sampling_speed = ADC_SAMPLING_SPEED::VERY_HIGH_SPEED;

    calibrating = 0;

    fail_flag = ADC_ERROR::CLEAR; // clear all errors

    num_measurements = 0;

// select b channels
#ifdef ADC_TEENSY_4
// T4 has no a or b channels
#else
    // ADC_CFG2_muxsel = 1;
    atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_MUXSEL);
#endif

    // set reference to vcc
    setReference(ADC_REFERENCE::REF_3V3);

    // set resolution to 10
    setResolution(10);

    // the first calibration will use 32 averages and lowest speed,
    // when this calibration is over the averages and speed will be set to default by wait_for_cal and init_calib will be cleared.
    init_calib = 1;
    setAveraging(32);
    setConversionSpeed(ADC_CONVERSION_SPEED::LOW_SPEED);
    setSamplingSpeed(ADC_SAMPLING_SPEED::LOW_SPEED);

    // begin init calibration
    calibrate();
}

// starts calibration
void ADC_Module::calibrate()
{

    __disable_irq();

    calibrating = 1;
#ifdef ADC_TEENSY_4
    atomic::clearBitFlag(adc_regs.GC, ADC_GC_CAL);
    atomic::setBitFlag(adc_regs.GS, ADC_GS_CALF);
    atomic::setBitFlag(adc_regs.GC, ADC_GC_CAL);
#else
    // ADC_SC3_cal = 0; // stop possible previous calibration
    atomic::clearBitFlag(adc_regs.SC3, ADC_SC3_CAL);
    // ADC_SC3_calf = 1; // clear possible previous error
    atomic::setBitFlag(adc_regs.SC3, ADC_SC3_CALF);
    // ADC_SC3_cal = 1; // start calibration
    atomic::setBitFlag(adc_regs.SC3, ADC_SC3_CAL);
#endif

    __enable_irq();
}

/* Waits until calibration is finished and writes the corresponding registers
*
*/
void ADC_Module::wait_for_cal(void)
{

// wait for calibration to finish
#ifdef ADC_TEENSY_4
    while (atomic::getBitFlag(adc_regs.GC, ADC_GC_CAL))
    { // Bit ADC_GC_CAL in register GC cleared when calib. finishes.
        yield();
    }
    if (atomic::getBitFlag(adc_regs.GS, ADC_GS_CALF))
    {                                  // calibration failed
        fail_flag |= ADC_ERROR::CALIB; // the user should know and recalibrate manually
    }
#else
    while (atomic::getBitFlag(adc_regs.SC3, ADC_SC3_CAL))
    { // Bit ADC_SC3_CAL in register ADC0_SC3 cleared when calib. finishes.
        yield();
    }
    if (atomic::getBitFlag(adc_regs.SC3, ADC_SC3_CALF))
    {                                  // calibration failed
        fail_flag |= ADC_ERROR::CALIB; // the user should know and recalibrate manually
    }
#endif

// set calibrated values to registers
#ifdef ADC_TEENSY_4
// T4 does not require anything else for calibration
#else
    __disable_irq();
    uint16_t sum;
    if (calibrating)
    {
        sum = adc_regs.CLPS + adc_regs.CLP4 + adc_regs.CLP3 + adc_regs.CLP2 + adc_regs.CLP1 + adc_regs.CLP0;
        sum = (sum / 2) | 0x8000;
        adc_regs.PG = sum;

        sum = adc_regs.CLMS + adc_regs.CLM4 + adc_regs.CLM3 + adc_regs.CLM2 + adc_regs.CLM1 + adc_regs.CLM0;
        sum = (sum / 2) | 0x8000;
        adc_regs.MG = sum;
    }
    __enable_irq();
#endif
    calibrating = 0;

    // the first calibration uses 32 averages and lowest speed,
    // when this calibration is over, set the averages and speed to default.
    if (init_calib)
    {

        // set conversion speed to medium
        setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED);

        // set sampling speed to medium
        setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);

        // number of averages to 4
        setAveraging(4);

        init_calib = 0; // clear
    }
}

//! Starts the calibration sequence, waits until it's done and writes the results
/** Usually it's not necessary to call this function directly, but do it if the "environment" changed
*   significantly since the program was started.
*/
void ADC_Module::recalibrate()
{

    calibrate();

    wait_for_cal();
}

/////////////// METHODS TO SET/GET SETTINGS OF THE ADC ////////////////////

/* Set the voltage reference you prefer, default is 3.3V
*   It needs to recalibrate
*  Use ADC_REF_3V3, ADC_REF_1V2 (not for Teensy LC) or ADC_REF_EXT
*/
void ADC_Module::setReference(ADC_REFERENCE type)
{
    ADC_REF_SOURCE ref_type = static_cast<ADC_REF_SOURCE>(type); // cast to source type, that is, either internal or default

    if (analog_reference_internal == ref_type)
    { // don't need to change anything
        return;
    }

    if (ref_type == ADC_REF_SOURCE::REF_ALT)
    { // 1.2V ref for Teensy 3.x, 3.3 VDD for Teensy LC
// internal reference requested
#ifdef ADC_USE_INTERNAL_VREF
        VREF::start(); // enable VREF if Teensy 3.x
#endif

        analog_reference_internal = ADC_REF_SOURCE::REF_ALT;

#ifdef ADC_TEENSY_4
// No REF_ALT for T4
#else
        // *ADC_SC2_ref = 1; // uses bitband: atomic
        atomic::setBitFlag(adc_regs.SC2, ADC_SC2_REFSEL(1));
#endif
    }
    else if (ref_type == ADC_REF_SOURCE::REF_DEFAULT)
    {   // ext ref for all Teensys, vcc also for Teensy 3.x
        // vcc or external reference requested

#ifdef ADC_USE_INTERNAL_VREF
        VREF::stop(); // disable 1.2V reference source when using the external ref (p. 102, 3.7.1.7)
#endif

        analog_reference_internal = ADC_REF_SOURCE::REF_DEFAULT;

#ifdef ADC_TEENSY_4
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_REFSEL(3));
#else
        // *ADC_SC2_ref = 0; // uses bitband: atomic
        atomic::clearBitFlag(adc_regs.SC2, ADC_SC2_REFSEL(1));
#endif
    }

    calibrate();
}

/* Change the resolution of the measurement
*  For single-ended measurements: 8, 10, 12 or 16 bits.
*  For differential measurements: 9, 11, 13 or 16 bits.
*  If you want something in between (11 bits single-ended for example) select the inmediate higher
*  and shift the result one to the right.
*
*  It doesn't recalibrate
*/
void ADC_Module::setResolution(uint8_t bits)
{

    if (analog_res_bits == bits)
    {
        return;
    }

    uint8_t config;

    if (calibrating)
        wait_for_cal();

    if (bits <= 9)
    {
        config = 8;
    }
    else if (bits <= 11)
    {
        config = 10;
#ifdef ADC_TEENSY_4
    }
    else if (bits > 11)
    {
        config = 12;
#else
    }
    else if (bits <= 13)
    {
        config = 12;
    }
    else if (bits > 13)
    {
        config = 16;
#endif
    }
    else
    {
        config = 8; // default to 8 bits
    }

    // conversion resolution
    // single-ended 8 bits is the same as differential 9 bits, etc.
    if ((config == 8) || (config == 9))
    {
#ifdef ADC_TEENSY_4
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_MODE(3));
#else
        // *ADC_CFG1_mode1 = 0;
        // *ADC_CFG1_mode0 = 0;
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_MODE(3));
#endif
        analog_max_val = 255; // diff mode 9 bits has 1 bit for sign, so max value is the same as single 8 bits
    }
    else if ((config == 10) || (config == 11))
    {
#ifdef ADC_TEENSY_4
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_MODE(3), ADC_CFG_MODE(1));
#else
        // *ADC_CFG1_mode1 = 1;
        // *ADC_CFG1_mode0 = 0;
        atomic::changeBitFlag(adc_regs.CFG1, ADC_CFG1_MODE(3), ADC_CFG1_MODE(2));
#endif
        analog_max_val = 1023;
    }
    else if ((config == 12) || (config == 13))
    {
#ifdef ADC_TEENSY_4
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_MODE(3), ADC_CFG_MODE(2));
#else
        // *ADC_CFG1_mode1 = 0;
        // *ADC_CFG1_mode0 = 1;
        atomic::changeBitFlag(adc_regs.CFG1, ADC_CFG1_MODE(3), ADC_CFG1_MODE(1));
#endif
        analog_max_val = 4095;
    }
    else
    {
#ifdef ADC_TEENSY_4
// Impossible for T4
#else
        // *ADC_CFG1_mode1 = 1;
        // *ADC_CFG1_mode0 = 1;
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_MODE(3));
#endif
        analog_max_val = 65535;
    }

    analog_res_bits = config;

    // no recalibration is needed when changing the resolution, p. 619
}

/* Returns the resolution of the ADC
*
*/
uint8_t ADC_Module::getResolution()
{
    return analog_res_bits;
}

/* Returns the maximum value for a measurement, that is: 2^resolution-1
*
*/
uint32_t ADC_Module::getMaxValue()
{
    return analog_max_val;
}

// Sets the conversion speed
/* Increase the sampling speed for low impedance sources, decrease it for higher impedance ones.
* \param speed can be any of the ADC_SAMPLING_SPEED enum: VERY_LOW_SPEED, LOW_SPEED, MED_SPEED, HIGH_SPEED or VERY_HIGH_SPEED.
*
* VERY_LOW_SPEED is the lowest possible sampling speed (+24 ADCK).
* LOW_SPEED adds +16 ADCK.
* MED_SPEED adds +10 ADCK.
* HIGH_SPEED adds +6 ADCK.
* VERY_HIGH_SPEED is the highest possible sampling speed (0 ADCK added).
*/
void ADC_Module::setConversionSpeed(ADC_CONVERSION_SPEED speed)
{

    if (speed == conversion_speed)
    { // no change
        return;
    }

    //if (calibrating) wait_for_cal();

    bool is_adack = false;
    uint32_t ADC_CFG1_speed = 0; // store the clock and divisor (set to 0 to avoid warnings)

    switch (speed)
    {
// normal bus clock
#ifndef ADC_TEENSY_4
    case ADC_CONVERSION_SPEED::VERY_LOW_SPEED:
        atomic::clearBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
        // ADC_CFG1_speed = ADC_CFG1_VERY_LOW_SPEED;
        ADC_CFG1_speed = get_CFG_VERY_LOW_SPEED(ADC_F_BUS);
        break;
#endif
    case ADC_CONVERSION_SPEED::LOW_SPEED:
#ifdef ADC_TEENSY_4
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADHSC);
        atomic::setBitFlag(adc_regs.CFG, ADC_CFG_ADLPC);
#else
        atomic::clearBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
#endif
        // ADC_CFG1_speed = ADC_CFG1_LOW_SPEED;
        ADC_CFG1_speed = get_CFG_LOW_SPEED(ADC_F_BUS);
        break;
    case ADC_CONVERSION_SPEED::MED_SPEED:
#ifdef ADC_TEENSY_4
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADHSC);
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADLPC);
#else
        atomic::clearBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
#endif
        ADC_CFG1_speed = get_CFG_MEDIUM_SPEED(ADC_F_BUS);
        break;
#ifndef ADC_TEENSY_4
    case ADC_CONVERSION_SPEED::HIGH_SPEED_16BITS:
        atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
        // ADC_CFG1_speed = ADC_CFG1_HI_SPEED_16_BITS;
        ADC_CFG1_speed = get_CFG_HI_SPEED_16_BITS(ADC_F_BUS);
        break;
#endif
    case ADC_CONVERSION_SPEED::HIGH_SPEED:
#ifdef ADC_TEENSY_4
        atomic::setBitFlag(adc_regs.CFG, ADC_CFG_ADHSC);
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADLPC);
#else
        atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
#endif
        ADC_CFG1_speed = get_CFG_HIGH_SPEED(ADC_F_BUS);
        break;
#ifndef ADC_TEENSY_4
    case ADC_CONVERSION_SPEED::VERY_HIGH_SPEED:
        atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
        // ADC_CFG1_speed = ADC_CFG1_VERY_HIGH_SPEED;
        ADC_CFG1_speed = get_CFG_VERY_HIGH_SPEED(ADC_F_BUS);
        break;
#endif

// adack - async clock source, independent of the bus clock
#ifdef ADC_TEENSY_4 // fADK = 10 or 20 MHz
    case ADC_CONVERSION_SPEED::ADACK_10:
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADHSC);
        is_adack = true;
        break;
    case ADC_CONVERSION_SPEED::ADACK_20:
        atomic::setBitFlag(adc_regs.CFG, ADC_CFG_ADHSC);
        is_adack = true;
        break;
#else // fADK = 2.4, 4.0, 5.2 or 6.2 MHz
    case ADC_CONVERSION_SPEED::ADACK_2_4:
        atomic::clearBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
        is_adack = true;
        break;
    case ADC_CONVERSION_SPEED::ADACK_4_0:
        atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
        is_adack = true;
        break;
    case ADC_CONVERSION_SPEED::ADACK_5_2:
        atomic::clearBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
        is_adack = true;
        break;
    case ADC_CONVERSION_SPEED::ADACK_6_2:
        atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_ADHSC);
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_ADLPC);
        is_adack = true;
        break;
#endif

    default:
        fail_flag |= ADC_ERROR::OTHER;
        return;
    }

    if (is_adack)
    {
// async clock source, independent of the bus clock
#ifdef ADC_TEENSY_4
        atomic::setBitFlag(adc_regs.GC, ADC_GC_ADACKEN);     // enable ADACK (takes max 5us to be ready)
        atomic::setBitFlag(adc_regs.CFG, ADC_CFG_ADICLK(3)); // select ADACK as clock source
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADIV(3)); // select no dividers
#else
        atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_ADACKEN);
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADICLK(3));
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_ADIV(3));
#endif
    }
    else
    {
// normal bus clock used - disable the internal asynchronous clock
// total speed can be: bus, bus/2, bus/4, bus/8 or bus/16.
#ifdef ADC_TEENSY_4
        atomic::clearBitFlag(adc_regs.GC, ADC_GC_ADACKEN);                                          // disable async
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADICLK(3), ADC_CFG1_speed & ADC_CFG_ADICLK(3)); // bus or bus/2
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADIV(3), ADC_CFG1_speed & ADC_CFG_ADIV(3));     // divisor for the clock source
#else
        atomic::clearBitFlag(adc_regs.CFG2, ADC_CFG2_ADACKEN);
        atomic::changeBitFlag(adc_regs.CFG1, ADC_CFG1_ADICLK(3), ADC_CFG1_speed & ADC_CFG1_ADICLK(3));
        atomic::changeBitFlag(adc_regs.CFG1, ADC_CFG1_ADIV(3), ADC_CFG1_speed & ADC_CFG1_ADIV(3));
#endif
    }

    conversion_speed = speed;
    calibrate();
}

// Sets the sampling speed
/* Increase the sampling speed for low impedance sources, decrease it for higher impedance ones.
* \param speed can be any of the ADC_SAMPLING_SPEED enum: VERY_LOW_SPEED, LOW_SPEED, MED_SPEED, HIGH_SPEED or VERY_HIGH_SPEED.
*
* VERY_LOW_SPEED is the lowest possible sampling speed (+24 ADCK).
* LOW_SPEED adds +16 ADCK.
* MED_SPEED adds +10 ADCK.
* HIGH_SPEED adds +6 ADCK.
* VERY_HIGH_SPEED is the highest possible sampling speed (0 ADCK added).
*/
void ADC_Module::setSamplingSpeed(ADC_SAMPLING_SPEED speed)
{
    if (calibrating)
        wait_for_cal();

    switch (speed)
    {
#ifdef ADC_TEENSY_4
    case ADC_SAMPLING_SPEED::VERY_LOW_SPEED:
        atomic::setBitFlag(adc_regs.CFG, ADC_CFG_ADLSMP); // long sampling time enable
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADSTS(3), ADC_CFG_ADSTS(3));
        break;
    case ADC_SAMPLING_SPEED::LOW_SPEED:
        atomic::setBitFlag(adc_regs.CFG, ADC_CFG_ADLSMP); // long sampling time enable
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADSTS(3), ADC_CFG_ADSTS(2));
        break;
    case ADC_SAMPLING_SPEED::LOW_MED_SPEED:
        atomic::setBitFlag(adc_regs.CFG, ADC_CFG_ADLSMP); // long sampling time enable
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADSTS(3), ADC_CFG_ADSTS(1));
        break;
    case ADC_SAMPLING_SPEED::MED_SPEED:
        atomic::setBitFlag(adc_regs.CFG, ADC_CFG_ADLSMP); // long sampling time enable
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADSTS(3), ADC_CFG_ADSTS(0));
        break;
    case ADC_SAMPLING_SPEED::MED_HIGH_SPEED:
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADLSMP); // long sampling time disabled
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADSTS(3), ADC_CFG_ADSTS(3));
        break;
    case ADC_SAMPLING_SPEED::HIGH_SPEED:
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADLSMP); // long sampling time disabled
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADSTS(3), ADC_CFG_ADSTS(2));
        break;
    case ADC_SAMPLING_SPEED::HIGH_VERY_HIGH_SPEED:
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADLSMP); // long sampling time disabled
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADSTS(3), ADC_CFG_ADSTS(1));
        break;
    case ADC_SAMPLING_SPEED::VERY_HIGH_SPEED:
        atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_ADLSMP); // long sampling time disabled
        atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_ADSTS(3), ADC_CFG_ADSTS(0));
        break;
#else
    case ADC_SAMPLING_SPEED::VERY_LOW_SPEED:
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADLSMP);      // long sampling time enable
        atomic::clearBitFlag(adc_regs.CFG2, ADC_CFG2_ADLSTS(3)); // maximum sampling time (+24 ADCK)
        break;
    case ADC_SAMPLING_SPEED::LOW_SPEED:
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADLSMP);                           // long sampling time enable
        atomic::changeBitFlag(adc_regs.CFG2, ADC_CFG2_ADLSTS(3), ADC_CFG2_ADLSTS(1)); // high sampling time (+16 ADCK)
        break;
    case ADC_SAMPLING_SPEED::MED_SPEED:
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADLSMP);                           // long sampling time enable
        atomic::changeBitFlag(adc_regs.CFG2, ADC_CFG2_ADLSTS(3), ADC_CFG2_ADLSTS(2)); // medium sampling time (+10 ADCK)
        break;
    case ADC_SAMPLING_SPEED::HIGH_SPEED:
        atomic::setBitFlag(adc_regs.CFG1, ADC_CFG1_ADLSMP);    // long sampling time enable
        atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_ADLSTS(3)); // low sampling time (+6 ADCK)
        break;
    case ADC_SAMPLING_SPEED::VERY_HIGH_SPEED:
        atomic::clearBitFlag(adc_regs.CFG1, ADC_CFG1_ADLSMP); // shortest sampling time
        break;
#endif
    }
    sampling_speed = speed;
}

/* Set the number of averages: 0, 4, 8, 16 or 32.
*
*/
void ADC_Module::setAveraging(uint8_t num)
{

    if (calibrating)
        wait_for_cal();

    if (num <= 1)
    {
        num = 0;
// ADC_SC3_avge = 0;
#ifdef ADC_TEENSY_4
        atomic::clearBitFlag(adc_regs.GC, ADC_GC_AVGE);
#else
        atomic::clearBitFlag(adc_regs.SC3, ADC_SC3_AVGE);
#endif
    }
    else
    {
// ADC_SC3_avge = 1;
#ifdef ADC_TEENSY_4
        atomic::setBitFlag(adc_regs.GC, ADC_GC_AVGE);
#else
        atomic::setBitFlag(adc_regs.SC3, ADC_SC3_AVGE);
#endif
        if (num <= 4)
        {
            num = 4;
// ADC_SC3_avgs0 = 0;
// ADC_SC3_avgs1 = 0;
#ifdef ADC_TEENSY_4
            atomic::clearBitFlag(adc_regs.CFG, ADC_CFG_AVGS(3));
#else
            atomic::clearBitFlag(adc_regs.SC3, ADC_SC3_AVGS(3));
#endif
        }
        else if (num <= 8)
        {
            num = 8;
// ADC_SC3_avgs0 = 1;
// ADC_SC3_avgs1 = 0;
#ifdef ADC_TEENSY_4
            atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_AVGS(3), ADC_CFG_AVGS(1));
#else
            atomic::changeBitFlag(adc_regs.SC3, ADC_SC3_AVGS(3), ADC_SC3_AVGS(1));
#endif
        }
        else if (num <= 16)
        {
            num = 16;
// ADC_SC3_avgs0 = 0;
// ADC_SC3_avgs1 = 1;
#ifdef ADC_TEENSY_4
            atomic::changeBitFlag(adc_regs.CFG, ADC_CFG_AVGS(3), ADC_CFG_AVGS(2));
#else
            atomic::changeBitFlag(adc_regs.SC3, ADC_SC3_AVGS(3), ADC_SC3_AVGS(2));
#endif
        }
        else
        {
            num = 32;
// ADC_SC3_avgs0 = 1;
// ADC_SC3_avgs1 = 1;
#ifdef ADC_TEENSY_4
            atomic::setBitFlag(adc_regs.CFG, ADC_CFG_AVGS(3));
#else
            atomic::setBitFlag(adc_regs.SC3, ADC_SC3_AVGS(3));
#endif
        }
    }
    analog_num_average = num;
}

/* Enable interrupts: An ADC Interrupt will be raised when the conversion is completed
*  (including hardware averages and if the comparison (if any) is true).
*/
void ADC_Module::enableInterrupts(void (*isr)(void), uint8_t priority)
{
    if (calibrating)
        wait_for_cal();

// ADC_SC1A_aien = 1;
#ifdef ADC_TEENSY_4
    atomic::setBitFlag(adc_regs.HC0, ADC_HC_AIEN);
    interrupts_enabled = true;
#else
    atomic::setBitFlag(adc_regs.SC1A, ADC_SC1_AIEN);
#endif

    attachInterruptVector(IRQ_ADC, isr);
    NVIC_SET_PRIORITY(IRQ_ADC, priority);
    NVIC_ENABLE_IRQ(IRQ_ADC);
}

/* Disable interrupts
*
*/
void ADC_Module::disableInterrupts()
{
// ADC_SC1A_aien = 0;
#ifdef ADC_TEENSY_4
    atomic::clearBitFlag(adc_regs.HC0, ADC_HC_AIEN);
    interrupts_enabled = false;
#else
    atomic::clearBitFlag(adc_regs.SC1A, ADC_SC1_AIEN);
#endif

    NVIC_DISABLE_IRQ(IRQ_ADC);
}

#ifdef ADC_USE_DMA
/* Enable DMA request: An ADC DMA request will be raised when the conversion is completed
*  (including hardware averages and if the comparison (if any) is true).
*/
void ADC_Module::enableDMA()
{

    if (calibrating)
        wait_for_cal();

// ADC_SC2_dma = 1;
#ifdef ADC_TEENSY_4
    atomic::setBitFlag(adc_regs.GC, ADC_GC_DMAEN);
#else
    atomic::setBitFlag(adc_regs.SC2, ADC_SC2_DMAEN);
#endif
}

/* Disable ADC DMA request
*
*/
void ADC_Module::disableDMA()
{

// ADC_SC2_dma = 0;
#ifdef ADC_TEENSY_4
    atomic::clearBitFlag(adc_regs.GC, ADC_GC_DMAEN);
#else
    atomic::clearBitFlag(adc_regs.SC2, ADC_SC2_DMAEN);
#endif
}
#endif

/* Enable the compare function: A conversion will be completed only when the ADC value
*  is >= compValue (greaterThan=1) or < compValue (greaterThan=0)
*  Call it after changing the resolution
*  Use with interrupts or poll conversion completion with isADC_Complete()
*/
void ADC_Module::enableCompare(int16_t compValue, bool greaterThan)
{

    if (calibrating)
        wait_for_cal(); // if we modify the adc's registers when calibrating, it will fail

// ADC_SC2_cfe = 1; // enable compare
// ADC_SC2_cfgt = (int32_t)greaterThan; // greater or less than?
#ifdef ADC_TEENSY_4
    atomic::setBitFlag(adc_regs.GC, ADC_GC_ACFE);
    atomic::changeBitFlag(adc_regs.GC, ADC_GC_ACFGT, ADC_GC_ACFGT * greaterThan);
    adc_regs.CV = ADC_CV_CV1(compValue);
#else
    atomic::setBitFlag(adc_regs.SC2, ADC_SC2_ACFE);
    atomic::changeBitFlag(adc_regs.SC2, ADC_SC2_ACFGT, ADC_SC2_ACFGT * greaterThan);

    adc_regs.CV1 = (int16_t)compValue; // comp value
#endif
}

/* Enable the compare function: A conversion will be completed only when the ADC value
*  is inside (insideRange=1) or outside (=0) the range given by (lowerLimit, upperLimit),
*  including (inclusive=1) the limits or not (inclusive=0).
*  See Table 31-78, p. 617 of the freescale manual.
*  Call it after changing the resolution
*/
void ADC_Module::enableCompareRange(int16_t lowerLimit, int16_t upperLimit, bool insideRange, bool inclusive)
{

    if (calibrating)
        wait_for_cal(); // if we modify the adc's registers when calibrating, it will fail

// ADC_SC2_cfe = 1; // enable compare
// ADC_SC2_cren = 1; // enable compare range
#ifdef ADC_TEENSY_4
    atomic::setBitFlag(adc_regs.GC, ADC_GC_ACFE);
    atomic::setBitFlag(adc_regs.GC, ADC_GC_ACREN);
#else
    atomic::setBitFlag(adc_regs.SC2, ADC_SC2_ACFE);
    atomic::setBitFlag(adc_regs.SC2, ADC_SC2_ACREN);
#endif

    if (insideRange && inclusive)
    { // True if value is inside the range, including the limits. CV1 <= CV2 and ACFGT=1
// ADC_SC2_cfgt = 1;
#ifdef ADC_TEENSY_4
        atomic::setBitFlag(adc_regs.GC, ADC_GC_ACFGT);
        adc_regs.CV = ADC_CV_CV1(lowerLimit) | ADC_CV_CV2(upperLimit);
#else
        atomic::setBitFlag(adc_regs.SC2, ADC_SC2_ACFGT);
        adc_regs.CV1 = (int16_t)lowerLimit;
        adc_regs.CV2 = (int16_t)upperLimit;
#endif
    }
    else if (insideRange && !inclusive)
    { // True if value is inside the range, excluding the limits. CV1 > CV2 and ACFGT=0
// ADC_SC2_cfgt = 0;
#ifdef ADC_TEENSY_4
        atomic::clearBitFlag(adc_regs.GC, ADC_GC_ACFGT);
        adc_regs.CV = ADC_CV_CV2(lowerLimit) | ADC_CV_CV1(upperLimit);
#else
        atomic::clearBitFlag(adc_regs.SC2, ADC_SC2_ACFGT);
        adc_regs.CV2 = (int16_t)lowerLimit;
        adc_regs.CV1 = (int16_t)upperLimit;
#endif
    }
    else if (!insideRange && inclusive)
    { // True if value is outside of range or is equal to either limit. CV1 > CV2 and ACFGT=1
// ADC_SC2_cfgt = 1;
#ifdef ADC_TEENSY_4
        atomic::setBitFlag(adc_regs.GC, ADC_GC_ACFGT);
        adc_regs.CV = ADC_CV_CV2(lowerLimit) | ADC_CV_CV1(upperLimit);
#else
        atomic::setBitFlag(adc_regs.SC2, ADC_SC2_ACFGT);
        adc_regs.CV2 = (int16_t)lowerLimit;
        adc_regs.CV1 = (int16_t)upperLimit;
#endif
    }
    else if (!insideRange && !inclusive)
    { // True if value is outside of range and not equal to either limit. CV1 > CV2 and ACFGT=0
// ADC_SC2_cfgt = 0;
#ifdef ADC_TEENSY_4
        atomic::clearBitFlag(adc_regs.GC, ADC_GC_ACFGT);
        adc_regs.CV = ADC_CV_CV1(lowerLimit) | ADC_CV_CV2(upperLimit);
#else
        atomic::clearBitFlag(adc_regs.SC2, ADC_SC2_ACFGT);
        adc_regs.CV1 = (int16_t)lowerLimit;
        adc_regs.CV2 = (int16_t)upperLimit;
#endif
    }
}

/* Disable the compare function
*
*/
void ADC_Module::disableCompare()
{

// ADC_SC2_cfe = 0;
#ifdef ADC_TEENSY_4
    atomic::clearBitFlag(adc_regs.GC, ADC_GC_ACFE);
#else
    atomic::clearBitFlag(adc_regs.SC2, ADC_SC2_ACFE);
#endif
}

#ifdef ADC_USE_PGA
/* Enables the PGA and sets the gain
*   Use only for signals lower than 1.2 V
*   \param gain can be 1, 2, 4, 8, 16 32 or 64
*
*/
void ADC_Module::enablePGA(uint8_t gain)
{
    if (calibrating)
        wait_for_cal();

    uint8_t setting;
    if (gain <= 1)
    {
        setting = 0;
    }
    else if (gain <= 2)
    {
        setting = 1;
    }
    else if (gain <= 4)
    {
        setting = 2;
    }
    else if (gain <= 8)
    {
        setting = 3;
    }
    else if (gain <= 16)
    {
        setting = 4;
    }
    else if (gain <= 32)
    {
        setting = 5;
    }
    else
    { // 64
        setting = 6;
    }

    adc_regs.PGA = ADC_PGA_PGAEN | ADC_PGA_PGAG(setting);
    pga_value = 1 << setting;
}

/* Returns the PGA level
*  PGA level = from 0 to 64
*/
uint8_t ADC_Module::getPGA()
{
    return pga_value;
}

//! Disable PGA
void ADC_Module::disablePGA()
{
    // ADC_PGA_pgaen = 0;
    atomic::clearBitFlag(adc_regs.PGA, ADC_PGA_PGAEN);
    pga_value = 1;
}
#endif

//////////////// INFORMATION ABOUT VALID PINS //////////////////

// check whether the pin is a valid analog pin
bool ADC_Module::checkPin(uint8_t pin)
{

    if (pin > ADC_MAX_PIN)
    {
        return false; // all others are invalid
    }

    // translate pin number to SC1A number, that also contains MUX a or b info.
    const uint8_t sc1a_pin = channel2sc1a[pin];

    // check for valid pin
    if ((sc1a_pin & ADC_SC1A_CHANNELS) == ADC_SC1A_PIN_INVALID)
    {
        return false; // all others are invalid
    }

    return true;
}

#if ADC_DIFF_PAIRS > 0
// check whether the pins are a valid analog differential pins (including PGA if enabled)
bool ADC_Module::checkDifferentialPins(uint8_t pinP, uint8_t pinN)
{
    if (pinP > ADC_MAX_PIN)
    {
        return false; // all others are invalid
    }

    // translate pinP number to SC1A number, to make sure it's differential
    uint8_t sc1a_pin = channel2sc1a[pinP];

    if (!(sc1a_pin & ADC_SC1A_PIN_DIFF))
    {
        return false; // all others are invalid
    }

    // get SC1A number, also whether it can do PGA
    sc1a_pin = getDifferentialPair(pinP);

    // the pair can't be measured with this ADC
    if ((sc1a_pin & ADC_SC1A_CHANNELS) == ADC_SC1A_PIN_INVALID)
    {
        return false; // all others are invalid
    }

#ifdef ADC_USE_PGA
    // check if PGA is enabled, and whether the pin has access to it in this ADC module
    if (isPGAEnabled() && !(sc1a_pin & ADC_SC1A_PIN_PGA))
    {
        return false;
    }
#endif // ADC_USE_PGA

    return true;
}
#endif

//////////////// HELPER METHODS FOR CONVERSION /////////////////

// Starts a single-ended conversion on the pin (sets the mux correctly)
// Doesn't do any of the checks on the pin
// It doesn't change the continuous conversion bit
void ADC_Module::startReadFast(uint8_t pin)
{

    // translate pin number to SC1A number, that also contains MUX a or b info.
    const uint8_t sc1a_pin = channel2sc1a[pin];

#ifdef ADC_TEENSY_4
// Teensy 4 has no a or b channels
#else
    if (sc1a_pin & ADC_SC1A_PIN_MUX)
    { // mux a
        atomic::clearBitFlag(adc_regs.CFG2, ADC_CFG2_MUXSEL);
    }
    else
    { // mux b
        atomic::setBitFlag(adc_regs.CFG2, ADC_CFG2_MUXSEL);
    }
#endif

    // select pin for single-ended mode and start conversion, enable interrupts if requested
    __disable_irq();
#ifdef ADC_TEENSY_4
    adc_regs.HC0 = (sc1a_pin & ADC_SC1A_CHANNELS) + interrupts_enabled * ADC_HC_AIEN;
#else
    adc_regs.SC1A = (sc1a_pin & ADC_SC1A_CHANNELS) + atomic::getBitFlag(adc_regs.SC1A, ADC_SC1_AIEN) * ADC_SC1_AIEN;
#endif
    __enable_irq();
}

#if ADC_DIFF_PAIRS > 0
// Starts a differential conversion on the pair of pins
// Doesn't do any of the checks on the pins
// It doesn't change the continuous conversion bit
void ADC_Module::startDifferentialFast(uint8_t pinP, uint8_t pinN)
{

    // get SC1A number
    uint8_t sc1a_pin = getDifferentialPair(pinP);

#ifdef ADC_USE_PGA
    // check if PGA is enabled
    if (isPGAEnabled())
    {
        sc1a_pin = 0x2; // PGA always uses DAD2
    }
#endif // ADC_USE_PGA

    __disable_irq();
    adc_regs.SC1A = ADC_SC1_DIFF + (sc1a_pin & ADC_SC1A_CHANNELS) + atomic::getBitFlag(adc_regs.SC1A, ADC_SC1_AIEN) * ADC_SC1_AIEN;
    __enable_irq();
}
#endif

//////////////// BLOCKING CONVERSION METHODS //////////////////
/*
    This methods are implemented like this:

    1. Check that the pin is correct
    2. if calibrating, wait for it to finish before modifiying any ADC register
    3. Check if we're interrupting a measurement, if so store the settings.
    4. Disable continuous conversion mode and start the current measurement
    5. Wait until it's done, and check whether the comparison (if any) was succesful.
    6. Get the result.
    7. If step 3. is true, restore the previous ADC settings

*/

/* Reads the analog value of the pin.
* It waits until the value is read and then returns the result.
* If a comparison has been set up and fails, it will return ADC_ERROR_VALUE.
* Set the resolution, number of averages and voltage reference using the appropriate functions.
*/
int ADC_Module::analogRead(uint8_t pin)
{

    //digitalWriteFast(LED_BUILTIN, HIGH);

    // check whether the pin is correct
    if (!checkPin(pin))
    {
        fail_flag |= ADC_ERROR::WRONG_PIN;
        return ADC_ERROR_VALUE;
    }

    // increase the counter of measurements
    num_measurements++;

    //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));

    if (calibrating)
        wait_for_cal();

    //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));

    // check if we are interrupting a measurement, store setting if so.
    // vars to save the current state of the ADC in case it's in use
    ADC_Config old_config = {};
    const uint8_t wasADCInUse = isConverting(); // is the ADC running now?

    if (wasADCInUse)
    { // this means we're interrupting a conversion
        // save the current conversion config, we don't want any other interrupts messing up the configs
        __disable_irq();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        saveConfig(&old_config);
        __enable_irq();
    }

    // no continuous mode
    singleMode();

    startReadFast(pin); // start single read

    // wait for the ADC to finish
    while (isConverting())
    {
        yield();
    }

    // it's done, check if the comparison (if any) was true
    int32_t result;
    __disable_irq(); // make sure nothing interrupts this part
    if (isComplete())
    { // conversion succeded
        result = (uint16_t)readSingle();
    }
    else
    { // comparison was false
        fail_flag |= ADC_ERROR::COMPARISON;
        result = ADC_ERROR_VALUE;
    }
    __enable_irq();

    // if we interrupted a conversion, set it again
    if (wasADCInUse)
    {
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
        __disable_irq();
        loadConfig(&old_config);
        __enable_irq();
    }

    num_measurements--;
    return result;

} // analogRead

#if ADC_DIFF_PAIRS > 0
/* Reads the differential analog value of two pins (pinP - pinN)
* It waits until the value is read and then returns the result
* If a comparison has been set up and fails, it will return ADC_ERROR_DIFF_VALUE
* Set the resolution, number of averages and voltage reference using the appropriate functions
*/
int ADC_Module::analogReadDifferential(uint8_t pinP, uint8_t pinN)
{

    if (!checkDifferentialPins(pinP, pinN))
    {
        fail_flag |= ADC_ERROR::WRONG_PIN;
        return ADC_ERROR_VALUE; // all others are invalid
    }

    // increase the counter of measurements
    num_measurements++;

    // check for calibration before setting channels,
    // because conversion will start as soon as we write to adc_regs.SC1A
    if (calibrating)
        wait_for_cal();

    uint8_t res = getResolution();

    // vars to saved the current state of the ADC in case it's in use
    ADC_Config old_config = {};
    uint8_t wasADCInUse = isConverting(); // is the ADC running now?

    if (wasADCInUse)
    { // this means we're interrupting a conversion
        // save the current conversion config, we don't want any other interrupts messing up the configs
        __disable_irq();
        saveConfig(&old_config);
        __enable_irq();
    }

    // no continuous mode
    singleMode();

    startDifferentialFast(pinP, pinN); // start conversion

    // wait for the ADC to finish
    while (isConverting())
    {
        yield();
        //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN) );
    }

    // it's done, check if the comparison (if any) was true
    int32_t result;
    __disable_irq(); // make sure nothing interrupts this part
    if (isComplete())
    {                                            // conversion succeded
        result = (int16_t)(int32_t)readSingle(); // cast to 32 bits
        if (res == 16)
        {                // 16 bit differential is actually 15 bit + 1 bit sign
            result *= 2; // multiply by 2 as if it were really 16 bits, so that getMaxValue gives a correct value.
        }
    }
    else
    { // comparison was false
        result = ADC_ERROR_VALUE;
        fail_flag |= ADC_ERROR::COMPARISON;
    }
    __enable_irq();

    // if we interrupted a conversion, set it again
    if (wasADCInUse)
    {
        __disable_irq();
        loadConfig(&old_config);
        __enable_irq();
    }

    num_measurements--;
    return result;

} // analogReadDifferential
#endif

/////////////// NON-BLOCKING CONVERSION METHODS //////////////
/*
    This methods are implemented like this:

    1. Check that the pin is correct
    2. if calibrating, wait for it to finish before modifiying any ADC register
    3. Check if we're interrupting a measurement, if so store the settings (in a member of the class, so it can be accessed).
    4. Disable continuous conversion mode and start the current measurement

    The fast methods only do step 4.

*/

/* Starts an analog measurement on the pin.
*  It returns inmediately, read value with readSingle().
*  If the pin is incorrect it returns false.
*/
bool ADC_Module::startSingleRead(uint8_t pin)
{

    // check whether the pin is correct
    if (!checkPin(pin))
    {
        fail_flag |= ADC_ERROR::WRONG_PIN;
        return false;
    }

    if (calibrating)
        wait_for_cal();

    // save the current state of the ADC in case it's in use
    adcWasInUse = isConverting(); // is the ADC running now?

    if (adcWasInUse)
    { // this means we're interrupting a conversion
        // save the current conversion config, the adc isr will restore the adc
        __disable_irq();
        saveConfig(&adc_config);
        __enable_irq();
    }

    // no continuous mode
    singleMode();

    // start measurement
    startReadFast(pin);

    return true;
}

#if ADC_DIFF_PAIRS > 0
/* Start a differential conversion between two pins (pinP - pinN).
* It returns inmediately, get value with readSingle().
* Incorrect pins will return false.
* Set the resolution, number of averages and voltage reference using the appropriate functions
*/
bool ADC_Module::startSingleDifferential(uint8_t pinP, uint8_t pinN)
{

    if (!checkDifferentialPins(pinP, pinN))
    {
        fail_flag |= ADC_ERROR::WRONG_PIN;
        return false; // all others are invalid
    }

    // check for calibration before setting channels,
    // because conversion will start as soon as we write to adc_regs.SC1A
    if (calibrating)
        wait_for_cal();

    // vars to saved the current state of the ADC in case it's in use
    adcWasInUse = isConverting(); // is the ADC running now?

    if (adcWasInUse)
    { // this means we're interrupting a conversion
        // save the current conversion config, we don't want any other interrupts messing up the configs
        __disable_irq();
        saveConfig(&adc_config);
        __enable_irq();
    }

    // no continuous mode
    singleMode();

    // start the conversion
    startDifferentialFast(pinP, pinN);

    return true;
}
#endif

///////////// CONTINUOUS CONVERSION METHODS ////////////
/*
    This methods are implemented like this:

    1. Check that the pin is correct
    2. If calibrating, wait for it to finish before modifiying any ADC register
    4. Enable continuous conversion mode and start the current measurement

*/

/* Starts continuous conversion on the pin
 * It returns as soon as the ADC is set, use analogReadContinuous() to read the values
 * Set the resolution, number of averages and voltage reference using the appropriate functions BEFORE calling this function
*/
bool ADC_Module::startContinuous(uint8_t pin)
{

    // check whether the pin is correct
    if (!checkPin(pin))
    {
        fail_flag |= ADC_ERROR::WRONG_PIN;
        return false;
    }

    // check for calibration before setting channels,
    if (calibrating)
        wait_for_cal();

    // increase the counter of measurements
    num_measurements++;

    // set continuous conversion flag
    continuousMode();

    startReadFast(pin);

    return true;
}

#if ADC_DIFF_PAIRS > 0
/* Starts continuous and differential conversion between the pins (pinP-pinN)
 * It returns as soon as the ADC is set, use analogReadContinuous() to read the value
 * Set the resolution, number of averages and voltage reference using the appropriate functions BEFORE calling this function
*/
bool ADC_Module::startContinuousDifferential(uint8_t pinP, uint8_t pinN)
{

    if (!checkDifferentialPins(pinP, pinN))
    {
        fail_flag |= ADC_ERROR::WRONG_PIN;
        return false; // all others are invalid
    }

    // increase the counter of measurements
    num_measurements++;

    // check for calibration before setting channels,
    // because conversion will start as soon as we write to adc_regs.SC1A
    if (calibrating)
        wait_for_cal();

    // save the current state of the ADC in case it's in use
    uint8_t wasADCInUse = isConverting(); // is the ADC running now?

    if (wasADCInUse)
    { // this means we're interrupting a conversion
        // save the current conversion config, we don't want any other interrupts messing up the configs
        __disable_irq();
        saveConfig(&adc_config);
        __enable_irq();
    }

    // set continuous mode
    continuousMode();

    // start conversions
    startDifferentialFast(pinP, pinN);

    return true;
}
#endif

/* Stops continuous conversion
*/
void ADC_Module::stopContinuous()
{

// set channel select to all 1's (31) to stop it.
#ifdef ADC_TEENSY_4
    adc_regs.HC0 = ADC_SC1A_PIN_INVALID + interrupts_enabled * ADC_HC_AIEN;
#else
    adc_regs.SC1A = ADC_SC1A_PIN_INVALID + atomic::getBitFlag(adc_regs.SC1A, ADC_SC1_AIEN) * ADC_SC1_AIEN;
#endif

    // decrease the counter of measurements (unless it's 0)
    if (num_measurements > 0)
    {
        num_measurements--;
    }

    return;
}

//////////// FREQUENCY METHODS ////////

//////////// PDB ////////////////
#ifdef ADC_USE_PDB

// frequency in Hz
void ADC_Module::startPDB(uint32_t freq)
{
    if (!(SIM_SCGC6 & SIM_SCGC6_PDB))
    {                               // setup PDB
        SIM_SCGC6 |= SIM_SCGC6_PDB; // enable pdb clock
    }

    if (freq > ADC_F_BUS)
        return; // too high
    if (freq < 1)
        return; // too low

    // mod will have to be a 16 bit value
    // we detect if it's higher than 0xFFFF and scale it back accordingly.
    uint32_t mod = (ADC_F_BUS / freq);

    uint8_t prescaler = 0; // from 0 to 7: factor of 1, 2, 4, 8, 16, 32, 64 or 128
    uint8_t mult = 0;      // from 0 to 3, factor of 1, 10, 20 or 40

    // if mod is too high we need to use prescaler and mult to bring it down to a 16 bit number
    const uint32_t min_level = 0xFFFF;
    if (mod > min_level)
    {
        if (mod < 2 * min_level)
        {
            prescaler = 1;
        }
        else if (mod < 4 * min_level)
        {
            prescaler = 2;
        }
        else if (mod < 8 * min_level)
        {
            prescaler = 3;
        }
        else if (mod < 10 * min_level)
        {
            mult = 1;
        }
        else if (mod < 16 * min_level)
        {
            prescaler = 4;
        }
        else if (mod < 20 * min_level)
        {
            mult = 2;
        }
        else if (mod < 32 * min_level)
        {
            prescaler = 5;
        }
        else if (mod < 40 * min_level)
        {
            mult = 3;
        }
        else if (mod < 64 * min_level)
        {
            prescaler = 6;
        }
        else if (mod < 128 * min_level)
        {
            prescaler = 7;
        }
        else if (mod < 160 * min_level)
        { // 16*10
            prescaler = 4;
            mult = 1;
        }
        else if (mod < 320 * min_level)
        { // 16*20
            prescaler = 4;
            mult = 2;
        }
        else if (mod < 640 * min_level)
        { // 16*40
            prescaler = 4;
            mult = 3;
        }
        else if (mod < 1280 * min_level)
        { // 32*40
            prescaler = 5;
            mult = 3;
        }
        else if (mod < 2560 * min_level)
        { // 64*40
            prescaler = 6;
            mult = 3;
        }
        else if (mod < 5120 * min_level)
        { // 128*40
            prescaler = 7;
            mult = 3;
        }
        else
        { // frequency too low
            return;
        }

        mod >>= prescaler;
        if (mult > 0)
        {
            mod /= 10;
            mod >>= (mult - 1);
        }
    }

    setHardwareTrigger(); // trigger ADC with hardware

    //                                   software trigger    enable PDB     PDB interrupt  continuous mode load immediately
    constexpr uint32_t ADC_PDB_CONFIG = PDB_SC_TRGSEL(15) | PDB_SC_PDBEN | PDB_SC_PDBIE | PDB_SC_CONT | PDB_SC_LDMOD(0);

    constexpr uint32_t PDB_CHnC1_TOS_1 = 0x0100;
    constexpr uint32_t PDB_CHnC1_EN_1 = 0x01;

    PDB0_IDLY = 1; // the pdb interrupt happens when IDLY is equal to CNT+1

    PDB0_MOD = (uint16_t)(mod - 1);

    PDB0_SC = ADC_PDB_CONFIG | PDB_SC_PRESCALER(prescaler) | PDB_SC_MULT(mult) | PDB_SC_LDOK; // load all new values

    PDB0_SC = ADC_PDB_CONFIG | PDB_SC_PRESCALER(prescaler) | PDB_SC_MULT(mult) | PDB_SC_SWTRIG; // start the counter!

    PDB0_CHnC1 = PDB_CHnC1_TOS_1 | PDB_CHnC1_EN_1; // enable pretrigger 0 (SC1A)

    //NVIC_ENABLE_IRQ(IRQ_PDB);
}

void ADC_Module::stopPDB()
{
    if (!(SIM_SCGC6 & SIM_SCGC6_PDB))
    { // if PDB clock wasn't on, return
        setSoftwareTrigger();
        return;
    }
    PDB0_SC = 0;
    setSoftwareTrigger();

    //NVIC_DISABLE_IRQ(IRQ_PDB);
}

//! Return the PDB's frequency
uint32_t ADC_Module::getPDBFrequency()
{
    const uint32_t mod = (uint32_t)PDB0_MOD;
    const uint8_t prescaler = (PDB0_SC & 0x7000) >> 12;
    const uint8_t mult = (PDB0_SC & 0xC) >> 2;

    const uint32_t freq = uint32_t((mod + 1) << (prescaler)) * uint32_t((mult == 0) ? 1 : 10 << (mult - 1));
    return ADC_F_BUS / freq;
}

#endif

#ifdef ADC_USE_QUAD_TIMER
// TODO: Add support for Teensy 3.x Quad timer
#if defined(ADC_TEENSY_4) // only supported by Teensy 4...
// try to use some teensy core functions...
// mainly out of pwm.c
extern "C"
{
    extern void xbar_connect(unsigned int input, unsigned int output);
    extern void quadtimer_init(IMXRT_TMR_t *p);
    extern void quadtimerWrite(IMXRT_TMR_t *p, unsigned int submodule, uint16_t val);
    extern void quadtimerFrequency(IMXRT_TMR_t *p, unsigned int submodule, float frequency);
}

void ADC_Module::startQuadTimer(uint32_t freq)
{
    // First lets setup the XBAR
    CCM_CCGR2 |= CCM_CCGR2_XBAR1(CCM_CCGR_ON); //turn clock on for xbara1
    xbar_connect(XBAR_IN, XBAR_OUT);

    // Update the ADC
    uint8_t adc_pin_channel = adc_regs.HC0 & 0x1f; // remember the trigger that was set
    setHardwareTrigger();                          // set the hardware trigger
    adc_regs.HC0 = (adc_regs.HC0 & ~0x1f) | 16;    // ADC_ETC channel remember other states...
    singleMode();                                  // make sure continuous is turned off as you want the trigger to di it.

    // setup adc_etc - BUGBUG have not used the preset values yet.
    if (IMXRT_ADC_ETC.CTRL & ADC_ETC_CTRL_SOFTRST)
    { // SOFTRST
        // Soft reset
        atomic::clearBitFlag(IMXRT_ADC_ETC.CTRL, ADC_ETC_CTRL_SOFTRST);
        delay(5); // give some time to be sure it is init
    }
    if (ADC_num == 0)
    { // BUGBUG - in real code, should probably know we init ADC or not..
        IMXRT_ADC_ETC.CTRL |=
            (ADC_ETC_CTRL_TSC_BYPASS | ADC_ETC_CTRL_DMA_MODE_SEL | ADC_ETC_CTRL_TRIG_ENABLE(1 << ADC_ETC_TRIGGER_INDEX)); // 0x40000001;  // start with trigger 0
        IMXRT_ADC_ETC.TRIG[ADC_ETC_TRIGGER_INDEX].CTRL = ADC_ETC_TRIG_CTRL_TRIG_CHAIN(0);                                 // chainlength -1 only us
        IMXRT_ADC_ETC.TRIG[ADC_ETC_TRIGGER_INDEX].CHAIN_1_0 =
            ADC_ETC_TRIG_CHAIN_IE0(1) /*| ADC_ETC_TRIG_CHAIN_B2B0 */
            | ADC_ETC_TRIG_CHAIN_HWTS0(1) | ADC_ETC_TRIG_CHAIN_CSEL0(adc_pin_channel);

        if (interrupts_enabled)
        {
            // Not sure yet?
        }
        if (adc_regs.GC & ADC_GC_DMAEN)
        {
            IMXRT_ADC_ETC.DMA_CTRL |= ADC_ETC_DMA_CTRL_TRIQ_ENABLE(ADC_ETC_TRIGGER_INDEX);
        }
    }
    else
    {
        // This is our second one... Try second trigger?
        // Remove the BYPASS?
        IMXRT_ADC_ETC.CTRL &= ~(ADC_ETC_CTRL_TSC_BYPASS);                                                       // 0x40000001;  // start with trigger 0
        IMXRT_ADC_ETC.CTRL |= ADC_ETC_CTRL_DMA_MODE_SEL | ADC_ETC_CTRL_TRIG_ENABLE(1 << ADC_ETC_TRIGGER_INDEX); // Add trigger
        IMXRT_ADC_ETC.TRIG[ADC_ETC_TRIGGER_INDEX].CTRL = ADC_ETC_TRIG_CTRL_TRIG_CHAIN(0);                       // chainlength -1 only us
        IMXRT_ADC_ETC.TRIG[ADC_ETC_TRIGGER_INDEX].CHAIN_1_0 =
            ADC_ETC_TRIG_CHAIN_IE0(1) /*| ADC_ETC_TRIG_CHAIN_B2B0 */
            | ADC_ETC_TRIG_CHAIN_HWTS0(1) | ADC_ETC_TRIG_CHAIN_CSEL0(adc_pin_channel);

        if (adc_regs.GC & ADC_GC_DMAEN)
        {
            IMXRT_ADC_ETC.DMA_CTRL |= ADC_ETC_DMA_CTRL_TRIQ_ENABLE(ADC_ETC_TRIGGER_INDEX);
        }
    }

    // Now init the QTimer.
    // Extracted from quadtimer_init in pwm.c but only the one channel...
    // Maybe see if we have to do this every time we call this.  But how often is that?
    IMXRT_TMR4.CH[QTIMER4_INDEX].CTRL = 0; // stop timer
    IMXRT_TMR4.CH[QTIMER4_INDEX].CNTR = 0;
    IMXRT_TMR4.CH[QTIMER4_INDEX].SCTRL = TMR_SCTRL_OEN | TMR_SCTRL_OPS | TMR_SCTRL_VAL | TMR_SCTRL_FORCE;
    IMXRT_TMR4.CH[QTIMER4_INDEX].CSCTRL = TMR_CSCTRL_CL1(1) | TMR_CSCTRL_ALT_LOAD;
    // COMP must be less than LOAD - otherwise output is always low
    IMXRT_TMR4.CH[QTIMER4_INDEX].LOAD = 24000; // low time  (65537 - x) -
    IMXRT_TMR4.CH[QTIMER4_INDEX].COMP1 = 0;    // high time (0 = always low, max = LOAD-1)
    IMXRT_TMR4.CH[QTIMER4_INDEX].CMPLD1 = 0;
    IMXRT_TMR4.CH[QTIMER4_INDEX].CTRL = TMR_CTRL_CM(1) | TMR_CTRL_PCS(8) |
                                        TMR_CTRL_LENGTH | TMR_CTRL_OUTMODE(6);

    quadtimerFrequency(&IMXRT_TMR4, QTIMER4_INDEX, freq);
    quadtimerWrite(&IMXRT_TMR4, QTIMER4_INDEX, 5);
}

//! Stop the PDB
void ADC_Module::stopQuadTimer()
{
    quadtimerWrite(&IMXRT_TMR4, QTIMER4_INDEX, 0);
    setSoftwareTrigger();
}

//! Return the PDB's frequency
uint32_t ADC_Module::getQuadTimerFrequency()
{
    // Can I reverse the calculations of quad timer set frequency?
    uint32_t high = IMXRT_TMR4.CH[QTIMER4_INDEX].CMPLD1;
    uint32_t low = 65537 - IMXRT_TMR4.CH[QTIMER4_INDEX].LOAD;
    uint32_t highPlusLow = high + low; //
    if (highPlusLow == 0)
        return 0; //

    uint8_t pcs = (IMXRT_TMR4.CH[QTIMER4_INDEX].CTRL >> 9) & 0x7;
    uint32_t freq = (F_BUS_ACTUAL >> pcs) / highPlusLow;
    //Serial.printf("ADC_Module::getTimerFrequency H:%u L:%u H+L=%u pcs:%u freq:%u\n", high, low, highPlusLow, pcs, freq);
    return freq;
}

#endif // Teensy 4
#endif // ADC_USE_QUAD_TIMER