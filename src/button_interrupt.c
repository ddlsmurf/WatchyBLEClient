#include "button_interrupt.h"

#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 150
#endif

volatile uint8_t buttonPressMask = 0;
volatile unsigned long buttonLastPressTime = 0;
portMUX_TYPE buttonISRMutex = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR ISR_buttonPress(void *arg)
{
  portENTER_CRITICAL_ISR(&buttonISRMutex);
  uintptr_t buttonIndex = (uintptr_t)arg;
  unsigned long now = xTaskGetTickCount(); // @todo something buggy about this debounce
  if ((now - buttonLastPressTime > BUTTON_DEBOUNCE_MS) || (now < buttonLastPressTime))
  {
    buttonPressMask |= 1 << buttonIndex;
    buttonLastPressTime = now;
  }
  portEXIT_CRITICAL_ISR(&buttonISRMutex);
}

void buttonSetup(int pin, uintptr_t index)
{
  pinMode(pin, INPUT);
  attachInterruptArg(pin, ISR_buttonPress, (void *)index, 2);
}

uint8_t buttonGetPressMask()
{
  uint8_t presses = 0;
  portENTER_CRITICAL(&buttonISRMutex);
  presses = buttonPressMask;
  buttonPressMask = 0;
  portEXIT_CRITICAL(&buttonISRMutex);
  return presses;
}