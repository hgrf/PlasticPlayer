#pragma once

#include <Arduino.h>

class Led
{
public:
  Led(int pin) : mPin(pin)
  {
  }

  void begin()
  {
    pinMode(mPin, OUTPUT);
  }

  void on()
  {
    digitalWrite(mPin, HIGH);
  }

  void off()
  {
    digitalWrite(mPin, LOW);
  }

private:
  int mPin;
};
