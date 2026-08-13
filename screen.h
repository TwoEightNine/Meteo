#include "Arduino.h"

class Screen {

public:
    Screen() {};
    ~Screen() {};
    virtual void loop() = 0; 
    virtual void onTouch(uint16_t x, uint16_t y) = 0;
};
