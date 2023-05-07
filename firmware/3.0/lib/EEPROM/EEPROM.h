/*
  EEPROM.h - EEPROM library
  Original Copyright (c) 2006 David A. Mellis.  All right reserved.
  New version by Christopher Andrews 2015.
  This copy has minor modificatons for use with Teensy, by Paul Stoffregen

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

#ifndef EEPROM_h
#define EEPROM_h

#include <inttypes.h>
#include <avr/eeprom.h>
#include <avr/io.h>

#if defined(__has_include) && __has_include(<type_traits>)
#include <type_traits>
#endif

#include <WString.h>    // need to include this so String is defined

/***
    EERef class.

    This object references an EEPROM cell.
    Its purpose is to mimic a typical byte of RAM, however its storage is the EEPROM.
    This class has an overhead of two bytes, similar to storing a pointer to an EEPROM cell.
***/

struct EERef{

    EERef( const int index )
        : index( index )                 {}

    //Access/read members.
    uint8_t operator*() const            { return eeprom_read_byte( (uint8_t*) index ); }
    operator const uint8_t() const       { return **this; }

    //Assignment/write members.
    EERef &operator=( const EERef &ref ) { return *this = *ref; }
    EERef &operator=( uint8_t in )       { return eeprom_write_byte( (uint8_t*) index, in ), *this;  }
    EERef &operator +=( uint8_t in )     { return *this = **this + in; }
    EERef &operator -=( uint8_t in )     { return *this = **this - in; }
    EERef &operator *=( uint8_t in )     { return *this = **this * in; }
    EERef &operator /=( uint8_t in )     { return *this = **this / in; }
    EERef &operator ^=( uint8_t in )     { return *this = **this ^ in; }
    EERef &operator %=( uint8_t in )     { return *this = **this % in; }
    EERef &operator &=( uint8_t in )     { return *this = **this & in; }
    EERef &operator |=( uint8_t in )     { return *this = **this | in; }
    EERef &operator <<=( uint8_t in )    { return *this = **this << in; }
    EERef &operator >>=( uint8_t in )    { return *this = **this >> in; }

    EERef &update( uint8_t in )          { return  in != *this ? *this = in : *this; }

    /** Prefix increment/decrement **/
    EERef& operator++()                  { return *this += 1; }
    EERef& operator--()                  { return *this -= 1; }

    /** Postfix increment/decrement **/
    uint8_t operator++ (int) {
        uint8_t ret = **this;
        return ++(*this), ret;
    }

    uint8_t operator-- (int) {
        uint8_t ret = **this;
        return --(*this), ret;
    }

    int index; //Index of current EEPROM cell.
};

/***
    EEPtr class.

    This object is a bidirectional pointer to EEPROM cells represented by EERef objects.
    Just like a normal pointer type, this can be dereferenced and repositioned using
    increment/decrement operators.
***/

struct EEPtr{

    EEPtr( const int index )
        : index( index )                {}

    operator const int() const          { return index; }
    EEPtr &operator=( int in )          { return index = in, *this; }

    //Iterator functionality.
    bool operator!=( const EEPtr &ptr ) { return index != ptr.index; }
    EERef operator*()                   { return index; }

    /** Prefix & Postfix increment/decrement **/
    EEPtr& operator++()                 { return ++index, *this; }
    EEPtr& operator--()                 { return --index, *this; }
    EEPtr operator++ (int)              { return index++; }
    EEPtr operator-- (int)              { return index--; }

    int index; //Index of current EEPROM cell.
};

/***
    EEPROMClass class.

    This object represents the entire EEPROM space.
    It wraps the functionality of EEPtr and EERef into a basic interface.
    This class is also 100% backwards compatible with earlier Arduino core releases.
***/

struct EEPROMClass{

#if defined(__arm__) && defined(TEENSYDUINO)
    EEPROMClass()                        { eeprom_initialize(); }
#endif

    //Basic user access methods.
    EERef operator[]( const int idx )    { return idx; }
    uint8_t read( int idx )              { return EERef( idx ); }
    void write( int idx, uint8_t val )   { (EERef( idx )) = val; }
    void update( int idx, uint8_t val )  { EERef( idx ).update( val ); }

    //STL and C++11 iteration capability.
    EEPtr begin()                        { return 0x00; }
    EEPtr end()                          { return length(); } //Standards requires this to be the item after the last valid entry. The returned pointer is invalid.
    uint16_t length()                    { return E2END + 1; }

    //Functionality to 'get' and 'put' objects to and from EEPROM.
    template< typename T > T &get( int idx, T &t ){
        #if defined(__has_include) && __has_include(<type_traits>)
        static_assert(std::is_trivially_copyable<T>::value,"You can not use this type with EEPROM.get" ); // the code below only makes sense if you can "memcpy" T
        #endif
        EEPtr e = idx;
        uint8_t *ptr = (uint8_t*) &t;
        for( int count = sizeof(T) ; count ; --count, ++e )  *ptr++ = *e;
        return t;
    }

    template< typename T > const T &put( int idx, const T &t ){
        #if defined(__has_include) && __has_include(<type_traits>)
        static_assert(std::is_trivially_copyable<T>::value, "You can not use this type with EEPROM.put"); // the code below only makes sense if you can "memcpy" T
        #endif
        const uint8_t *ptr = (const uint8_t*) &t;
#ifdef __arm__
        eeprom_write_block(ptr, (void *)idx, sizeof(T));
#else
        EEPtr e = idx;
        for( int count = sizeof(T) ; count ; --count, ++e )  (*e).update( *ptr++ );
#endif
        return t;
    }
};


// put - Specialization for Arduino Strings -------------------------------
// to put an Arduino String to the EEPROM we copy its internal buffer
// including the trailing \0 to the eprom

template <>
inline const String &EEPROMClass::put(int idx, const String &s)
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
inline String &EEPROMClass::get(int idx, String &s){
    s = "";             // just in case...
    EEPtr e = idx;

    char c = *e;        // read in bytes until we find the terminating \0
    while (c != '\0')
    {
        s.append(c);
        c = *(++e);
    }
    return s;
}


static EEPROMClass EEPROM __attribute__ ((unused));
#endif
