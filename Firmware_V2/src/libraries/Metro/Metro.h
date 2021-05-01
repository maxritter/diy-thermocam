

#ifndef Metro_h
#define Metro_h

#include <inttypes.h>

class Metro
{

public:
  void begin(unsigned long interval_millis);
  void begin(unsigned long interval_millis, uint8_t autoreset);
  void interval(unsigned long interval_millis);
  char check();
  void reset();
	
private:
  uint8_t autoreset;
  unsigned long  previous_millis, interval_millis;

};

#endif


