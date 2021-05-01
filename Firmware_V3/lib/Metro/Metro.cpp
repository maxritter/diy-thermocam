
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include "Metro.h"


void Metro::begin(unsigned long interval_millis)
{
	this->autoreset = 0;
	interval(interval_millis);
	reset();
}

// New creator so I can use either the original check behavior or benjamin.soelberg's
// suggested one (see below). 
// autoreset = 0 is benjamin.soelberg's check behavior
// autoreset != 0 is the original behavior

void Metro::begin(unsigned long interval_millis, uint8_t autoreset)
{   
	this->autoreset = autoreset; // Fix by Paul Bouchier
	interval(interval_millis);
	reset();
}

void Metro::interval(unsigned long interval_millis)
{
  this->interval_millis = interval_millis;
}

// Benjamin.soelberg's check behavior:
// When a check is true, add the interval to the internal counter.
// This should guarantee a better overall stability.

// Original check behavior:
// When a check is true, add the interval to the current millis() counter.
// This method can add a certain offset over time.

char Metro::check()
{
  if (millis() - this->previous_millis >= this->interval_millis) {
    // As suggested by benjamin.soelberg@gmail.com, the following line 
    // this->previous_millis = millis();
    // was changed to
    // this->previous_millis += this->interval_millis;
    
    // If the interval is set to 0 we revert to the original behavior
    if (this->interval_millis <= 0 || this->autoreset ) {
    	this->previous_millis = millis();
	} else {
		this->previous_millis += this->interval_millis; 
	}
    
    return 1;
  }
  
  
  
  return 0;

}

void Metro::reset() 
{
 
  this->previous_millis = millis();

}


