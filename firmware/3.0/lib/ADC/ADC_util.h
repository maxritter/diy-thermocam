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

/* util.h: Util functions for ino sketches and tests.
 *         This would increase the size of the ADC library because of the strings.
 */

/*! \page util ADC util
Util functions for ino sketches and tests.
This would increase the size of the ADC library because of the strings.
See the namespace ADC_util for all functions.
*/

#ifndef ADC_UTIL_H
#define ADC_UTIL_H

#include <settings_defines.h>

using ADC_Error::ADC_ERROR;
using namespace ADC_settings;

//! Util functions for ino sketches and tests.
namespace ADC_util
{

  //! Convert the conversion speed code to text
  /** Convert the conversion speed code to text
* \param conv_speed The conversion speed code
* \return the corresponding text
*/
  const char *getConversionEnumStr(ADC_CONVERSION_SPEED conv_speed)
  {
    switch (conv_speed)
    {
#if defined(ADC_TEENSY_4) // Teensy 4
#else
    case ADC_CONVERSION_SPEED::VERY_LOW_SPEED:
      return (const char *)"VERY_LOW_SPEED";
#endif
    case ADC_CONVERSION_SPEED::LOW_SPEED:
      return (const char *)"LOW_SPEED";
    case ADC_CONVERSION_SPEED::MED_SPEED:
      return (const char *)"MED_SPEED";
    case ADC_CONVERSION_SPEED::HIGH_SPEED:
      return (const char *)"HIGH_SPEED";
#if defined(ADC_TEENSY_4) // Teensy 4
#else
    case ADC_CONVERSION_SPEED::VERY_HIGH_SPEED:
      return (const char *)"VERY_HIGH_SPEED";
#endif
#if defined(ADC_TEENSY_4) // Teensy 4
    case ADC_CONVERSION_SPEED::ADACK_10:
      return (const char *)"ADACK_10";
    case ADC_CONVERSION_SPEED::ADACK_20:
      return (const char *)"ADACK_20";
#else
    case ADC_CONVERSION_SPEED::HIGH_SPEED_16BITS:
      return (const char *)"HIGH_SPEED_16BITS";
    case ADC_CONVERSION_SPEED::ADACK_2_4:
      return (const char *)"ADACK_2_4";
    case ADC_CONVERSION_SPEED::ADACK_4_0:
      return (const char *)"ADACK_4_0";
    case ADC_CONVERSION_SPEED::ADACK_5_2:
      return (const char *)"ADACK_5_2";
    case ADC_CONVERSION_SPEED::ADACK_6_2:
      return (const char *)"ADACK_6_2";
#endif
    }
    return (const char *)"NONE";
  }

  //! Convert the sampling speed code to text
  /** Convert the sampling speed code to text
* \param samp_speed The sampling speed code
* \return the corresponding text
*/
  const char *getSamplingEnumStr(ADC_SAMPLING_SPEED samp_speed)
  {
    switch (samp_speed)
    {
    case ADC_SAMPLING_SPEED::VERY_LOW_SPEED:
      return (const char *)"VERY_LOW_SPEED";
    case ADC_SAMPLING_SPEED::LOW_SPEED:
      return (const char *)"LOW_SPEED";
    case ADC_SAMPLING_SPEED::MED_SPEED:
      return (const char *)"MED_SPEED";
    case ADC_SAMPLING_SPEED::HIGH_SPEED:
      return (const char *)"HIGH_SPEED";
    case ADC_SAMPLING_SPEED::VERY_HIGH_SPEED:
      return (const char *)"VERY_HIGH_SPEED";
#if defined(ADC_TEENSY_4) // Teensy 4
    case ADC_SAMPLING_SPEED::LOW_MED_SPEED:
      return (const char *)"LOW_MED_SPEED";
    case ADC_SAMPLING_SPEED::MED_HIGH_SPEED:
      return (const char *)"MED_HIGH_SPEED";
    case ADC_SAMPLING_SPEED::HIGH_VERY_HIGH_SPEED:
      return (const char *)"HIGH_VERY_HIGH_SPEED";
#endif
    }
    return (const char *)"NONE";
  }

  //! Converts the error code to text.
  /** Converts the error code to text.
* \param fail_flag The error code
* \return the corresponding text
*/
  const char *getStringADCError(ADC_ERROR fail_flag)
  {
    if (fail_flag != ADC_ERROR::CLEAR)
    {
      switch (fail_flag)
      {
      case ADC_ERROR::CALIB:
        return (const char *)"Calibration";
      case ADC_ERROR::WRONG_PIN:
        return (const char *)"Wrong pin";
      case ADC_ERROR::ANALOG_READ:
        return (const char *)"Analog read";
      case ADC_ERROR::COMPARISON:
        return (const char *)"Comparison";
      case ADC_ERROR::ANALOG_DIFF_READ:
        return (const char *)"Analog differential read";
      case ADC_ERROR::CONT:
        return (const char *)"Continuous read";
      case ADC_ERROR::CONT_DIFF:
        return (const char *)"Continuous differential read";
      case ADC_ERROR::WRONG_ADC:
        return (const char *)"Wrong ADC";
      case ADC_ERROR::SYNCH:
        return (const char *)"Synchronous";
      case ADC_ERROR::OTHER:
      case ADC_ERROR::CLEAR: // silence warnings
      default:
        return (const char *)"Unknown";
      }
    }
    return (const char *)"";
  }

  //! List of possible averages
  const uint8_t averages_list[] = {1, 4, 8, 16, 32};

#if defined(ADC_TEENSY_4) // Teensy 4
  //! List of possible resolutions
  const uint8_t resolutions_list[] = {8, 10, 12};
#else
  //! List of possible resolutions
  const uint8_t resolutions_list[] = {8, 10, 12, 16};
#endif

#if defined(ADC_TEENSY_4) // Teensy 4
  //! List of possible conversion speeds
  const ADC_CONVERSION_SPEED conversion_speed_list[] = {
      ADC_CONVERSION_SPEED::LOW_SPEED,
      ADC_CONVERSION_SPEED::MED_SPEED,
      ADC_CONVERSION_SPEED::HIGH_SPEED,
      ADC_CONVERSION_SPEED::ADACK_10,
      ADC_CONVERSION_SPEED::ADACK_20};
#else
  //! List of possible conversion speeds
  const ADC_CONVERSION_SPEED conversion_speed_list[] = {
      ADC_CONVERSION_SPEED::VERY_LOW_SPEED,
      ADC_CONVERSION_SPEED::LOW_SPEED,
      ADC_CONVERSION_SPEED::MED_SPEED,
      ADC_CONVERSION_SPEED::HIGH_SPEED,
      ADC_CONVERSION_SPEED::HIGH_SPEED_16BITS,
      ADC_CONVERSION_SPEED::VERY_HIGH_SPEED,
      ADC_CONVERSION_SPEED::ADACK_2_4,
      ADC_CONVERSION_SPEED::ADACK_4_0,
      ADC_CONVERSION_SPEED::ADACK_5_2,
      ADC_CONVERSION_SPEED::ADACK_6_2};
#endif

#if defined(ADC_TEENSY_4) // Teensy 4
  //! List of possible sampling speeds
  const ADC_SAMPLING_SPEED sampling_speed_list[] = {
      ADC_SAMPLING_SPEED::VERY_LOW_SPEED,
      ADC_SAMPLING_SPEED::LOW_SPEED,
      ADC_SAMPLING_SPEED::LOW_MED_SPEED,
      ADC_SAMPLING_SPEED::MED_SPEED,
      ADC_SAMPLING_SPEED::MED_HIGH_SPEED,
      ADC_SAMPLING_SPEED::HIGH_SPEED,
      ADC_SAMPLING_SPEED::HIGH_VERY_HIGH_SPEED,
      ADC_SAMPLING_SPEED::VERY_HIGH_SPEED};
#else
  //! List of possible sampling speeds
  const ADC_SAMPLING_SPEED sampling_speed_list[] = {
      ADC_SAMPLING_SPEED::VERY_LOW_SPEED,
      ADC_SAMPLING_SPEED::LOW_SPEED,
      ADC_SAMPLING_SPEED::MED_SPEED,
      ADC_SAMPLING_SPEED::HIGH_SPEED,
      ADC_SAMPLING_SPEED::VERY_HIGH_SPEED};
#endif

} // namespace ADC_util

using namespace ADC_util;

#endif // ADC_UTIL_H