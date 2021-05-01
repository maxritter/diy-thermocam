/* Teensy 3.x, LC, 4.0 ADC library
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


#ifndef ANALOGBUFFERDMA_H
#define ANALOGBUFFERDMA_H

#include "DMAChannel.h"
#include "ADC.h"

#ifdef ADC_USE_DMA

// lets wrap some of our Dmasettings stuff into helper class
class AnalogBufferDMA
{
    // keep our settings and the like:
public: // At least temporary to play with dma settings.
#ifndef KINETISL
    DMASetting _dmasettings_adc[2];
#endif
    DMAChannel _dmachannel_adc;

    static AnalogBufferDMA *_activeObjectPerADC[2];
    static void adc_0_dmaISR();
    static void adc_1_dmaISR();
    void processADC_DMAISR();

public:
    AnalogBufferDMA(volatile uint16_t *buffer1, uint16_t buffer1_count,
                    volatile uint16_t *buffer2 = nullptr, uint16_t buffer2_count = 0) : _buffer1(buffer1), _buffer1_count(buffer1_count), _buffer2(buffer2), _buffer2_count(buffer2_count){};

    void init(ADC *adc, int8_t adc_num = -1);

    void stopOnCompletion(bool stop_on_complete);
    inline bool stopOnCompletion(void) { return _stop_on_completion; }
    bool clearCompletion();
    inline volatile uint16_t *bufferLastISRFilled() { return (!_buffer2 || (_interrupt_count & 1)) ? _buffer1 : _buffer2; }
    inline uint16_t bufferCountLastISRFilled() { return (!_buffer2 || (_interrupt_count & 1)) ? _buffer1_count : _buffer2_count; }
    inline uint32_t interruptCount() { return _interrupt_count; }
    inline uint32_t interruptDeltaTime() { return _interrupt_delta_time; }
    inline bool interrupted() { return _interrupt_delta_time != 0; }
    inline void clearInterrupt() { _interrupt_delta_time = 0; }
    inline void userData(uint32_t new_data) { _user_data = new_data; }
    inline uint32_t userData(void) { return _user_data; }

protected:
    volatile uint32_t _interrupt_count = 0;
    volatile uint32_t _interrupt_delta_time;
    volatile uint32_t _last_isr_time;

    volatile uint16_t *_buffer1;
    uint16_t _buffer1_count;
    volatile uint16_t *_buffer2;
    uint16_t _buffer2_count;
    uint32_t _user_data = 0;
    bool _stop_on_completion = false;
};

#endif // ADC_USE_DMA
#endif

