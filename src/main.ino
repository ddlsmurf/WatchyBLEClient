/*
  Copyright (c) 2014-2020 Electronic Cats SAPI de CV.  All right reserved.
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <esp32notifications.h>
#include <GxEPD2_BW.h>
#include "config.h"
#include "button_interrupt.h"
#include "watchy_hw.h"
#include "ble_notification.h"
#include "watchy_display_state.h"

static char LOG_TAG[] = "MAIN.INO";

// typedef GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display_t;
display_t display(GxEPD2_154_D67(WATCHY_DISPLAY_CS_PIN, WATCHY_DISPLAY_DC_PIN, WATCHY_DISPLAY_RESET_PIN, WATCHY_DISPLAY_BUSY_PIN));
BLENotifications notifications;
WatchyDisplayState displayState(display, notifications);

void onBLEStateChanged(BLENotifications::State state, const void *userData)
{
  switch (state)
  {
  case BLENotifications::StateConnected:
    Serial.println("StateConnected - connected to a phone or tablet");
    break;

  case BLENotifications::StateDisconnected:
    Serial.println("StateDisconnected - disconnected from a phone or tablet");
    // notifications.startAdvertising();
    ESP.restart(); // not sure why advertising restart doesnt work
    break;
  }
  displayState.setConnected(state == BLENotifications::StateConnected);
}

void onAMSTrackUpdated(const AMSTrackAttributeID_t attribute, const std::string &value, const void *userData)
{
  displayState.addAMSTrackAttribute(attribute, value);
}

void onAMSPlayerUpdated(const AMSPlayerAttributeID_t attribute, const std::string &value, const void *userData)
{
  displayState.addAMSPlayerAttribute(attribute, value);
}

static const char *ANCSEventIdNotificationToString(ANCS::event_id_t id)
{
  switch (id)
  {
  case ANCS::EventIDNotificationAdded:
    return "NotificationAdded";
  case ANCS::EventIDNotificationModified:
    return "NotificationModified";
  case ANCS::EventIDNotificationRemoved:
    return "NotificationRemoved";
  default:
    return "NotificationAdded???";
  }
}
static const char *ANCSEventFlagsToString(ANCS::EventFlags id)
{
  static std::string result = "EventFlags:";
  if (id & ANCS::EventFlagSilent)
    result += " Silent";
  if (id & ANCS::EventFlagImportant)
    result += " Important";
  if (id & ANCS::EventFlagPreExisting)
    result += " PreExisting";
  if (id & ANCS::EventFlagPositiveAction)
    result += " PositiveAction";
  if (id & ANCS::EventFlagNegativeAction)
    result += " NegativeAction";
  return result.c_str();
}

void onNotificationArrived(const Notification *rawData, const void *userData)
{
  displayState.setANCSNotification(rawData);
}

void onNotificationRemoved(const Notification *rawData, const void *userData)
{
  displayState.clearANCSNotification(rawData->uuid);
}

void setup()
{
  // delay(2000);
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  notifications.begin(WATCHY_BLE_NAME);

  notifications.setConnectionStateChangedCallback(onBLEStateChanged);
  notifications.setNotificationCallback(onNotificationArrived);
  notifications.setRemovedCallback(onNotificationRemoved);

  notifications.setOnAMSPlayerUpdateCB(onAMSPlayerUpdated);
  notifications.setOnAMSTrackUpdateCB(onAMSTrackUpdated);

  buttonSetup(WATCHY_BUTTON_1_PIN, 0);
  buttonSetup(WATCHY_BUTTON_2_PIN, 1);
  buttonSetup(WATCHY_BUTTON_3_PIN, 2);
  buttonSetup(WATCHY_BUTTON_4_PIN, 3);
}

void loop()
{
  uint8_t presses = buttonGetPressMask();
  for (int i = 0; i < 4; i++)
    if (buttonWasPressed(presses, i))
      ESP_LOGI(LOG_TAG, "Button %d press", i + 1);
  displayState.handleButtonPress(presses);
  delay(50);
  displayState.updateIfNeeded();
}
