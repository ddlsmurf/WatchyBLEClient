#ifndef CONFIG_H__
#define CONFIG_H__

#include <GxEPD2_BW.h>

#include <Fonts/FreeMonoBold9pt7b.h>
#define FONT_BUTTONS FreeMonoBold9pt7b
#include <Fonts/FreeSans9pt7b.h>
#define FONT_BIG FreeSans9pt7b
#include <Fonts/Org_01.h>
#define FONT_SMALL Org_01

#define WATCHY_BLE_NAME "Watchy ANCS AMS"
// Max time to show a notification before returning to AMS screen
#define ANCS_NOTIFICATION_DELAY_MS 10000
// Delay before screen updates
#define SCREEN_UPDATE_DEBOUNCE_PERIOD_MS 200
#define CTS_UPDATE_PERIOD_MS 29000

typedef GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display_t;

#endif // CONFIG_H__
