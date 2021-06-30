#include <Arduino.h>
#include <EEPROM.h>

// put - Specialization for Arduino Strings -------------------------------
// to put an Arduino String to the EEPROM we copy its internal buffer
// including the trailing \0 to the eprom

template <>
const String &EEPROMClass::put(int idx, const String &s)
{
    const uint8_t *ptr = (uint8_t *)s.c_str();

#ifdef __arm__
    eeprom_write_block(ptr, (void *)idx, s.length() + 1); // length() doesn't account for the trailing \0
#else
    EEPtr e = idx;
    for (int count = s.length() + 1; count; --count, ++e)
        (*e).update(*ptr++);
#endif
    return s;
}

// get - Specialization for Arduino Strings -------------------------------
// to "get" an Arduino String from the EEPROM we append chars from the EEPROM
// into it until we find the delimiting /0.
// String.append is not very efficient, code could probably be opitimized if required...

template <>
String &EEPROMClass::get(int idx, String &s){
    s = "";             // just in case...
    EEPtr e = idx;

    char c = *e;        // read in bytes until we find the terminating \0
    while (c != '\0') {
        s.append(c);
        c = *(++e);
    }
    return s;
}

