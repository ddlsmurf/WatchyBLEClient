#ifndef BUTTON_INTERRUPT_H__
#define BUTTON_INTERRUPT_H__

#include <Arduino.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Attach interrupt to pin, which will control bit <index> in the result of buttonGetPressMask */
    void buttonSetup(int pin, uintptr_t index);
    uint8_t buttonGetPressMask();
#define buttonWasPressed(pressMask, index) (((pressMask) & (1 << (index))) != 0)

#ifdef __cplusplus
}
#endif

#endif // BUTTON_INTERRUPT_H__