/*
    ------------------------------------------------------------------------------------------------------
    i2c_t3 - I2C library for Teensy 3.0/3.1/LC

    - (v9) Modified 01Jul16 by Brian (nox771 at gmail.com)
        - Added support for Teensy 3.5/3.6:
            - fully supported (Master/Slave modes, IMM/ISR/DMA operation)
            - supports all available pin/bus options on Wire/Wire1/Wire2/Wire3
        - Fixed LC slave bug, whereby it was incorrectly detecting STOPs directed to other slaves
        - I2C rate is now set using a much more flexible method than previously used (this is partially
          motivated by increasing device count and frequencies).  As a result, the fixed set of rate
          enums are no longer needed (however they are currently still supported), and desired I2C
          frequency can be directly specified, eg. for 400kHz, I2C_RATE_400 can be replaced by 400000.
          Some setRate() functions are deprecated due to these changes.

    - (v8) Modified 02Apr15 by Brian (nox771 at gmail.com)
        - added support for Teensy LC:
            - fully supported (Master/Slave modes, IMM/ISR/DMA operation)
            - Wire: pins 16/17 or 18/19, rate limited to I2C_RATE_1200
            - Wire1: pins 22/23, rate limited to I2C_RATE_2400
        - added timeout on acquiring bus (prevents lockup when bus cannot be acquired)
        - added setDefaultTimeout() function for setting the default timeout to apply to all commands
        - added resetBus() function for toggling SCL to release stuck Slave devices
        - added setRate(rate) function, similar to setClock(freq), but using rate specifiers (does not
                require specifying busFreq)
        - added I2C_AUTO_RETRY user define

    - (v7) Modified 09Jan15 by Brian (nox771 at gmail.com)
        - added support for F_BUS frequencies: 60MHz, 56MHz, 48MHz, 36MHz, 24MHz, 16MHz, 8MHz, 4MHz, 2MHz
        - added new rates: I2C_RATE_1800, I2C_RATE_2800, I2C_RATE_3000
        - added new priority escalation - in cases where I2C ISR is blocked by having a lower priority than
                                          calling function, the I2C will either adjust I2C ISR to a higher priority,
                                          or switch to Immediate mode as needed.
        - added new operating mode control - I2C can be set to operate in ISR mode, DMA mode (Master only),
                                             or Immediate Mode (Master only)
        - added new begin() functions to allow setting the initial operating mode:
            - begin(i2c_mode mode, uint8_t address, i2c_pins pins, i2c_pullup pullup, i2c_rate rate, i2c_op_mode opMode)
            - begin(i2c_mode mode, uint8_t address1, uint8_t address2, i2c_pins pins, i2c_pullup pullup, i2c_rate rate, i2c_op_mode opMode)
        - added new functions:
            - uint8_t setOpMode(i2c_op_mode opMode) - used to change operating mode on the fly (only when bus is idle)
            - void sendTransmission() - non-blocking Tx with implicit I2C_STOP, added for symmetry with endTransmission()
            - uint8_t setRate(uint32_t busFreq, i2c_rate rate) - used to set I2C clock dividers to get desired rate, i2c_rate argument
            - uint8_t setRate(uint32_t busFreq, uint32_t i2cFreq) - used to set I2C clock dividers to get desired SCL freq, uint32_t argument
                                                                    (quantized to nearest i2c_rate)
        - added new Wire compatibility functions:
            - void setClock(uint32_t i2cFreq) - (note: degenerate form of setRate() with busFreq == F_BUS)
            - uint8_t endTransmission(uint8_t sendStop)
            - uint8_t requestFrom(uint8_t addr, uint8_t len)
            - uint8_t requestFrom(uint8_t addr, uint8_t len, uint8_t sendStop)
        - fixed bug in Slave Range code whereby onRequest() callback occurred prior to updating rxAddr instead of after
        - fixed bug in arbitration, was missing from Master Tx mode
        - removed I2C1 defines (now included in kinetis.h)
        - removed all debug code (eliminates rbuf dependency)

    - (v6) Modified 16Jan14 by Brian (nox771 at gmail.com)
        - all new structure using dereferenced pointers instead of hardcoding. This allows functions
          (including ISRs) to be reused across multiple I2C buses.  Most functions moved to static,
          which in turn are called by inline user functions.  Added new struct (i2cData) for holding all
          bus information.
        - added support for Teensy 3.1 and I2C1 interface on pins 29/30 and 26/31.
        - added header define (I2C_BUS_ENABLE n) to control number of enabled buses (eg. both I2C0 & I2C1
          or just I2C0).  When using only I2C0 the code and ram usage will be lower.
        - added interrupt flag (toggles pin high during ISR) with independent defines for I2C0 and
          I2C1 (refer to header file), useful for logic analyzer trigger

    - (v5) Modified 09Jun13 by Brian (nox771 at gmail.com)
        - fixed bug in ISR timeout code in which timeout condition could fail to reset in certain cases
        - fixed bug in Slave mode in sda_rising_isr attach, whereby it was not getting attached on the addr byte
        - moved debug routines so they are entirely defined internal to the library (no end user code req'd)
        - debug routines now use IntervalTimer library
        - added support for range of Slave addresses
        - added getRxAddr() for Slave using addr range to determine its called address
        - removed virtual keyword from all functions (is not a base class)

    - (v1-v4) Modified 26Feb13 by Brian (nox771 at gmail.com)
        - Reworked begin function:
            - added option for pins to use (SCL:SDA on 19:18 or 16:17 - note pin order difference)
            - added option for internal pullup - as mentioned in previous code pullup is very strong,
                                                 approx 190 ohms, but is possibly useful for high speed I2C
            - added option for rates - 100kHz, 200kHz, 300kHz, 400kHz, 600kHz, 800kHz, 1MHz, 1.2MHz, <-- 24/48MHz bus
                                       1.5MHz, 2.0MHz, 2.4MHz                                        <-- 48MHz bus only
        - Removed string.h dependency (memcpy)
        - Changed Master modes to interrupt driven
        - Added non-blocking Tx/Rx routines, and status/done/finish routines:
            - sendTransmission() - non-blocking transmit
            - sendRequest() - non-blocking receive
            - status() - reports current status
            - done() - indicates Tx/Rx complete (for main loop polling if I2C is running in background)
            - finish() - loops until Tx/Rx complete or bus error
        - Added readByte()/peekByte() for uint8_t return values (note: returns 0 instead of -1 if buf empty)
        - Added fixes for Slave Rx mode - in short Slave Rx on this part is fubar
          (as proof, notice the difference in the I2Cx_FLT register in the KL25 Sub-Family parts)
            - the SDA-rising ISR hack can work but only detects STOP conditons.
              A slave Rx followed by RepSTART won't be detected since bus remains busy.
              To fix this if IAAS occurs while already in Slave Rx mode then it will
              assume RepSTART occurred and trigger onReceive callback.
        - Separated Tx/Rx buffer sizes for asymmetric devices (adjustable in i2c_t3.h)
        - Changed Tx/Rx buffer indicies to size_t to allow for large (>256 byte) buffers
        - Left debug routines in place (controlled via header defines - default is OFF).  If debug is
            enabled, note that it can easily overrun the Debug queue on large I2C transfers, yielding
            garbage output.  Adjust ringbuf size (in rbuf.h) and possibly PIT interrupt rate to adjust
            data flow to Serial (note also the buffer in Serial can overflow if written too quickly).
        - Added getError() function to return Wire error code
        - Added pinConfigure() function for changing pins on the fly (only when bus not busy)
        - Added timeouts to endTransmission(), requestFrom(), and finish()
    ------------------------------------------------------------------------------------------------------
    Some code segments derived from:
    TwoWire.cpp - TWI/I2C library for Wiring & Arduino
    Copyright (c) 2006 Nicholas Zambetti.  All right reserved.
    Modified 2012 by Todd Krein (todd@krein.org) to implement repeated starts
    ------------------------------------------------------------------------------------------------------
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MKL26Z64__) || \
    defined(__MK64FX512__) || defined(__MK66FX1M0__) // 3.0/3.1-3.2/LC/3.5/3.6

#include "i2c_t3.h"


// ------------------------------------------------------------------------------------------------------
// Static inits
//
#define I2C_STRUCT(a1,f,c1,s,d,c2,flt,ra,smb,a2,slth,sltl,pins) \
    {a1, f, c1, s, d, c2, flt, ra, smb, a2, slth, sltl, {}, 0, 0, {}, 0, 0, I2C_OP_MODE_ISR, I2C_MASTER, pins, \
     I2C_PULLUP_EXT, 100000, I2C_STOP, I2C_WAITING, 0, 0, 0, 0, I2C_DMA_OFF, nullptr, nullptr, nullptr, 0}

struct i2cStruct i2c_t3::i2cData[] =
{
    I2C_STRUCT(&I2C0_A1, &I2C0_F, &I2C0_C1, &I2C0_S, &I2C0_D, &I2C0_C2, &I2C0_FLT, &I2C0_RA, &I2C0_SMB, &I2C0_A2, &I2C0_SLTH, &I2C0_SLTL, I2C_PINS_18_19)
#if (I2C_BUS_NUM >= 2) && defined(__MK20DX256__) // 3.1/3.2
   ,I2C_STRUCT(&I2C1_A1, &I2C1_F, &I2C1_C1, &I2C1_S, &I2C1_D, &I2C1_C2, &I2C1_FLT, &I2C1_RA, &I2C1_SMB, &I2C1_A2, &I2C1_SLTH, &I2C1_SLTL, I2C_PINS_29_30)
#elif (I2C_BUS_NUM >= 2) && defined(__MKL26Z64__) // LC
   ,I2C_STRUCT(&I2C1_A1, &I2C1_F, &I2C1_C1, &I2C1_S, &I2C1_D, &I2C1_C2, &I2C1_FLT, &I2C1_RA, &I2C1_SMB, &I2C1_A2, &I2C1_SLTH, &I2C1_SLTL, I2C_PINS_22_23)
#elif (I2C_BUS_NUM >= 2) && (defined(__MK64FX512__) || defined(__MK66FX1M0__))  // 3.5/3.6
   ,I2C_STRUCT(&I2C1_A1, &I2C1_F, &I2C1_C1, &I2C1_S, &I2C1_D, &I2C1_C2, &I2C1_FLT, &I2C1_RA, &I2C1_SMB, &I2C1_A2, &I2C1_SLTH, &I2C1_SLTL, I2C_PINS_37_38)
#endif
#if (I2C_BUS_NUM >= 3) && (defined(__MK64FX512__) || defined(__MK66FX1M0__))  // 3.5/3.6
   ,I2C_STRUCT(&I2C2_A1, &I2C2_F, &I2C2_C1, &I2C2_S, &I2C2_D, &I2C2_C2, &I2C2_FLT, &I2C2_RA, &I2C2_SMB, &I2C2_A2, &I2C2_SLTH, &I2C2_SLTL, I2C_PINS_3_4)
#endif
#if (I2C_BUS_NUM >= 4) && defined(__MK66FX1M0__) // 3.6
   ,I2C_STRUCT(&I2C3_A1, &I2C3_F, &I2C3_C1, &I2C3_S, &I2C3_D, &I2C3_C2, &I2C3_FLT, &I2C3_RA, &I2C3_SMB, &I2C3_A2, &I2C3_SLTH, &I2C3_SLTL, I2C_PINS_56_57)
#endif
};


// ------------------------------------------------------------------------------------------------------
// Constructor/Destructor
//
i2c_t3::i2c_t3(uint8_t i2c_bus)
{
    bus = i2c_bus;
    i2c = &i2cData[bus];
}
i2c_t3::~i2c_t3()
{
    // if DMA active, delete DMA object
    if(i2c->opMode == I2C_OP_MODE_DMA)
        delete i2c->DMA;
}


// ------------------------------------------------------------------------------------------------------
// Initialize I2C - initializes I2C as Master or address range Slave
// return: none
// parameters:
//      mode = I2C_MASTER, I2C_SLAVE
//      address1 = 1st 7bit address for specifying Slave address range (ignored for Master mode)
//      address2 = 2nd 7bit address for specifying Slave address range (ignored for Master mode)
//      pins = pins to use, options are:
//          Interface  Devices     Pin Name      SCL    SDA
//          ---------  -------  --------------  -----  -----    (note: in almost all cases SCL is the
//             Wire      All    I2C_PINS_16_17    16     17      lower pin #, except cases marked *)
//             Wire      All    I2C_PINS_18_19    19     18  *
//             Wire    3.5/3.6  I2C_PINS_7_8       7      8
//             Wire    3.5/3.6  I2C_PINS_33_34    33     34
//             Wire    3.5/3.6  I2C_PINS_47_48    47     48
//            Wire1       LC    I2C_PINS_22_23    22     23
//            Wire1    3.1/3.2  I2C_PINS_26_31    26     31
//            Wire1    3.1/3.2  I2C_PINS_29_30    29     30
//            Wire1    3.5/3.6  I2C_PINS_37_38    37     38
//            Wire2    3.5/3.6  I2C_PINS_3_4       3      4
//            Wire3      3.6    I2C_PINS_56_57    57     56  *
//      pullup = I2C_PULLUP_EXT, I2C_PULLUP_INT
//      rate = I2C frequency to use, can be specified directly in Hz, eg. 400000 for 400kHz, or using one of the
//             following enum values (deprecated):
//             I2C_RATE_100, I2C_RATE_200, I2C_RATE_300, I2C_RATE_400,
//             I2C_RATE_600, I2C_RATE_800, I2C_RATE_1000, I2C_RATE_1200,
//             I2C_RATE_1500, I2C_RATE_1800, I2C_RATE_2000, I2C_RATE_2400,
//             I2C_RATE_2800, I2C_RATE_3000
//      opMode = I2C_OP_MODE_IMM, I2C_OP_MODE_ISR, I2C_OP_MODE_DMA (ignored for Slave mode, defaults to ISR)
//
void i2c_t3::begin_(struct i2cStruct* i2c, uint8_t bus, i2c_mode mode, uint8_t address1, uint8_t address2,
                    i2c_pins pins, i2c_pullup pullup, uint32_t rate, i2c_op_mode opMode)
{
    // Enable I2C internal clock
    if(bus == 0)
        SIM_SCGC4 |= SIM_SCGC4_I2C0;
    #if I2C_BUS_NUM >= 2
        if(bus == 1)
            SIM_SCGC4 |= SIM_SCGC4_I2C1;
    #endif
    #if I2C_BUS_NUM >= 3
        if(bus == 2)
            SIM_SCGC1 |= SIM_SCGC1_I2C2;
    #endif
    #if I2C_BUS_NUM >= 4
        if(bus == 3)
            SIM_SCGC1 |= SIM_SCGC1_I2C3;
    #endif

    i2c->currentMode = mode; // Set mode
    i2c->currentStatus = I2C_WAITING; // reset status

    // Set Master/Slave address (zeroed in Master to prevent accidental Rx when setup is changed dynamically)
    if(i2c->currentMode == I2C_MASTER)
    {
        *(i2c->C2) = I2C_C2_HDRS; // Set high drive select
        *(i2c->A1) = 0;
        *(i2c->RA) = 0;
    }
    else
    {
        *(i2c->C2) = (address2) ? (I2C_C2_HDRS|I2C_C2_RMEN) // Set high drive select and range-match enable
                                : I2C_C2_HDRS;              // Set high drive select
        // set Slave address, if two addresses are given, setup range and put lower address in A1, higher in RA
        *(i2c->A1) = (address2) ? ((address1 < address2) ? (address1<<1) : (address2<<1))
                                : (address1<<1);
        *(i2c->RA) = (address2) ? ((address1 < address2) ? (address2<<1) : (address1<<1))
                                : 0;
    }

    // Setup pins and options (note: does not "unset" unused pins if changed).  As noted in
    // original TwoWire.cpp, internal 3.0/3.1 pullup is strong (about 190 ohms), but it can
    // work if other devices on bus have strong enough pulldown devices (usually true).
    //
    pinConfigure_(i2c, bus, pins, pullup, 0);

    // Set I2C rate
    #if defined(__MKL26Z64__) // LC
        if(bus == 1)
            setRate_(i2c, (uint32_t)F_CPU, rate); // LC Wire1 bus uses system clock (F_CPU) instead of bus clock (F_BUS)
        else
            setRate_(i2c, (uint32_t)F_BUS, rate);
    #else
        setRate_(i2c, (uint32_t)F_BUS, rate);
    #endif

    // Set config registers and operating mode
    setOpMode_(i2c, bus, opMode);
    if(i2c->currentMode == I2C_MASTER)
        *(i2c->C1) = I2C_C1_IICEN; // Master - enable I2C (hold in Rx mode, intr disabled)
    else
        *(i2c->C1) = I2C_C1_IICEN|I2C_C1_IICIE; // Slave - enable I2C and interrupts
}


// Get Default Pins - this obtains the default pins to use when using simplified begin() calls,
//                    intended for internal use only
// return: i2c_pins - default pin setting:
//                    Wire:  I2C_PINS_18_19
//                    Wire1: I2C_PINS_29_30 (3.1/3.2), I2C_PINS_22_23 (LC), I2C_PINS_37_38 (3.5/3.6)
//                    Wire2: I2C_PINS_3_4   (3.5/3.6)
//                    Wire3: I2C_PINS_56_57 (3.6)
//
i2c_pins i2c_t3::getDefaultPins_(uint8_t bus)
{
    i2c_pins defpins = I2C_PINS_18_19;
    #if defined(__MKL26Z64__)
        defpins = (bus == 1) ? I2C_PINS_22_23 : I2C_PINS_18_19; // LC
    #elif defined(__MK20DX128__)
        defpins = I2C_PINS_18_19; // 3.0
    #elif defined(__MK20DX256__)
        defpins = (bus == 1) ? I2C_PINS_29_30 : I2C_PINS_18_19; // 3.1/3.2
    #elif defined(__MK64FX512__)
        defpins = (bus == 2) ? I2C_PINS_3_4   :
                  (bus == 1) ? I2C_PINS_37_38 :
                               I2C_PINS_18_19; // 3.5
    #elif defined(__MK66FX1M0__)
        defpins = (bus == 3) ? I2C_PINS_56_57 :
                  (bus == 2) ? I2C_PINS_3_4   :
                  (bus == 1) ? I2C_PINS_37_38 :
                               I2C_PINS_18_19; // 3.6
    #endif
    return defpins;
}


// Set Operating Mode - this configures operating mode of the I2C as either Immediate, ISR, or DMA.
//                      By default Arduino-style begin() calls will initialize to ISR mode.  This can
//                      only be called when the bus is idle (no changing mode in the middle of Tx/Rx).
//                      Note that Slave mode can only use ISR operation.
// return: 1=success, 0=fail (bus busy)
// parameters:
//      opMode = I2C_OP_MODE_ISR, I2C_OP_MODE_DMA, I2C_OP_MODE_IMM
//
uint8_t i2c_t3::setOpMode_(struct i2cStruct* i2c, uint8_t bus, i2c_op_mode opMode)
{
    if(*(i2c->S) & I2C_S_BUSY) return 0; // return immediately if bus busy

    *(i2c->C1) = I2C_C1_IICEN; // reset I2C modes, stop intr, stop DMA
    *(i2c->S) = I2C_S_IICIF | I2C_S_ARBL; // clear status flags just in case

    // Slaves can only use ISR
    if(i2c->currentMode == I2C_SLAVE) opMode = I2C_OP_MODE_ISR;

    if(opMode == I2C_OP_MODE_IMM)
    {
        i2c->opMode = I2C_OP_MODE_IMM;
    }
    if(opMode == I2C_OP_MODE_ISR || opMode == I2C_OP_MODE_DMA)
    {
        // Nested Vec Interrupt Ctrl - enable I2C interrupt
        if(bus == 0)
        {
            NVIC_ENABLE_IRQ(IRQ_I2C0);
            I2C0_INTR_FLAG_INIT; // init I2C0 interrupt flag if used
        }
        #if I2C_BUS_NUM >= 2
            if(bus == 1)
            {
                NVIC_ENABLE_IRQ(IRQ_I2C1);
                I2C1_INTR_FLAG_INIT; // init I2C1 interrupt flag if used
            }
        #endif
        #if I2C_BUS_NUM >= 3
            if(bus == 2)
            {
                NVIC_ENABLE_IRQ(IRQ_I2C2);
                I2C2_INTR_FLAG_INIT; // init I2C2 interrupt flag if used
            }
        #endif
        #if I2C_BUS_NUM >= 4
            if(bus == 3)
            {
                NVIC_ENABLE_IRQ(IRQ_I2C3);
                I2C3_INTR_FLAG_INIT; // init I2C3 interrupt flag if used
            }
        #endif
        if(opMode == I2C_OP_MODE_DMA)
        {
            // attempt to get a DMA Channel (if not already allocated)
            if(i2c->DMA == nullptr)
                i2c->DMA = new DMAChannel();
            // check if object created but no available channel
            if(i2c->DMA != nullptr && i2c->DMA->channel == DMA_NUM_CHANNELS)
            {
                // revert to ISR mode if no DMA channels avail
                delete i2c->DMA;
                i2c->DMA = nullptr;
                i2c->opMode = I2C_OP_MODE_ISR;
            }
            else
            {
                // DMA object has valid channel
                if(bus == 0)
                {
                    // setup static DMA settings
                    i2c->DMA->disableOnCompletion();
                    i2c->DMA->attachInterrupt(i2c0_isr);
                    i2c->DMA->interruptAtCompletion();
                    i2c->DMA->triggerAtHardwareEvent(DMAMUX_SOURCE_I2C0);
                }
                #if I2C_BUS_NUM >= 2
                    if(bus == 1)
                    {
                        // setup static DMA settings
                        i2c->DMA->disableOnCompletion();
                        i2c->DMA->attachInterrupt(i2c1_isr);
                        i2c->DMA->interruptAtCompletion();
                        i2c->DMA->triggerAtHardwareEvent(DMAMUX_SOURCE_I2C1);
                    }
                #endif
                #if I2C_BUS_NUM >= 3
                    // note: on T3.6 I2C2 shares DMAMUX with I2C1
                    if(bus == 2)
                    {
                        // setup static DMA settings
                        i2c->DMA->disableOnCompletion();
                        i2c->DMA->attachInterrupt(i2c2_isr);
                        i2c->DMA->interruptAtCompletion();
                        i2c->DMA->triggerAtHardwareEvent(DMAMUX_SOURCE_I2C2);
                    }
                #endif
                #if I2C_BUS_NUM >= 4
                    // note: on T3.6 I2C3 shares DMAMUX with I2C0
                    if(bus == 3)
                    {
                        // setup static DMA settings
                        i2c->DMA->disableOnCompletion();
                        i2c->DMA->attachInterrupt(i2c3_isr);
                        i2c->DMA->interruptAtCompletion();
                        i2c->DMA->triggerAtHardwareEvent(DMAMUX_SOURCE_I2C3);
                    }
                #endif
                i2c->activeDMA = I2C_DMA_OFF;
                i2c->opMode = I2C_OP_MODE_DMA;
            }
        }
        else
            i2c->opMode = I2C_OP_MODE_ISR;
    }
    return 1;
}


// Set I2C rate - reconfigures I2C frequency divider based on supplied bus freq and desired I2C freq.
//                This will be done assuming an idealized I2C rate, even though at high I2C rates
//                the actual throughput is much lower than theoretical value.
//
//                Since the division ratios are quantized with non-uniform spacing, the selected rate
//                will be the one using the nearest available divider.
// return: none
// parameters:
//      busFreq = bus frequency, typically F_BUS unless reconfigured
//      freq = desired I2C frequency (will be quantized to nearest rate), or can be I2C_RATE_XXX enum (deprecated),
//             such as I2C_RATE_100, I2C_RATE_400, etc...
//
// Max I2C rate is 1/20th F_BUS.  Some examples:
//
//     F_CPU      F_BUS     Max I2C
//     (MHz)      (MHz)       Rate
// -------------  -----    ----------
//    240/120      120        6.0M    bus overclock
//      216        108        5.4M    bus overclock
//     192/96       96        4.8M    bus overclock
//      180         90        4.5M    bus overclock
//      240         80        4.0M    bus overclock
//   216/144/72     72        3.6M    bus overclock
//      192         64        3.2M    bus overclock
//  240/180/120     60        3.0M
//      168         56        2.8M
//      216         54        2.7M
// 192/144/96/48    48        2.4M
//       72         36        1.8M
//       24         24        1.2M
//       16         16        800k
//        8          8        400k
//        4          4        200k
//        2          2        100k
//
void i2c_t3::setRate_(struct i2cStruct* i2c, uint32_t busFreq, uint32_t i2cFreq)
{
    int32_t target_div = ((busFreq/1000)<<8)/(i2cFreq/1000);
    size_t idx;
    // find closest divide ratio
    for(idx=0; idx < sizeof(i2c_div_num)/sizeof(i2c_div_num[0]) && (i2c_div_num[idx]<<8) <= target_div; idx++);
    if(idx && abs(target_div-(i2c_div_num[idx-1]<<8)) <= abs(target_div-(i2c_div_num[idx]<<8))) idx--;
    // Set divider to set rate
    *(i2c->F) = i2c_div_ratio[idx];
    // save current rate setting
    i2c->currentRate = busFreq/i2c_div_num[idx];

    // Set filter
    if(busFreq >= 48000000)
        *(i2c->FLT) = 4;
    else
        *(i2c->FLT) = busFreq/12000000;
}


// ------------------------------------------------------------------------------------------------------
// Configure I2C pins - reconfigures active I2C pins on-the-fly (only works when bus is idle).  If reconfig
//                      set then inactive pins will switch to input mode using same pullup configuration.
// return: 1=success, 0=fail (bus busy or incompatible pins)
// parameters:
//      pins = pins to use, options are:
//          Interface  Devices     Pin Name      SCL    SDA
//          ---------  -------  --------------  -----  -----    (note: in almost all cases SCL is the
//             Wire      All    I2C_PINS_16_17    16     17      lower pin #, except cases marked *)
//             Wire      All    I2C_PINS_18_19    19     18  *
//             Wire    3.5/3.6  I2C_PINS_7_8       7      8
//             Wire    3.5/3.6  I2C_PINS_33_34    33     34
//             Wire    3.5/3.6  I2C_PINS_47_48    47     48
//            Wire1       LC    I2C_PINS_22_23    22     23
//            Wire1    3.1/3.2  I2C_PINS_26_31    26     31
//            Wire1    3.1/3.2  I2C_PINS_29_30    29     30
//            Wire1    3.5/3.6  I2C_PINS_37_38    37     38
//            Wire2    3.5/3.6  I2C_PINS_3_4       3      4
//            Wire3      3.6    I2C_PINS_56_57    57     56  *
//      pullup = I2C_PULLUP_EXT, I2C_PULLUP_INT
//      reconfig = 1=reconfigure old pins, 0=do not reconfigure old pins (base routine only)
//
#define PIN_CONFIG_ALT(name,alt) uint32_t name = (pullup == I2C_PULLUP_EXT) ? (PORT_PCR_MUX(alt)|PORT_PCR_ODE|PORT_PCR_SRE|PORT_PCR_DSE) \
                                                                            : (PORT_PCR_MUX(alt)|PORT_PCR_PE|PORT_PCR_PS)

uint8_t i2c_t3::pinConfigure_(struct i2cStruct* i2c, uint8_t bus, i2c_pins pins, i2c_pullup pullup, uint8_t reconfig)
{
    uint8_t retval = 0;

    if(reconfig && (*(i2c->S) & I2C_S_BUSY)) return 0; // if reconfig return immediately if bus busy (otherwise assume initial setup)

    // Create config settings
    //
    PIN_CONFIG_ALT(pinConfigAlt2,2);
    #if defined(__MK20DX256__) // 3.1/3.2
        PIN_CONFIG_ALT(pinConfigAlt6,6);
    #endif
    #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
        PIN_CONFIG_ALT(pinConfigAlt5,5);
        PIN_CONFIG_ALT(pinConfigAlt7,7);
    #endif

    // Verify new pin setting is valid
    //
    if(bus == 0)
    {
        switch(pins)
        {
        case I2C_PINS_16_17: retval = 1; break;
        case I2C_PINS_18_19: retval = 1; break;
        #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
        case I2C_PINS_7_8:   retval = 1; break;
        case I2C_PINS_33_34: retval = 1; break;
        case I2C_PINS_47_48: retval = 1; break;
        #endif
        default: break;
        }
    }
    #if I2C_BUS_NUM >= 2
        if(bus == 1)
        {
            switch(pins)
            {
            #if defined(__MKL26Z64__) // LC
            case I2C_PINS_22_23: retval = 1; break;
            #endif
            #if defined(__MK20DX256__) // 3.1/3.2
            case I2C_PINS_26_31: retval = 1; break;
            case I2C_PINS_29_30: retval = 1; break;
            #endif
            #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
            case I2C_PINS_37_38: retval = 1; break;
            #endif
            default: break;
            }
        }
    #endif
    #if I2C_BUS_NUM >= 3
        if(bus == 2)
        {
            switch(pins)
            {
            #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
            case I2C_PINS_3_4: retval = 1; break;
            #endif
            default: break;
            }
        }
    #endif
    #if I2C_BUS_NUM >= 4
        if(bus == 3)
        {
            switch(pins)
            {
            #if defined(__MK66FX1M0__) // 3.6
            case I2C_PINS_56_57: retval = 1; break;
            #endif
            default: break;
            }
        }
    #endif

    // If compatible pin setting found then reconfig (break-before-make) and update.
    //
    // If pins are given an impossible value (eg. I2C0 with I2C_PINS_26_31) then the function will return fail,
    // and there will be no change to the configuration.
    //
    if(retval)
    {
        // If reconfig set, switch previous pins to non-I2C
        if(reconfig)
        {
            uint8_t old_mode = (i2c->currentPullup == I2C_PULLUP_EXT) ? INPUT : INPUT_PULLUP;
            switch(i2c->currentPins)
            {
            case I2C_PINS_16_17:
                pinMode(17,old_mode);
                pinMode(16,old_mode);
                break;
            case I2C_PINS_18_19:
                pinMode(18,old_mode);
                pinMode(19,old_mode);
                break;
            #if defined(__MKL26Z64__) // LC
            case I2C_PINS_22_23:
                pinMode(23,old_mode);
                pinMode(22,old_mode);
                break;
            #endif
            #if defined(__MK20DX256__) // 3.1/3.2
            case I2C_PINS_29_30:
                pinMode(30,old_mode);
                pinMode(29,old_mode);
                break;
            case I2C_PINS_26_31:
                pinMode(31,old_mode);
                pinMode(26,old_mode);
                break;
            #endif
            #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
            case I2C_PINS_3_4:
                pinMode(4,old_mode);
                pinMode(3,old_mode);
                break;
            case I2C_PINS_7_8:
                pinMode(8,old_mode);
                pinMode(7,old_mode);
                break;
            case I2C_PINS_33_34:
                pinMode(34,old_mode);
                pinMode(33,old_mode);
                break;
            case I2C_PINS_37_38:
                pinMode(38,old_mode);
                pinMode(37,old_mode);
                break;
            case I2C_PINS_47_48:
                pinMode(48,old_mode);
                pinMode(47,old_mode);
                break;
            #endif
            #if defined(__MK66FX1M0__) // 3.6
            case I2C_PINS_56_57:
                pinMode(56,old_mode);
                pinMode(57,old_mode);
                break;
            #endif
            default: break;
            }
        }

        // Configure new pins
        //
        if(bus == 0)
        {
            switch(pins)
            {
            case I2C_PINS_16_17:
                CORE_PIN17_CONFIG = pinConfigAlt2;
                CORE_PIN16_CONFIG = pinConfigAlt2;
                break;
            case I2C_PINS_18_19:
                CORE_PIN18_CONFIG = pinConfigAlt2;
                CORE_PIN19_CONFIG = pinConfigAlt2;
                break;
            #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
            case I2C_PINS_7_8:
                CORE_PIN8_CONFIG = pinConfigAlt7;
                CORE_PIN7_CONFIG = pinConfigAlt7;
                break;
            case I2C_PINS_33_34:
                CORE_PIN34_CONFIG = pinConfigAlt5;
                CORE_PIN33_CONFIG = pinConfigAlt5;
                break;
            case I2C_PINS_47_48:
                CORE_PIN48_CONFIG = pinConfigAlt2;
                CORE_PIN47_CONFIG = pinConfigAlt2;
                break;
            #endif
            default: break;
            }
        }
        #if I2C_BUS_NUM >= 2
            if(bus == 1)
            {
                switch(pins)
                {
                #if defined(__MKL26Z64__) // LC
                case I2C_PINS_22_23:
                    CORE_PIN23_CONFIG = pinConfigAlt2;
                    CORE_PIN22_CONFIG = pinConfigAlt2;
                    break;
                #endif
                #if defined(__MK20DX256__) // 3.1/3.2
                case I2C_PINS_26_31:
                    CORE_PIN31_CONFIG = pinConfigAlt6;
                    CORE_PIN26_CONFIG = pinConfigAlt6;
                    break;
                case I2C_PINS_29_30:
                    CORE_PIN30_CONFIG = pinConfigAlt2;
                    CORE_PIN29_CONFIG = pinConfigAlt2;
                    break;
                #endif
                #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
                case I2C_PINS_37_38:
                    CORE_PIN38_CONFIG = pinConfigAlt2;
                    CORE_PIN37_CONFIG = pinConfigAlt2;
                    break;
                #endif
                default: break;
                }
            }
        #endif
        #if I2C_BUS_NUM >= 3
            if(bus == 2)
            {
                switch(pins)
                {
                #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
                case I2C_PINS_3_4:
                    CORE_PIN4_CONFIG = pinConfigAlt5;
                    CORE_PIN3_CONFIG = pinConfigAlt5;
                    break;
                #endif
                default: break;
                }
            }
        #endif
        #if I2C_BUS_NUM >= 4
            if(bus == 3)
            {
                switch(pins)
                {
                #if defined(__MK66FX1M0__) // 3.6
                case I2C_PINS_56_57:
                    CORE_PIN56_CONFIG = pinConfigAlt2;
                    CORE_PIN57_CONFIG = pinConfigAlt2;
                    break;
                #endif
                default: break;
                }
            }
        #endif

        // Update settings
        i2c->currentPins = pins;
        i2c->currentPullup = pullup;
    }

    return retval;
}


// ------------------------------------------------------------------------------------------------------
// Acquire Bus - acquires bus in Master mode and escalates priority as needed, intended
//               for internal use only
// return: 1=success, 0=fail (cannot acquire bus)
// parameters:
//      timeout = timeout in microseconds
//      forceImm = flag to indicate if immediate mode is required
//
uint8_t i2c_t3::acquireBus_(struct i2cStruct* i2c, uint8_t bus, uint32_t timeout, uint8_t& forceImm)
{
    elapsedMicros deltaT;
    int irqPriority, currPriority;

    // update timeout
    timeout = (timeout == 0) ? i2c->defTimeout : timeout;

    // TODO may need to check bus busy before issuing START if multi-master

    // start timer, then take control of the bus
    deltaT = 0;
    if(*(i2c->C1) & I2C_C1_MST)
    {
        // we are already the bus master, so send a repeated start
        *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_RSTA | I2C_C1_TX;
    }
    else
    {
        while(timeout == 0 || deltaT < timeout)
        {
            // we are not currently the bus master, so check if bus ready
            if(!(*(i2c->S) & I2C_S_BUSY))
            {
                // become the bus master in transmit mode (send start)
                i2c->currentMode = I2C_MASTER;
                *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
                break;
            }
        }
        #if defined(I2C_AUTO_RETRY)
            // if not master and auto-retry set, then reset bus and try one last time
            if(!(*(i2c->C1) & I2C_C1_MST))
            {
                resetBus_(i2c,bus);
                if(!(*(i2c->S) & I2C_S_BUSY))
                {
                    // become the bus master in transmit mode (send start)
                    i2c->currentMode = I2C_MASTER;
                    *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
                }
            }
        #endif
        // check if not master
        if(!(*(i2c->C1) & I2C_C1_MST))
        {
            i2c->currentStatus = I2C_TIMEOUT; // bus not acquired, mark as timeout
            return 0;
        }
    }

    // For ISR operation, check if current routine has higher priority than I2C IRQ, and if so
    // either escalate priority of I2C IRQ or send I2C using immediate mode
    if(i2c->opMode == I2C_OP_MODE_ISR || i2c->opMode == I2C_OP_MODE_DMA)
    {
        currPriority = nvic_execution_priority();
        switch(bus)
        {
        case 0:  irqPriority = NVIC_GET_PRIORITY(IRQ_I2C0); break;
        #if defined(__MKL26Z64__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.1/3.2/3.5/3.6
        case 1:  irqPriority = NVIC_GET_PRIORITY(IRQ_I2C1); break;
        #endif
        #if defined(__MK64FX512__) || defined(__MK66FX1M0__) // 3.5/3.6
        case 2:  irqPriority = NVIC_GET_PRIORITY(IRQ_I2C2); break;
        #endif
        #if defined(__MK66FX1M0__) // 3.6
        case 3:  irqPriority = NVIC_GET_PRIORITY(IRQ_I2C3); break;
        #endif
        default: irqPriority = NVIC_GET_PRIORITY(IRQ_I2C0); break;
        }
        if(currPriority <= irqPriority)
        {
            if(currPriority < 16)
                forceImm = 1; // current priority cannot be surpassed, force Immediate mode
            else
            {
                switch(bus)
                {
                case 0:  NVIC_SET_PRIORITY(IRQ_I2C0, currPriority-16); break;
                #if defined(__MKL26Z64__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.1/3.2/3.5/3.6
                case 1:  NVIC_SET_PRIORITY(IRQ_I2C1, currPriority-16); break;
                #endif
                #if defined(__MK64FX512__) || defined(__MK66FX1M0__) // 3.5/3.6
                case 2:  NVIC_SET_PRIORITY(IRQ_I2C2, currPriority-16); break;
                #endif
                #if defined(__MK66FX1M0__) // 3.6
                case 3:  NVIC_SET_PRIORITY(IRQ_I2C3, currPriority-16); break;
                #endif
                default: NVIC_SET_PRIORITY(IRQ_I2C0, currPriority-16); break;
                }
            }
        }
    }
    return 1;
}


// ------------------------------------------------------------------------------------------------------
// Reset Bus - toggles SCL until SDA line is released (9 clocks max).  This is used to correct
//             a hung bus in which a Slave device missed some clocks and remains stuck outputting
//             a low signal on SDA (thereby preventing START/STOP signaling).
// return: none
//
void i2c_t3::resetBus_(struct i2cStruct* i2c, uint8_t bus)
{
    uint8_t scl=0, sda=0, count=0;

    switch(i2c->currentPins)
    {
    case I2C_PINS_16_17: sda = 17; scl = 16; break;
    case I2C_PINS_18_19: sda = 18; scl = 19; break;
    #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
    case I2C_PINS_7_8:   sda =  8; scl =  7; break;
    case I2C_PINS_33_34: sda = 34; scl = 33; break;
    case I2C_PINS_47_48: sda = 48; scl = 47; break;
    #endif
    #if defined(__MKL26Z64__) // LC
    case I2C_PINS_22_23: sda = 23; scl = 22; break;
    #endif
    #if defined(__MK20DX256__) // 3.1/3.2
    case I2C_PINS_29_30: sda = 30; scl = 29; break;
    case I2C_PINS_26_31: sda = 31; scl = 26; break;
    #endif
    #if defined(__MK64FX512__) || defined(__MK66FX1M0__)  // 3.5/3.6
    case I2C_PINS_37_38: sda = 38; scl = 37; break;
    case I2C_PINS_3_4:   sda =  4; scl =  3; break;
    #endif
    #if defined(__MK66FX1M0__) // 3.6
    case I2C_PINS_56_57: sda = 56; scl = 57; break;
    #endif
    }
    if(sda && scl)
    {
        // change pin mux to digital I/O
        pinMode(sda,((i2c->currentPullup == I2C_PULLUP_EXT) ? INPUT : INPUT_PULLUP));
        digitalWrite(scl,HIGH);
        pinMode(scl,OUTPUT);

        while(digitalRead(sda) == 0 && count++ < 10)
        {
            digitalWrite(scl,LOW);
            delayMicroseconds(5);       // 10us period == 100kHz
            digitalWrite(scl,HIGH);
            delayMicroseconds(5);
        }

        // reset status
        i2c->currentStatus = I2C_WAITING; // reset status

        // reconfigure pins for I2C
        pinConfigure_(i2c, bus, i2c->currentPins, i2c->currentPullup, 0);
    }
}


// ------------------------------------------------------------------------------------------------------
// Setup Master Transmit - initialize Tx buffer for transmit to slave at address
// return: none
// parameters:
//      address = target 7bit slave address
//
void i2c_t3::beginTransmission(uint8_t address)
{
    i2c->txBuffer[0] = (address << 1); // store target addr
    i2c->txBufferLength = 1;
    clearWriteError(); // clear any previous write error
    i2c->currentStatus = I2C_WAITING; // reset status
}


// ------------------------------------------------------------------------------------------------------
// Master Transmit - blocking routine with timeout, transmits Tx buffer to slave. i2c_stop parameter can be used
//                   to indicate if command should end with a STOP(I2C_STOP) or not (I2C_NOSTOP).
// return: 0=success, 1=data too long, 2=recv addr NACK, 3=recv data NACK, 4=other error
// parameters:
//      i2c_stop = I2C_NOSTOP, I2C_STOP
//      timeout = timeout in microseconds
//
uint8_t i2c_t3::endTransmission(struct i2cStruct* i2c, uint8_t bus, i2c_stop sendStop, uint32_t timeout)
{
    sendTransmission_(i2c, bus, sendStop, timeout);

    // wait for completion or timeout
    finish_(i2c, bus, timeout);

    return getError();
}


// ------------------------------------------------------------------------------------------------------
// Send Master Transmit - non-blocking routine, starts transmit of Tx buffer to slave. i2c_stop parameter can be
//                        used to indicate if command should end with a STOP (I2C_STOP) or not (I2C_NOSTOP). Use
//                        done() or finish() to determine completion and status() to determine success/fail.
// return: none
// parameters:
//      i2c_stop = I2C_NOSTOP, I2C_STOP
//      timeout = timeout in microseconds (only used for Immediate operation)
//
void i2c_t3::sendTransmission_(struct i2cStruct* i2c, uint8_t bus, i2c_stop sendStop, uint32_t timeout)
{
    uint8_t status, forceImm=0;
    size_t idx;

    // exit immediately if sending 0 bytes
    if(i2c->txBufferLength == 0) return;

    // update timeout
    timeout = (timeout == 0) ? i2c->defTimeout : timeout;

    // clear the status flags
    #if defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.5/3.6
        *(i2c->FLT) |= I2C_FLT_STOPF | I2C_FLT_STARTF;  // clear STOP/START intr
        *(i2c->FLT) &= ~I2C_FLT_SSIE;                   // disable STOP/START intr (not used in Master mode)
    #endif
    *(i2c->S) = I2C_S_IICIF | I2C_S_ARBL; // clear intr, arbl

    // try to take control of the bus
    if(!acquireBus_(i2c, bus, timeout, forceImm)) return;

    //
    // Immediate mode - blocking
    //
    if(i2c->opMode == I2C_OP_MODE_IMM || forceImm)
    {
        elapsedMicros deltaT;
        i2c->currentStatus = I2C_SENDING;
        i2c->currentStop = sendStop;

        for(idx=0; idx < i2c->txBufferLength && (timeout == 0 || deltaT < timeout); idx++)
        {
            // send data, wait for done
            *(i2c->D) = i2c->txBuffer[idx];
            i2c_wait_(i2c);
            status = *(i2c->S);

            // check arbitration
            if(status & I2C_S_ARBL)
            {
                i2c->currentStatus = I2C_ARB_LOST;
                *(i2c->S) = I2C_S_ARBL; // clear arbl flag
                // TODO: this is clearly not right, after ARBL it should drop into IMM slave mode if IAAS=1
                //       Right now Rx message would be ignored regardless of IAAS
                *(i2c->C1) = I2C_C1_IICEN; // change to Rx mode, intr disabled (does this send STOP if ARBL flagged?)
                return;
            }
            // check if slave ACK'd
            else if(status & I2C_S_RXAK)
            {
                if(idx == 0)
                    i2c->currentStatus = I2C_ADDR_NAK; // NAK on Addr
                else
                    i2c->currentStatus = I2C_DATA_NAK; // NAK on Data
                *(i2c->C1) = I2C_C1_IICEN; // send STOP, change to Rx mode, intr disabled
                return;
            }
        }

        // Set final status
        if(idx < i2c->txBufferLength)
            i2c->currentStatus = I2C_TIMEOUT; // Tx incomplete, mark as timeout
        else
            i2c->currentStatus = I2C_WAITING; // Tx complete, change to waiting state

        // send STOP if configured
        if(i2c->currentStop == I2C_STOP)
            *(i2c->C1) = I2C_C1_IICEN; // send STOP, change to Rx mode, intr disabled
        else
            *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX; // no STOP, stay in Tx mode, intr disabled
    }
    //
    // ISR/DMA mode - non-blocking
    //
    else if(i2c->opMode == I2C_OP_MODE_ISR || i2c->opMode == I2C_OP_MODE_DMA)
    {
        // send target addr and enable interrupts
        i2c->currentStatus = I2C_SENDING;
        i2c->currentStop = sendStop;
        i2c->txBufferIndex = 0;
        if(i2c->opMode == I2C_OP_MODE_DMA && i2c->txBufferLength >= 5) // limit transfers less than 5 bytes to ISR method
        {
            // init DMA, let the hack begin
            i2c->activeDMA = I2C_DMA_ADDR;
            i2c->DMA->sourceBuffer(&i2c->txBuffer[2],i2c->txBufferLength-3); // DMA sends all except first/second/last bytes
            i2c->DMA->destination(*(i2c->D));
        }
        // start ISR
        *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST | I2C_C1_TX; // enable intr
        *(i2c->D) = i2c->txBuffer[0]; // writing first data byte will start ISR
    }
}


// ------------------------------------------------------------------------------------------------------
// Master Receive - blocking routine with timeout, requests length bytes from slave at address. Receive data will
//                  be placed in the Rx buffer. i2c_stop parameter can be used to indicate if command should end
//                  with a STOP (I2C_STOP) or not (I2C_NOSTOP).
// return: #bytes received = success, 0=fail (0 length request, NAK, timeout, or bus error)
// parameters:
//      address = target 7bit slave address
//      length = number of bytes requested
//      i2c_stop = I2C_NOSTOP, I2C_STOP
//      timeout = timeout in microseconds
//
size_t i2c_t3::requestFrom_(struct i2cStruct* i2c, uint8_t bus, uint8_t addr, size_t len, i2c_stop sendStop, uint32_t timeout)
{
    // exit immediately if request for 0 bytes
    if(len == 0) return 0;

    sendRequest_(i2c, bus, addr, len, sendStop, timeout);

    // wait for completion or timeout
    if(finish_(i2c, bus, timeout))
        return i2c->rxBufferLength;
    else
        return 0; // NAK, timeout or bus error
}


// ------------------------------------------------------------------------------------------------------
// Start Master Receive - non-blocking routine, starts request for length bytes from slave at address. Receive
//                        data will be placed in the Rx buffer. i2c_stop parameter can be used to indicate if
//                        command should end with a STOP (I2C_STOP) or not (I2C_NOSTOP). Use done() or finish()
//                        to determine completion and status() to determine success/fail.
// return: none
// parameters:
//      address = target 7bit slave address
//      length = number of bytes requested
//      i2c_stop = I2C_NOSTOP, I2C_STOP
//      timeout = timeout in microseconds (only used for Immediate operation)
//
void i2c_t3::sendRequest_(struct i2cStruct* i2c, uint8_t bus, uint8_t addr, size_t len, i2c_stop sendStop, uint32_t timeout)
{
    uint8_t status, data, chkTimeout=0, forceImm=0;

    // exit immediately if request for 0 bytes or request too large
    if(len == 0) return;
    if(len > I2C_RX_BUFFER_LENGTH) { i2c->currentStatus=I2C_BUF_OVF; return; }

    i2c->reqCount = len; // store request length
    i2c->rxBufferIndex = 0; // reset buffer
    i2c->rxBufferLength = 0;
    timeout = (timeout == 0) ? i2c->defTimeout : timeout;

    // clear the status flags
    #if defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.5/3.6
        *(i2c->FLT) |= I2C_FLT_STOPF | I2C_FLT_STARTF;  // clear STOP/START intr
        *(i2c->FLT) &= ~I2C_FLT_SSIE;                   // disable STOP/START intr (not used in Master mode)
    #endif
    *(i2c->S) = I2C_S_IICIF | I2C_S_ARBL; // clear intr, arbl

    // try to take control of the bus
    if(!acquireBus_(i2c, bus, timeout, forceImm)) return;

    //
    // Immediate mode - blocking
    //
    if(i2c->opMode == I2C_OP_MODE_IMM || forceImm)
    {
        elapsedMicros deltaT;
        i2c->currentStatus = I2C_SEND_ADDR;
        i2c->currentStop = sendStop;

        // Send target address
        *(i2c->D) = (addr << 1) | 1; // address + READ
        i2c_wait_(i2c);
        status = *(i2c->S);

        // check arbitration
        if(status & I2C_S_ARBL)
        {
            i2c->currentStatus = I2C_ARB_LOST;
            *(i2c->S) = I2C_S_ARBL; // clear arbl flag
            // TODO: this is clearly not right, after ARBL it should drop into IMM slave mode if IAAS=1
            //       Right now Rx message would be ignored regardless of IAAS
            *(i2c->C1) = I2C_C1_IICEN; // change to Rx mode, intr disabled (does this send STOP if ARBL flagged?)
            return;
        }
        // check if slave ACK'd
        else if(status & I2C_S_RXAK)
        {
            i2c->currentStatus = I2C_ADDR_NAK; // NAK on Addr
            *(i2c->C1) = I2C_C1_IICEN; // send STOP, change to Rx mode, intr disabled
            return;
        }
        else
        {
            // Slave addr ACK, change to Rx mode
            i2c->currentStatus = I2C_RECEIVING;
            if(i2c->reqCount == 1)
                *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TXAK; // no STOP, Rx, NAK on recv
            else
                *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST; // no STOP, change to Rx
            data = *(i2c->D); // dummy read

            // Master receive loop
            while(i2c->rxBufferLength < i2c->reqCount && i2c->currentStatus == I2C_RECEIVING)
            {
                i2c_wait_(i2c);
                chkTimeout = (timeout != 0 && deltaT >= timeout);
                // check if 2nd to last byte or timeout
                if((i2c->rxBufferLength+2) == i2c->reqCount || (chkTimeout && !i2c->timeoutRxNAK))
                {
                    *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TXAK; // no STOP, Rx, NAK on recv
                }
                // if last byte or timeout send STOP
                if((i2c->rxBufferLength+1) >= i2c->reqCount || (chkTimeout && i2c->timeoutRxNAK))
                {
                    i2c->timeoutRxNAK = 0; // clear flag
                    // change to Tx mode
                    *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
                    // grab last data
                    data = *(i2c->D);
                    i2c->rxBuffer[i2c->rxBufferLength++] = data;
                    if(chkTimeout)
                        i2c->currentStatus = I2C_TIMEOUT; // Rx incomplete, mark as timeout
                    else
                        i2c->currentStatus = I2C_WAITING; // Rx complete, change to waiting state
                    if(i2c->currentStop == I2C_STOP) // NAK then STOP
                    {
                        delayMicroseconds(1); // empirical patch, lets things settle before issuing STOP
                        *(i2c->C1) = I2C_C1_IICEN; // send STOP, change to Rx mode, intr disabled
                    }
                    // else NAK no STOP
                }
                else
                {
                    // grab next data, not last byte, will ACK
                    data = *(i2c->D);
                    i2c->rxBuffer[i2c->rxBufferLength++] = data;
                }
                if(chkTimeout) i2c->timeoutRxNAK = 1; // set flag to indicate NAK sent
            }
        }
    }
    //
    // ISR/DMA mode - non-blocking
    //
    else if(i2c->opMode == I2C_OP_MODE_ISR || i2c->opMode == I2C_OP_MODE_DMA)
    {
        // send 1st data and enable interrupts
        i2c->currentStatus = I2C_SEND_ADDR;
        i2c->currentStop = sendStop;
        if(i2c->opMode == I2C_OP_MODE_DMA && i2c->reqCount >= 5) // limit transfers less than 5 bytes to ISR method
        {
            // init DMA, let the hack begin
            i2c->activeDMA = I2C_DMA_ADDR;
            i2c->DMA->source(*(i2c->D));
            i2c->DMA->destinationBuffer(&i2c->rxBuffer[0],i2c->reqCount-1); // DMA gets all except last byte
        }
        // start ISR
        *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST | I2C_C1_TX; // enable intr
        *(i2c->D) = (addr << 1) | 1; // address + READ
    }
}


// ------------------------------------------------------------------------------------------------------
// Get Wire Error - returns "Wire" error code from a failed Tx/Rx command
// return: 0=success, 1=data too long, 2=recv addr NACK, 3=recv data NACK, 4=other error (timeout, arb lost)
//
uint8_t i2c_t3::getError(void)
{
    // convert status to Arduino return values (give these a higher priority than buf overflow error)
    switch(i2c->currentStatus)
    {
    case I2C_BUF_OVF:  return 1;
    case I2C_ADDR_NAK: return 2;
    case I2C_DATA_NAK: return 3;
    case I2C_ARB_LOST: return 4;
    case I2C_TIMEOUT:  return 4;
    default: break;
    }
    if(getWriteError()) return 1; // if write_error was set then flag as buffer overflow
    return 0; // no errors
}


// ------------------------------------------------------------------------------------------------------
// Done Check - returns simple complete/not-complete value to indicate I2C status
// return: 1=Tx/Rx complete (with or without errors), 0=still running
//
uint8_t i2c_t3::done_(struct i2cStruct* i2c)
{
    return (i2c->currentStatus==I2C_WAITING ||
            i2c->currentStatus==I2C_ADDR_NAK ||
            i2c->currentStatus==I2C_DATA_NAK ||
            i2c->currentStatus==I2C_ARB_LOST ||
            i2c->currentStatus==I2C_TIMEOUT ||
            i2c->currentStatus==I2C_BUF_OVF);
}


// ------------------------------------------------------------------------------------------------------
// Finish - blocking routine with timeout, loops until Tx/Rx is complete or timeout occurs
// return: 1=success (Tx or Rx completed, no error), 0=fail (NAK, timeout or Arb Lost)
// parameters:
//      timeout = timeout in microseconds
//
uint8_t i2c_t3::finish_(struct i2cStruct* i2c, uint8_t bus, uint32_t timeout)
{
    elapsedMicros deltaT;

    // update timeout
    timeout = (timeout == 0) ? i2c->defTimeout : timeout;

    // wait for completion or timeout
    deltaT = 0;
    while(!done_(i2c) && (timeout == 0 || deltaT < timeout));

    // DMA mode and timeout
    if(timeout != 0 && deltaT >= timeout && i2c->opMode == I2C_OP_MODE_DMA && i2c->activeDMA != I2C_DMA_OFF)
    {
        // If DMA mode times out, then wait for transfer to end then mark it as timeout.
        // This is done this way because abruptly ending the DMA seems to cause
        // the I2C_S_BUSY flag to get stuck, and I cannot find a reliable way to clear it.
        while(!done_(i2c));
        i2c->currentStatus = I2C_TIMEOUT;
    }

    // check exit status, if still Tx/Rx then timeout occurred
    if(i2c->currentStatus == I2C_SENDING ||
       i2c->currentStatus == I2C_SEND_ADDR ||
       i2c->currentStatus == I2C_RECEIVING)
        i2c->currentStatus = I2C_TIMEOUT; // set to timeout state

    // delay to allow bus to settle (eg. allow STOP to complete and be recognized,
    //                               not just on our side, but on slave side also)
    delayMicroseconds(4);
    if(i2c->currentStatus == I2C_WAITING) return 1;
    return 0;
}


// ------------------------------------------------------------------------------------------------------
// Write - write data to Tx buffer
// return: #bytes written = success, 0=fail
// parameters:
//      data = data byte
//
size_t i2c_t3::write(uint8_t data)
{
    if(i2c->txBufferLength < I2C_TX_BUFFER_LENGTH)
    {
        i2c->txBuffer[i2c->txBufferLength++] = data;
        return 1;
    }
    setWriteError();
    return 0;
}


// ------------------------------------------------------------------------------------------------------
// Write Array - write length number of bytes from data array to Tx buffer
// return: #bytes written = success, 0=fail
// parameters:
//      data = pointer to uint8_t array of data
//      length = number of bytes to write
//
size_t i2c_t3::write(const uint8_t* data, size_t quantity)
{
    if(i2c->txBufferLength < I2C_TX_BUFFER_LENGTH)
    {
        size_t avail = I2C_TX_BUFFER_LENGTH - i2c->txBufferLength;
        uint8_t* dest = i2c->txBuffer + i2c->txBufferLength;

        if(quantity > avail)
        {
            quantity = avail; // truncate to space avail if needed
            setWriteError();
        }
        for(size_t count=quantity; count; count--)
            *dest++ = *data++;
        i2c->txBufferLength += quantity;
        return quantity;
    }
    setWriteError();
    return 0;
}


// ------------------------------------------------------------------------------------------------------
// Read - returns next data byte (signed int) from Rx buffer
// return: data, -1 if buffer empty
//
int i2c_t3::read_(struct i2cStruct* i2c)
{
    if(i2c->rxBufferIndex >= i2c->rxBufferLength) return -1;
    return i2c->rxBuffer[i2c->rxBufferIndex++];
}


// ------------------------------------------------------------------------------------------------------
// Peek - returns next data byte (signed int) from Rx buffer without removing it from Rx buffer
// return: data, -1 if buffer empty
//
int i2c_t3::peek_(struct i2cStruct* i2c)
{
    if(i2c->rxBufferIndex >= i2c->rxBufferLength) return -1;
    return i2c->rxBuffer[i2c->rxBufferIndex];
}


// ------------------------------------------------------------------------------------------------------
// Read Byte - returns next data byte (uint8_t) from Rx buffer
// return: data, 0 if buffer empty
//
uint8_t i2c_t3::readByte_(struct i2cStruct* i2c)
{
    if(i2c->rxBufferIndex >= i2c->rxBufferLength) return 0;
    return i2c->rxBuffer[i2c->rxBufferIndex++];
}


// ------------------------------------------------------------------------------------------------------
// Peek Byte - returns next data byte (uint8_t) from Rx buffer without removing it from Rx buffer
// return: data, 0 if buffer empty
//
uint8_t i2c_t3::peekByte_(struct i2cStruct* i2c)
{
    if(i2c->rxBufferIndex >= i2c->rxBufferLength) return 0;
    return i2c->rxBuffer[i2c->rxBufferIndex];
}


// ======================================================================================================
// ------------------------------------------------------------------------------------------------------
// I2C Interrupt Service Routine
// ------------------------------------------------------------------------------------------------------
// ======================================================================================================


void i2c0_isr(void) // I2C0 ISR
{
    I2C0_INTR_FLAG_ON;
    i2c_isr_handler(&(i2c_t3::i2cData[0]),0);
    I2C0_INTR_FLAG_OFF;
}
#if I2C_BUS_NUM >= 2
    void i2c1_isr(void) // I2C1 ISR
    {
        I2C1_INTR_FLAG_ON;
        i2c_isr_handler(&(i2c_t3::i2cData[1]),1);
        I2C1_INTR_FLAG_OFF;
    }
#endif
#if I2C_BUS_NUM >= 3
    void i2c2_isr(void) // I2C2 ISR
    {
        I2C2_INTR_FLAG_ON;
        i2c_isr_handler(&(i2c_t3::i2cData[2]),2);
        I2C2_INTR_FLAG_OFF;
    }
#endif
#if I2C_BUS_NUM >= 4
    void i2c3_isr(void) // I2C3 ISR
    {
        I2C3_INTR_FLAG_ON;
        i2c_isr_handler(&(i2c_t3::i2cData[3]),3);
        I2C3_INTR_FLAG_OFF;
    }
#endif

//
// I2C ISR base handler
//
void i2c_isr_handler(struct i2cStruct* i2c, uint8_t bus)
{
    uint8_t status, c1, data;

    status = *(i2c->S);
    c1 = *(i2c->C1);
    #if defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.5/3.6
        uint8_t flt = *(i2c->FLT);  // store flags
    #endif

    if(c1 & I2C_C1_MST)
    {
        //
        // Master Mode
        //
        if(c1 & I2C_C1_TX)
        {
            if(i2c->activeDMA == I2C_DMA_BULK || i2c->activeDMA == I2C_DMA_LAST)
            {
                if(i2c->DMA->complete() && i2c->activeDMA == I2C_DMA_BULK)
                {
                    // clear DMA interrupt, final byte should trigger another ISR
                    i2c->DMA->clearInterrupt();
                    *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST | I2C_C1_TX; // intr en, Tx mode, DMA disabled
                    // DMA says complete at the beginning of its last byte, need to
                    // wait until end of its last byte to re-engage ISR
                    i2c->activeDMA = I2C_DMA_LAST;
                }
                else if(i2c->activeDMA == I2C_DMA_LAST)
                {
                    // wait for TCF
                    while(!(*(i2c->S) & I2C_S_TCF));
                    // clear DMA, only do this after TCF
                    i2c->DMA->clearComplete();
                    // re-engage ISR for last byte
                    i2c->activeDMA = I2C_DMA_OFF;
                    i2c->txBufferIndex = i2c->txBufferLength-1;
                    *(i2c->D) = i2c->txBuffer[i2c->txBufferIndex];
                }
                else if(i2c->DMA->error())
                {
                    i2c->DMA->clearInterrupt();
                    i2c->DMA->clearError();
                    i2c->activeDMA = I2C_DMA_OFF;
                    // check arbitration
                    if(status & I2C_S_ARBL)
                    {
                        // Arbitration Lost
                        i2c->currentStatus = I2C_ARB_LOST;
                        *(i2c->S) = I2C_S_ARBL | I2C_S_IICIF; // clear arbl flag and intr
                        *(i2c->C1) = I2C_C1_IICEN; // change to Rx mode, intr disabled (does this send STOP if ARBL flagged?), DMA disabled
                        i2c->txBufferIndex = 0; // reset Tx buffer index to prepare for resend
                        return; // TODO does this need to check IAAS and drop to Slave Rx? if so set Rx + dummy read. not sure if this would work for DMA
                    }
                }
                *(i2c->S) = I2C_S_IICIF; // clear intr
                return;
            } // end DMA Tx
            else
            {
                // Continue Master Transmit
                // check if Master Tx or Rx
                if(i2c->currentStatus == I2C_SENDING)
                {
                    // check arbitration
                    if(status & I2C_S_ARBL)
                    {
                        // Arbitration Lost
                        i2c->activeDMA = I2C_DMA_OFF; // clear pending DMA (if happens on address byte)
                        i2c->currentStatus = I2C_ARB_LOST;
                        *(i2c->S) = I2C_S_ARBL | I2C_S_IICIF; // clear arbl flag and intr
                        *(i2c->C1) = I2C_C1_IICEN; // change to Rx mode, intr disabled (does this send STOP if ARBL flagged?)
                        i2c->txBufferIndex = 0; // reset Tx buffer index to prepare for resend
                        return; // does this need to check IAAS and drop to Slave Rx? if so set Rx + dummy read.
                    }
                    // check if slave ACK'd
                    else if(status & I2C_S_RXAK)
                    {
                        i2c->activeDMA = I2C_DMA_OFF; // clear pending DMA (if happens on address byte)
                        if(i2c->txBufferIndex == 0)
                            i2c->currentStatus = I2C_ADDR_NAK; // NAK on Addr
                        else
                            i2c->currentStatus = I2C_DATA_NAK; // NAK on Data
                        // send STOP, change to Rx mode, intr disabled
                        // note: Slave NAK is an error, so send STOP regardless of setting
                        *(i2c->C1) = I2C_C1_IICEN;
                    }
                    else
                    {
                        // check if last byte transmitted
                        if(++i2c->txBufferIndex >= i2c->txBufferLength)
                        {
                            // Tx complete, change to waiting state
                            i2c->currentStatus = I2C_WAITING;
                            // send STOP if configured
                            if(i2c->currentStop == I2C_STOP)
                                *(i2c->C1) = I2C_C1_IICEN; // send STOP, change to Rx mode, intr disabled
                            else
                                *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX; // no STOP, stay in Tx mode, intr disabled
                        }
                        else if(i2c->activeDMA == I2C_DMA_ADDR)
                        {
                            // Start DMA
                            i2c->activeDMA = I2C_DMA_BULK;
                            *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST | I2C_C1_TX | I2C_C1_DMAEN; // intr en, Tx mode, DMA en
                            i2c->DMA->enable();
                            *(i2c->D) = i2c->txBuffer[1]; // DMA will start on next request
                        }
                        else
                        {
                            // ISR transmit next byte
                            *(i2c->D) = i2c->txBuffer[i2c->txBufferIndex];
                        }
                    }
                    *(i2c->S) = I2C_S_IICIF; // clear intr
                    return;
                }
                else if(i2c->currentStatus == I2C_SEND_ADDR)
                {
                    // Master Receive, addr sent
                    if(status & I2C_S_ARBL)
                    {
                        // Arbitration Lost
                        i2c->currentStatus = I2C_ARB_LOST;
                        *(i2c->S) = I2C_S_ARBL | I2C_S_IICIF; // clear arbl flag and intr
                        *(i2c->C1) = I2C_C1_IICEN; // change to Rx mode, intr disabled (does this send STOP if ARBL flagged?)
                        return; // TODO does this need to check IAAS and drop to Slave Rx? if so set Rx + dummy read. not sure if this would work for DMA
                    }
                    else if(status & I2C_S_RXAK)
                    {
                        // Slave addr NAK
                        i2c->currentStatus = I2C_ADDR_NAK; // NAK on Addr
                        // send STOP, change to Rx mode, intr disabled
                        *(i2c->C1) = I2C_C1_IICEN;
                    }
                    else if(i2c->activeDMA == I2C_DMA_ADDR)
                    {
                        // Start DMA
                        i2c->activeDMA = I2C_DMA_BULK;
                        *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST | I2C_C1_DMAEN; // intr en, no STOP, change to Rx, DMA en
                        i2c->DMA->enable();
                        data = *(i2c->D); // dummy read
                    }
                    else
                    {
                        // Slave addr ACK, change to Rx mode
                        i2c->currentStatus = I2C_RECEIVING;
                        if(i2c->reqCount == 1)
                            *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST | I2C_C1_TXAK; // no STOP, Rx, NAK on recv
                        else
                            *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST; // no STOP, change to Rx
                        data = *(i2c->D); // dummy read
                    }
                    *(i2c->S) = I2C_S_IICIF; // clear intr
                    return;
                }
                else if(i2c->currentStatus == I2C_TIMEOUT)
                {
                    // send STOP if configured
                    if(i2c->currentStop == I2C_STOP)
                    {
                        // send STOP, change to Rx mode, intr disabled
                        *(i2c->C1) = I2C_C1_IICEN;
                    }
                    else
                    {
                        // no STOP, stay in Tx mode, intr disabled
                        *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
                    }
                    *(i2c->S) = I2C_S_IICIF; // clear intr
                    return;
                }
                else
                {
                    // Should not be in Tx mode if not sending
                    // send STOP, change to Rx mode, intr disabled
                    *(i2c->C1) = I2C_C1_IICEN;
                    *(i2c->S) = I2C_S_IICIF; // clear intr
                    return;
                }
            } // end ISR Tx
        }
        else
        {
            // Continue Master Receive
            //
            if(i2c->activeDMA == I2C_DMA_BULK || i2c->activeDMA == I2C_DMA_LAST)
            {
                if(i2c->DMA->complete() && i2c->activeDMA == I2C_DMA_BULK) // 2nd to last byte
                {
                    // clear DMA interrupt, final byte should trigger another ISR
                    i2c->DMA->clearInterrupt();
                    *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST | I2C_C1_TXAK; // intr en, Rx mode, DMA disabled, NAK on recv
                    i2c->activeDMA = I2C_DMA_LAST;
                }
                else if(i2c->activeDMA == I2C_DMA_LAST) // last byte
                {
                    // clear DMA
                    i2c->DMA->clearComplete();
                    i2c->activeDMA = I2C_DMA_OFF;
                    if(i2c->currentStatus != I2C_TIMEOUT)
                        i2c->currentStatus = I2C_WAITING; // Rx complete, change to waiting state
                    // change to Tx mode
                    *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
                    // grab last data
                    i2c->rxBufferLength = i2c->reqCount-1;
                    i2c->rxBuffer[i2c->rxBufferLength++] = *(i2c->D);
                    if(i2c->currentStop == I2C_STOP) // NAK then STOP
                    {
                        delayMicroseconds(1); // empirical patch, lets things settle before issuing STOP
                        *(i2c->C1) = I2C_C1_IICEN; // send STOP, change to Rx mode, intr disabled
                    }
                    // else NAK no STOP
                }
                else if(i2c->DMA->error()) // not sure what would cause this...
                {
                    i2c->DMA->clearError();
                    i2c->DMA->clearInterrupt();
                    i2c->activeDMA = I2C_DMA_OFF;
                    i2c->currentStatus = I2C_WAITING;
                    *(i2c->C1) = I2C_C1_IICEN; // change to Rx mode, intr disabled, DMA disabled
                }
                *(i2c->S) = I2C_S_IICIF; // clear intr
                return;
            }
            else
            {
                // check if 2nd to last byte or timeout
                if((i2c->rxBufferLength+2) == i2c->reqCount || (i2c->currentStatus == I2C_TIMEOUT && !i2c->timeoutRxNAK))
                {
                    *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_MST | I2C_C1_TXAK; // no STOP, Rx, NAK on recv
                }
                // if last byte or timeout send STOP
                if((i2c->rxBufferLength+1) >= i2c->reqCount || (i2c->currentStatus == I2C_TIMEOUT && i2c->timeoutRxNAK))
                {
                    i2c->timeoutRxNAK = 0; // clear flag
                    if(i2c->currentStatus != I2C_TIMEOUT)
                        i2c->currentStatus = I2C_WAITING; // Rx complete, change to waiting state
                    // change to Tx mode
                    *(i2c->C1) = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
                    // grab last data
                    i2c->rxBuffer[i2c->rxBufferLength++] = *(i2c->D);
                    if(i2c->currentStop == I2C_STOP) // NAK then STOP
                    {
                        delayMicroseconds(1); // empirical patch, lets things settle before issuing STOP
                        *(i2c->C1) = I2C_C1_IICEN; // send STOP, change to Rx mode, intr disabled
                    }
                    // else NAK no STOP
                }
                else
                {
                    // grab next data, not last byte, will ACK
                    i2c->rxBuffer[i2c->rxBufferLength++] = *(i2c->D);
                }
                if(i2c->currentStatus == I2C_TIMEOUT && !i2c->timeoutRxNAK)
                    i2c->timeoutRxNAK = 1; // set flag to indicate NAK sent
                *(i2c->S) = I2C_S_IICIF; // clear intr
                return;
            }
        }
    }
    else
    {
        //
        // Slave Mode
        //
        if(status & I2C_S_ARBL)
        {
            // Arbitration Lost
            *(i2c->S) = I2C_S_ARBL; // clear arbl flag
            if(!(status & I2C_S_IAAS))
            {
                #if defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.5/3.6
                    *(i2c->FLT) = flt;   // clear STOP/START intr
                #endif
                *(i2c->S) = I2C_S_IICIF; // clear intr
                return;
            }
        }
        if(status & I2C_S_IAAS)
        {
            // If in Slave Rx already, then RepSTART occured, run callback
            if(i2c->currentStatus == I2C_SLAVE_RX && i2c->user_onReceive != nullptr)
            {
                i2c->rxBufferIndex = 0;
                i2c->user_onReceive(i2c->rxBufferLength);
            }
            // Is Addressed As Slave
            if(status & I2C_S_SRW)
            {
                // Addressed Slave Transmit
                //
                i2c->currentStatus = I2C_SLAVE_TX;
                i2c->txBufferLength = 0;
                *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_TX;
                i2c->rxAddr = (*(i2c->D) >> 1); // read to get target addr
                if(i2c->user_onRequest != nullptr)
                    i2c->user_onRequest(); // load Tx buffer with data
                if(i2c->txBufferLength == 0)
                    i2c->txBuffer[0] = 0; // send 0's if buffer empty
                *(i2c->D) = i2c->txBuffer[0]; // send first data
                i2c->txBufferIndex = 1;
            }
            else
            {
                // Addressed Slave Receive
                //
                // setup SDA-rising ISR - required for STOP detection in Slave Rx mode for 3.0/3.1/3.2
                #if defined(__MK20DX128__) || defined(__MK20DX256__) // 3.0/3.1/3.2
                    i2c->irqCount = 0;
                    if(i2c->currentPins == I2C_PINS_18_19)
                        attachInterrupt(18, i2c_t3::sda0_rising_isr, RISING);
                    else if(i2c->currentPins == I2C_PINS_16_17)
                        attachInterrupt(17, i2c_t3::sda0_rising_isr, RISING);
                    #if I2C_BUS_NUM >= 2
                    else if(i2c->currentPins == I2C_PINS_29_30)
                        attachInterrupt(30, i2c_t3::sda1_rising_isr, RISING);
                    else if(i2c->currentPins == I2C_PINS_26_31)
                        attachInterrupt(31, i2c_t3::sda1_rising_isr, RISING);
                    #endif
                #elif defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
                    *(i2c->FLT) |= I2C_FLT_SSIE; // enable START/STOP intr for LC/3.5/3.6
                #endif
                i2c->currentStatus = I2C_SLAVE_RX;
                i2c->rxBufferLength = 0;
                *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE;
                i2c->rxAddr = (*(i2c->D) >> 1); // read to get target addr
            }
            #if defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.5/3.6
                *(i2c->FLT) = flt;   // clear STOP/START intr
            #endif
            *(i2c->S) = I2C_S_IICIF; // clear intr
            return;
        }
        if(c1 & I2C_C1_TX)
        {
            // Continue Slave Transmit
            if((status & I2C_S_RXAK) == 0)
            {
                // Master ACK'd previous byte
                if(i2c->txBufferIndex < i2c->txBufferLength)
                    data = i2c->txBuffer[i2c->txBufferIndex++];
                else
                    data = 0; // send 0's if buffer empty
                *(i2c->D) = data;
                *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_TX;
            }
            else
            {
                // Master did not ACK previous byte
                *(i2c->C1) = I2C_C1_IICEN | I2C_C1_IICIE; // switch to Rx mode
                data = *(i2c->D); // dummy read
                i2c->currentStatus = I2C_WAITING;
            }
        }
        else if(i2c->currentStatus == I2C_SLAVE_RX)
        {
            #if defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.5/3.6
                if(flt & I2C_FLT_STOPF) // STOP detected, run callback
                {
                    // LC (MKL26) appears to have the same I2C_FLT reg definition as 3.6 (K66)
                    // There is both STOPF and STARTF and they are both enabled via SSIE, and they must both
                    // be cleared in order to work
                    i2c->currentStatus = I2C_WAITING;
                    if(i2c->user_onReceive != nullptr)
                    {
                        i2c->rxBufferIndex = 0;
                        i2c->user_onReceive(i2c->rxBufferLength);
                    }
                    *(i2c->FLT) = flt;          // clear STOP/START intr
                    *(i2c->S) = I2C_S_IICIF; // clear intr
                    return;
                }
            #endif
            // Continue Slave Receive
            //
            // setup SDA-rising ISR - required for STOP detection in Slave Rx mode for 3.0/3.1/3.2
            #if defined(__MK20DX128__) || defined(__MK20DX256__) // 3.0/3.1
                i2c->irqCount = 0;
                if(i2c->currentPins == I2C_PINS_18_19)
                    attachInterrupt(18, i2c_t3::sda0_rising_isr, RISING);
                else if(i2c->currentPins == I2C_PINS_16_17)
                    attachInterrupt(17, i2c_t3::sda0_rising_isr, RISING);
                #if I2C_BUS_NUM >= 2
                else if(i2c->currentPins == I2C_PINS_29_30)
                    attachInterrupt(30, i2c_t3::sda1_rising_isr, RISING);
                else if(i2c->currentPins == I2C_PINS_26_31)
                    attachInterrupt(31, i2c_t3::sda1_rising_isr, RISING);
                #endif
            #endif
            data = *(i2c->D);
            if(i2c->rxBufferLength < I2C_RX_BUFFER_LENGTH)
                i2c->rxBuffer[i2c->rxBufferLength++] = data;
        }
        #if defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) // LC/3.5/3.6
            *(i2c->FLT) = flt;   // clear STOP/START intr
        #endif
        *(i2c->S) = I2C_S_IICIF; // clear intr
    }
}

#if defined(__MK20DX128__) || defined(__MK20DX256__) // 3.0/3.1/3.2
// ------------------------------------------------------------------------------------------------------
// SDA-Rising Interrupt Service Routine - 3.0/3.1/3.2 only
//
// Detects the stop condition that terminates a slave receive transfer.
//

// I2C0 SDA ISR
void i2c_t3::sda0_rising_isr(void)
{
    i2c_t3::sda_rising_isr_handler(&(i2c_t3::i2cData[0]),0);
}

#if I2C_BUS_NUM >= 2
    // I2C1 SDA ISR
    void i2c_t3::sda1_rising_isr(void)
    {
        i2c_t3::sda_rising_isr_handler(&(i2c_t3::i2cData[1]),1);
    }
#endif

//
// SDA ISR base handler
//
void i2c_t3::sda_rising_isr_handler(struct i2cStruct* i2c, uint8_t bus)
{
    uint8_t status = *(i2c->S); // capture status first, can change if ISR is too slow
    if(!(status & I2C_S_BUSY))
    {
        i2c->currentStatus = I2C_WAITING;
        if(i2c->currentPins == I2C_PINS_18_19)
            detachInterrupt(18);
        else if(i2c->currentPins == I2C_PINS_16_17)
            detachInterrupt(17);
        #if I2C_BUS_NUM >= 2
        else if(i2c->currentPins == I2C_PINS_29_30)
            detachInterrupt(30);
        else if(i2c->currentPins == I2C_PINS_26_31)
            detachInterrupt(31);
        #endif
        if(i2c->user_onReceive != nullptr)
        {
            i2c->rxBufferIndex = 0;
            i2c->user_onReceive(i2c->rxBufferLength);
        }
    }
    else
    {
        if(++(i2c->irqCount) >= 2 || !(i2c->currentMode == I2C_SLAVE))
        {
            if(i2c->currentPins == I2C_PINS_18_19)
                detachInterrupt(18);
            else if(i2c->currentPins == I2C_PINS_16_17)
                detachInterrupt(17);
            #if I2C_BUS_NUM >= 2
            else if(i2c->currentPins == I2C_PINS_29_30)
                detachInterrupt(30);
            else if(i2c->currentPins == I2C_PINS_26_31)
                detachInterrupt(31);
            #endif
        }
    }
}
#endif // sda_rising_isr

// ------------------------------------------------------------------------------------------------------
// Instantiate
//
i2c_t3 Wire  = i2c_t3(0);       // I2C0
#if I2C_BUS_NUM >= 2
    i2c_t3 Wire1 = i2c_t3(1);   // I2C1
#endif
#if I2C_BUS_NUM >= 3
    i2c_t3 Wire2 = i2c_t3(2);   // I2C2
#endif
#if I2C_BUS_NUM >= 4
    i2c_t3 Wire3 = i2c_t3(3);   // I2C3
#endif

#endif // i2c_t3
