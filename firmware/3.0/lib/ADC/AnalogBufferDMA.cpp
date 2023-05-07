/* Teensy 3.x, LC, 4.0 ADC library
   https://github.com/pedvide/ADC
   Copyright (c) 2020 Pedro Villanueva

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#include "AnalogBufferDMA.h"

#ifdef ADC_USE_DMA

//#define DEBUG_DUMP_DATA
// Global objects
AnalogBufferDMA *AnalogBufferDMA::_activeObjectPerADC[2] = {nullptr, nullptr};

#if defined(__IMXRT1062__) // Teensy 4.0
#define SOURCE_ADC_0 ADC1_R0
#define DMAMUX_ADC_0 DMAMUX_SOURCE_ADC1
#define SOURCE_ADC_1 ADC2_R0
#define DMAMUX_ADC_1 DMAMUX_SOURCE_ADC2
#elif defined(KINETISK)
#define SOURCE_ADC_0 ADC0_RA
#define DMAMUX_ADC_0 DMAMUX_SOURCE_ADC0
#ifdef ADC_DUAL_ADCS
#define SOURCE_ADC_1 ADC1_RA
#define DMAMUX_ADC_1 DMAMUX_SOURCE_ADC1
#endif
#elif defined(KINETISL)
#define SOURCE_ADC_0 ADC0_RA
#define DMAMUX_ADC_0 DMAMUX_SOURCE_ADC0
#endif

//=============================================================================
// Debug support
//=============================================================================

#ifdef DEBUG_DUMP_DATA
static void dumpDMA_TCD(DMABaseClass *dmabc)
{
#ifndef KINETISL
  Serial.printf("%x %x:", (uint32_t)dmabc, (uint32_t)dmabc->TCD);

  Serial.printf("SA:%x SO:%d AT:%x NB:%x SL:%d DA:%x DO: %d CI:%x DL:%x CS:%x BI:%x\n", (uint32_t)dmabc->TCD->SADDR,
                dmabc->TCD->SOFF, dmabc->TCD->ATTR, dmabc->TCD->NBYTES, dmabc->TCD->SLAST, (uint32_t)dmabc->TCD->DADDR,
                dmabc->TCD->DOFF, dmabc->TCD->CITER, dmabc->TCD->DLASTSGA, dmabc->TCD->CSR, dmabc->TCD->BITER);
#else
  Serial.printf("%x %x:", (uint32_t)dmabc, (uint32_t)dmabc->CFG);

  Serial.printf("SAR:%x DAR:%x DSR_BCR:%x DCR:%x\n", (uint32_t)dmabc->CFG->SAR, dmabc->CFG->DAR, dmabc->CFG->DSR_BCR,
                dmabc->CFG->DCR);
#endif
}
#endif

//=============================================================================
// Init - Initialize the object including setup DMA structures
//=============================================================================
void AnalogBufferDMA::init(ADC *adc, int8_t adc_num)
{
  // enable DMA and interrupts
#ifdef DEBUG_DUMP_DATA
  Serial.println("AnalogBufferDMA::init");
  Serial.flush();
#endif

#ifndef KINETISL
  // setup a DMA Channel.
  // Now lets see the different things that RingbufferDMA setup for us before
  // See if we were created with one or two buffers.  If one assume we stop on completion, else assume continuous.
  if (_buffer2 && _buffer2_count)
  {
#ifdef ADC_DUAL_ADCS
    _dmasettings_adc[0].source((volatile uint16_t &)((adc_num == 1) ? SOURCE_ADC_1 : SOURCE_ADC_0));
#else
    _dmasettings_adc[0].source((volatile uint16_t &)(SOURCE_ADC_0));
#endif
    _dmasettings_adc[0].destinationBuffer((uint16_t *)_buffer1, _buffer1_count * 2); // 2*b_size is necessary for some reason
    _dmasettings_adc[0].replaceSettingsOnCompletion(_dmasettings_adc[1]);            // go off and use second one...
    _dmasettings_adc[0].interruptAtCompletion();                                     //interruptAtHalf or interruptAtCompletion
#ifdef ADC_DUAL_ADCS
    _dmasettings_adc[1].source((volatile uint16_t &)((adc_num == 1) ? SOURCE_ADC_1 : SOURCE_ADC_0));
    _dmasettings_adc[1].destinationBuffer((uint16_t *)_buffer2, _buffer2_count * 2); // 2*b_size is necessary for some reason
    _dmasettings_adc[1].replaceSettingsOnCompletion(_dmasettings_adc[0]);            // Cycle back to the first one
    _dmasettings_adc[1].interruptAtCompletion();                                     //interruptAtHalf or interruptAtCompletion
#endif

    _dmachannel_adc = _dmasettings_adc[0];

    _stop_on_completion = false;
  }
  else
  {
// Only one buffer so lets just setup the dmachannel ...
// Serial.printf("AnalogBufferDMA::init Single buffer %d\n", adc_num);
#ifdef ADC_DUAL_ADCS
    _dmachannel_adc.source((volatile uint16_t &)((adc_num == 1) ? SOURCE_ADC_1 : SOURCE_ADC_0));
#else
    _dmachannel_adc.source((volatile uint16_t &)(SOURCE_ADC_0));
#endif
    _dmachannel_adc.destinationBuffer((uint16_t *)_buffer1, _buffer1_count * 2); // 2*b_size is necessary for some reason
    _dmachannel_adc.interruptAtCompletion();                                     //interruptAtHalf or interruptAtCompletion
    _dmachannel_adc.disableOnCompletion();                                       // we will disable on completion.
    _stop_on_completion = true;
  }

  if (adc_num == 1)
  {
#ifdef ADC_DUAL_ADCS
    _activeObjectPerADC[1] = this;
    _dmachannel_adc.attachInterrupt(&adc_1_dmaISR);
    _dmachannel_adc.triggerAtHardwareEvent(DMAMUX_ADC_1); // start DMA channel when ADC finishes a conversion
#endif
  }
  else
  {
    _activeObjectPerADC[0] = this;
    _dmachannel_adc.attachInterrupt(&adc_0_dmaISR);
    _dmachannel_adc.triggerAtHardwareEvent(DMAMUX_ADC_0); // start DMA channel when ADC finishes a conversion
  }
  //arm_dcache_flush((void*)dmaChannel, sizeof(dmaChannel));
  _dmachannel_adc.enable();

  adc->adc[adc_num]->continuousMode();
  adc->adc[adc_num]->enableDMA();

#ifdef DEBUG_DUMP_DATA
  dumpDMA_TCD(&_dmachannel_adc);
  dumpDMA_TCD(&_dmasettings_adc[0]);
  dumpDMA_TCD(&_dmasettings_adc[1]);
#if defined(__IMXRT1062__) // Teensy 4.0

  if (adc_num == 1)
  {
    Serial.printf("ADC2: HC0:%x HS:%x CFG:%x GC:%x GS:%x\n", ADC2_HC0, ADC2_HS, ADC2_CFG, ADC2_GC, ADC2_GS);
  }
  else
  {
    Serial.printf("ADC1: HC0:%x HS:%x CFG:%x GC:%x GS:%x\n", ADC1_HC0, ADC1_HS, ADC1_CFG, ADC1_GC, ADC1_GS);
  }
#endif
#endif
#else
  // Kinetisl (TLC)
  // setup a DMA Channel.
  // Now lets see the different things that RingbufferDMA setup for us before
  _dmachannel_adc.source((volatile uint16_t &)(SOURCE_ADC_0));
  ;
  _dmachannel_adc.destinationBuffer((uint16_t *)_buffer1, _buffer1_count * 2); // 2*b_size is necessary for some reason
  _dmachannel_adc.disableOnCompletion();                                       // ISR will hae to restart with other buffer
  _dmachannel_adc.interruptAtCompletion();                                     //interruptAtHalf or interruptAtCompletion
  _activeObjectPerADC[0] = this;
  _dmachannel_adc.attachInterrupt(&adc_0_dmaISR);
  _dmachannel_adc.triggerAtHardwareEvent(DMAMUX_ADC_0); // start DMA channel when ADC finishes a conversion
  _dmachannel_adc.enable();

  adc->startContinuous(adc_num);
  adc->adc[adc_num]->enableDMA();
#ifdef DEBUG_DUMP_DATA
  dumpDMA_TCD(&_dmachannel_adc);
#endif

#endif

  _last_isr_time = millis();
}

//=============================================================================
// stopOnCompletion: allows you to turn on or off stopping when a DMA buffer
//    has completed filling. Default is on when only one buffer passed in to the
//    constructor and off if two buffers passed in.
//=============================================================================
void AnalogBufferDMA::stopOnCompletion(bool stop_on_complete)
{
#ifndef KINETISL
  if (stop_on_complete)
    _dmachannel_adc.TCD->CSR |= DMA_TCD_CSR_DREQ;
  else
    _dmachannel_adc.TCD->CSR &= ~DMA_TCD_CSR_DREQ;
#else
  if (stop_on_complete)
    _dmachannel_adc.CFG->DCR |= DMA_DCR_D_REQ;
  else
    _dmachannel_adc.CFG->DCR &= ~DMA_DCR_D_REQ;
#endif
  _stop_on_completion = stop_on_complete;
}

//=============================================================================
// ClearCompletion: if we have stop on completion, then clear the completion state
//                  i.e. reenable the dma operation.  Note only valid if we are
//                  in the stopOnCompletion state.
//=============================================================================
bool AnalogBufferDMA::clearCompletion()
{
  if (!_stop_on_completion)
    return false;
  // should probably check to see if we are dsiable or not...
  _dmachannel_adc.enable();
  return true;
}

//=============================================================================
// processADC_DMAISR: Process the DMA completion ISR
//     common for both ISRs on those processors who have more than one ADC
//=============================================================================
void AnalogBufferDMA::processADC_DMAISR()
{
  //digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
  uint32_t cur_time = millis();

  _interrupt_count++;
  _interrupt_delta_time = cur_time - _last_isr_time;
  _last_isr_time = cur_time;
  // update the internal buffer positions
  _dmachannel_adc.clearInterrupt();
#ifdef KINETISL
  // Lets try to clear the previous interrupt, change buffers
  // and restart
  if (_buffer2 && (_interrupt_count & 1))
  {
    _dmachannel_adc.destinationBuffer((uint16_t *)_buffer2, _buffer2_count * 2); // 2*b_size is necessary for some reason
  }
  else
  {
    _dmachannel_adc.destinationBuffer((uint16_t *)_buffer1, _buffer1_count * 2); // 2*b_size is necessary for some reason
  }

  // If we are not stopping on completion, then reenable...
  if (!_stop_on_completion)
    _dmachannel_adc.enable();

#endif
}

//=============================================================================
// adc_0_dmaISR: called for first ADC when DMA has completed filling a buffer.
//=============================================================================
void AnalogBufferDMA::adc_0_dmaISR()
{
  if (_activeObjectPerADC[0])
  {
    _activeObjectPerADC[0]->processADC_DMAISR();
  }
#if defined(__IMXRT1062__) // Teensy 4.0
  asm("DSB");
#endif
}

//=============================================================================
// adc_1_dmaISR - Used for processors that have a second ADC object
//=============================================================================
void AnalogBufferDMA::adc_1_dmaISR()
{
  if (_activeObjectPerADC[1])
  {
    _activeObjectPerADC[1]->processADC_DMAISR();
  }
#if defined(__IMXRT1062__) // Teensy 4.0
  asm("DSB");
#endif
}

#endif // ADC_USE_DMA
