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

#ifndef ADC_ATOMIC_H
#define ADC_ATOMIC_H

/*  int __builtin_ctz (unsigned int x):
    Returns the number of trailing 0-bits in x, 
    starting at the least significant bit position. 
    If x is 0, the result is undefined. 
*/
/* int __builtin_clz (unsigned int x)
   Returns the number of leading 0-bits in x, 
   starting at the most significant bit position. 
   If x is 0, the result is undefined. 
*/
/* int __builtin_popcount (unsigned int x)
    Returns the number of 1-bits in x. 
*/

// kinetis.h has the following types for addresses: uint32_t, uint16_t, uint8_t, int32_t, int16_t

//! Atomic set, clear, change, or get bit in a register
namespace atomic
{
/////// Atomic bit set/clear
/* Clear bit in address (make it zero), set bit (make it one), or return the value of that bit
    *   changeBitFlag can change up to 2 bits in a flag at the same time
    *   We can change this functions depending on the board.
    *   Teensy 3.x use bitband while Teensy LC has a more advanced bit manipulation engine.
    *   Teensy 4 also has bitband capabilities, but are not yet implemented, instead registers are 
    *   set and cleared manually. TODO: fix this.
    */
#if defined(KINETISK) // Teensy 3.x
    //! Bitband address
    /** Gets the aliased address of the bit-band register
    *   \param reg  Register in the bit-band area
    *   \param bit  Bit number of reg to read/modify
    *   \return A pointer to the aliased address of the bit of reg
    */
    template <typename T>
    __attribute__((always_inline)) inline volatile T &bitband_address(volatile T &reg, uint8_t bit)
    {
        return (*(volatile T *)(((uint32_t)&reg - 0x40000000) * 32 + bit * 4 + 0x42000000));
    }

    template <typename T>
    __attribute__((always_inline)) inline void setBit(volatile T &reg, uint8_t bit)
    {
        bitband_address(reg, bit) = 1;
    }
    template <typename T>
    __attribute__((always_inline)) inline void setBitFlag(volatile T &reg, T flag)
    {
        // 31-__builtin_clzl(flag) = gets bit number in flag
        // __builtin_clzl works for long ints, which are guaranteed by standard to be at least 32 bit wide.
        // there's no difference in the asm emitted.
        bitband_address(reg, 31 - __builtin_clzl(flag)) = 1;
        if (__builtin_popcount(flag) > 1)
        {
            // __builtin_ctzl returns the number of trailing 0-bits in x, starting at the least significant bit position
            bitband_address(reg, __builtin_ctzl(flag)) = 1;
        }
    }

    template <typename T>
    __attribute__((always_inline)) inline void clearBit(volatile T &reg, uint8_t bit)
    {
        bitband_address(reg, bit) = 0;
    }
    template <typename T>
    __attribute__((always_inline)) inline void clearBitFlag(volatile T &reg, T flag)
    {
        bitband_address(reg, 31 - __builtin_clzl(flag)) = 0;
        if (__builtin_popcount(flag) > 1)
        {
            bitband_address(reg, __builtin_ctzl(flag)) = 0;
        }
    }

    template <typename T>
    __attribute__((always_inline)) inline void changeBit(volatile T &reg, uint8_t bit, bool state)
    {
        bitband_address(reg, bit) = state;
    }
    template <typename T>
    __attribute__((always_inline)) inline void changeBitFlag(volatile T &reg, T flag, T state)
    {
        bitband_address(reg, __builtin_ctzl(flag)) = (state >> __builtin_ctzl(flag)) & 0x1;
        if (__builtin_popcount(flag) > 1)
        {
            bitband_address(reg, 31 - __builtin_clzl(flag)) = (state >> (31 - __builtin_clzl(flag))) & 0x1;
        }
    }

    template <typename T>
    __attribute__((always_inline)) inline volatile bool getBit(volatile T &reg, uint8_t bit)
    {
        return (volatile bool)bitband_address(reg, bit);
    }
    template <typename T>
    __attribute__((always_inline)) inline volatile bool getBitFlag(volatile T &reg, T flag)
    {
        return (volatile bool)bitband_address(reg, 31 - __builtin_clzl(flag));
    }

#elif defined(__IMXRT1062__) // Teensy 4
    template <typename T>
    __attribute__((always_inline)) inline void setBitFlag(volatile T &reg, T flag)
    {
        __disable_irq();
        reg |= flag;
        __enable_irq();
    }

    template <typename T>
    __attribute__((always_inline)) inline void clearBitFlag(volatile T &reg, T flag)
    {
        __disable_irq();
        reg &= ~flag;
        __enable_irq();
    }

    template <typename T>
    __attribute__((always_inline)) inline void changeBitFlag(volatile T &reg, T flag, T state)
    {
        // flag can be 1 or 2 bits wide
        // state can have one or two bits set
        if (__builtin_popcount(flag) == 1)
        { // 1 bit
            if (state)
            {
                setBitFlag(reg, flag);
            }
            else
            {
                clearBitFlag(reg, flag);
            }
        }
        else
        { // 2 bits
            // lsb first
            if ((state >> __builtin_ctzl(flag)) & 0x1)
            { // lsb of state is 1
                setBitFlag(reg, (uint32_t)(1 << __builtin_ctzl(flag)));
            }
            else
            { // lsb is 0
                clearBitFlag(reg, (uint32_t)(1 << __builtin_ctzl(flag)));
            }
            // msb
            if ((state >> (31 - __builtin_clzl(flag))) & 0x1)
            { // msb of state is 1
                setBitFlag(reg, (uint32_t)(1 << (31 - __builtin_clzl(flag))));
            }
            else
            { // msb is 0
                clearBitFlag(reg, (uint32_t)(1 << (31 - __builtin_clzl(flag))));
            }
        }
    }

    template <typename T>
    __attribute__((always_inline)) inline volatile bool getBitFlag(volatile T &reg, T flag)
    {
        return (volatile bool)((reg)&flag) >> (31 - __builtin_clzl(flag));
    }

#elif defined(KINETISL) // Teensy LC
    // bit manipulation engine

    template <typename T>
    __attribute__((always_inline)) inline void setBit(volatile T &reg, uint8_t bit)
    {
        //temp = *(uint32_t *)((uint32_t)(reg) | (1<<26) | (bit<<21)); // LAS
        *(volatile T *)((uint32_t)(&reg) | (1 << 27)) = 1 << bit; // OR
    }
    template <typename T>
    __attribute__((always_inline)) inline void setBitFlag(volatile T &reg, uint32_t flag)
    {
        *(volatile T *)((uint32_t)&reg | (1 << 27)) = flag; // OR
    }

    template <typename T>
    __attribute__((always_inline)) inline void clearBit(volatile T &reg, uint8_t bit)
    {
        //temp = *(uint32_t *)((uint32_t)(reg) | (3<<27) | (bit<<21)); // LAC
        *(volatile T *)((uint32_t)(&reg) | (1 << 26)) = ~(1 << bit); // AND
    }
    template <typename T>
    __attribute__((always_inline)) inline void clearBitFlag(volatile T &reg, uint32_t flag)
    {
        //temp = *(uint32_t *)((uint32_t)(reg) | (3<<27) | (bit<<21)); // LAC
        *(volatile T *)((uint32_t)(&reg) | (1 << 26)) = ~flag; // AND
    }

    template <typename T>
    __attribute__((always_inline)) inline void changeBit(volatile T &reg, uint8_t bit, bool state)
    {
        //temp = *(uint32_t *)((uint32_t)(reg) | ((3-2*!!state)<<27) | (bit<<21)); // LAS/LAC
        state ? setBit(reg, bit) : clearBit(reg, bit);
    }
    template <typename T>
    __attribute__((always_inline)) inline void changeBitFlag(volatile T &reg, T flag, T state)
    {
        // BFI, bitfield width set to __builtin_popcount(flag)
        // least significant bit set to __builtin_ctzl(flag)
        *(volatile T *)((uint32_t)(&reg) | (1 << 28) | (__builtin_ctzl(flag) << 23) | ((__builtin_popcount(flag) - 1) << 19)) = state;
    }

    template <typename T>
    __attribute__((always_inline)) inline volatile bool getBit(volatile T &reg, uint8_t bit)
    {
        return (volatile bool)*(volatile T *)((uint32_t)(&reg) | (1 << 28) | (bit << 23)); // UBFX
    }
    template <typename T>
    __attribute__((always_inline)) inline volatile bool getBitFlag(volatile T &reg, T flag)
    {
        return (volatile bool)*(volatile T *)((uint32_t)(&reg) | (1 << 28) | ((31 - __builtin_clzl(flag)) << 23)); // UBFX
    }

#endif

} // namespace atomic

#endif // ADC_ATOMIC_H
