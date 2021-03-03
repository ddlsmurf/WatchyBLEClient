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
#include <Fonts/FreeMonoBold9pt7b.h>
#define FONT_BUTTONS FreeMonoBold9pt7b
#include <Fonts/FreeSans9pt7b.h>
#define FONT_BIG FreeSans9pt7b
#include <Fonts/Org_01.h>
#define FONT_SMALL Org_01
#include "button_interrupt.h"
#include "watchy_hw.h"
#include "ble_notification.h"

static char LOG_TAG[] = "MAIN.INO";
#define WATCHY_BLE_NAME "Watchy ANCS AMS"
#define ANCS_NOTIFICATION_DELAY_MS 10000
#define SCREEN_UPDATE_DEBOUNCE_PERIOD_MS 200

typedef GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display_t;
display_t display(GxEPD2_154_D67(WATCHY_DISPLAY_CS_PIN, WATCHY_DISPLAY_DC_PIN, WATCHY_DISPLAY_RESET_PIN, WATCHY_DISPLAY_BUSY_PIN));
BLENotifications notifications;

class WatchyDisplayState
{
private:
  display_t &display;

  bool firstDisplay = true;

  uint32_t needsUpdate = 1;
  void invalidate()
  {
    if (!needsUpdate)
      needsUpdate = millis();
  }
  void invalidateImmediate()
  {
    needsUpdate = millis() - SCREEN_UPDATE_DEBOUNCE_PERIOD_MS - 1;
  }

  bool bleConnected = false;

  // ANCS state
  uint32_t ancsUUID = 0;
  uint32_t ancsShowTimeMS = 0;
  std::string ancsString;
  ANCS::EventFlags ancsFlags = (ANCS::EventFlags)0;
  bool isShowingANCSNotification() { return ancsShowTimeMS != 0; }

  // AMS state
  bool amsIsPlaying = false;
  float amsVolume = -1;
  std::string amsPlayerName = "";
  std::string amsTrackArtist = "";
  std::string amsTrackAlbum = "";
  std::string amsTrackTitle = "";

  bool amsIsCommandAvailable(AMSRemoteCommandID_t cmd)
  {
    return notifications.clientAMS && notifications.clientAMS->isCommandAvailable(cmd);
  }
  bool amsCheckIsEmpty()
  {
    return amsTrackArtist.length() == 0 &&
           amsTrackAlbum.length() == 0 &&
           amsTrackTitle.length() == 0;
  }

private:
#define ALIGN_NEAR 0
#define ALIGN_MIDDLE 1
#define ALIGN_FAR 2

  uint16_t alignText(const GFXfont *font, const char *text, int halign, int valign)
  {
    // @todo this is terrible at multiline alignment
    display.setFont(font);
    int16_t x, y;
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    // display.drawRect(0, 0, 200, 200, GxEPD_BLACK);
    // ESP_LOGI(LOG_TAG, "Text '%s' is %d ; %d by %d x %d - font: %d", text.c_str(), x1, y1, w, h, font->yAdvance);
    // ESP_LOGI(LOG_TAG, "                                    pg: %d", pgm_read_byte(&font->yAdvance));
    x = halign == ALIGN_NEAR ? 0 : (halign == ALIGN_FAR ? WATCHY_DISPLAY_WIDTH - w - 4 : (WATCHY_DISPLAY_WIDTH - w) / 2);
    y = (valign == ALIGN_NEAR ? 0 : (valign == ALIGN_FAR ? WATCHY_DISPLAY_HEIGHT - h - 4 : (WATCHY_DISPLAY_HEIGHT - h) / 2)) - y1;
    display.setCursor(x, y);
    // display.drawRect(x, y + y1, w, h, GxEPD_BLACK);
    display.println(text);
    return h;
  }

  void drawButtons(const char *button1, const char *button2, const char *button3, const char *button4)
  {
    if (button1 != nullptr)
      alignText(&FONT_BUTTONS, button1, ALIGN_NEAR, ALIGN_FAR);
    if (button2 != nullptr)
      alignText(&FONT_BUTTONS, button2, ALIGN_NEAR, ALIGN_NEAR);
    if (button3 != nullptr)
      alignText(&FONT_BUTTONS, button3, ALIGN_FAR, ALIGN_NEAR);
    if (button4 != nullptr)
      alignText(&FONT_BUTTONS, button4, ALIGN_FAR, ALIGN_FAR);
  }

  void alignTextTest(const GFXfont *font = &FONT_BIG);

  void drawScreenNoBLE()
  {
    std::string message = std::string("BLE not connected\nLook for ") + WATCHY_BLE_NAME;
    alignText(&FONT_BIG, message.c_str(), ALIGN_NEAR, ALIGN_MIDDLE);
  }

  void drawScreenANCSNotification()
  {
    drawButtons(nullptr, nullptr,
                (ancsFlags & ANCS::EventFlagPositiveAction) != 0 ? "V" : nullptr,
                (ancsFlags & ANCS::EventFlagNegativeAction) != 0 ? "X" : nullptr);
    alignText(&FONT_BIG, ancsString.c_str(), ALIGN_NEAR, ALIGN_MIDDLE);
  }

  void drawScreenAMSMediaStateVolumeBar()
  {
    if (amsVolume >= 0)
    {
      const uint16_t barWidth = 10,
                     barHMargin = 2,
                     barVMargins = 30;
      const uint16_t barX = WATCHY_DISPLAY_WIDTH - barWidth - barHMargin;
      const uint16_t barHeight = WATCHY_DISPLAY_HEIGHT - 2 * barVMargins;
      const uint16_t barFillHeight = amsVolume * barHeight;
      display.drawRect(barX, barVMargins, barWidth, barHeight, GxEPD_BLACK);
      display.fillRect(barX + 1, barVMargins + barHeight - barFillHeight, barWidth - 2, barFillHeight, GxEPD_BLACK);
    }
  }

  void drawScreenAMSMediaState()
  {
    drawButtons(
        amsIsCommandAvailable(AMSRemoteCommandIDTogglePlayPause) ? (amsIsPlaying ? "||" : "|>") : nullptr,
        amsIsCommandAvailable(AMSRemoteCommandIDNextTrack) ? ">|" : nullptr,
        amsIsCommandAvailable(AMSRemoteCommandIDVolumeUp) ? "+" : nullptr,
        amsIsCommandAvailable(AMSRemoteCommandIDVolumeDown) ? "-" : nullptr);
    if (amsPlayerName.length() > 0)
      alignText(&FONT_SMALL, amsPlayerName.c_str(), ALIGN_MIDDLE, ALIGN_NEAR);
    drawScreenAMSMediaStateVolumeBar();
    std::string text = amsCheckIsEmpty() ? "Please start playing something on your phone"
                                         : amsTrackTitle + "\n" + amsTrackArtist;
    alignText(&FONT_BIG, text.c_str(), ALIGN_NEAR, ALIGN_MIDDLE);
  }

public:
  void updateDisplay()
  {
    needsUpdate = 0;
    display.init(0, firstDisplay);
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    if (!bleConnected)
      drawScreenNoBLE();
    else if (isShowingANCSNotification())
      drawScreenANCSNotification();
    else
      drawScreenAMSMediaState();

    display.display(!firstDisplay);
    display.hibernate();
    firstDisplay = false;
  };

  WatchyDisplayState(display_t &display) : display(display){};
  ~WatchyDisplayState(){};

  void setConnected(const bool isConnected)
  {
    if (bleConnected == isConnected)
      return;
    bleConnected = isConnected;
    invalidate();
  }

  void clearANCSNotification(uint32_t uuid)
  {
    if (uuid != ancsUUID)
      return;
    ancsUUID = 0;
    ancsShowTimeMS = 0;
    ancsFlags = (ANCS::EventFlags)0;
    ancsString = "";
    // Need to get the screen updated fast because the buttons
    // won't reflect the right action on screen until it does,
    // but their behaviour is immediately changed
    invalidateImmediate();
  }
  void clearANCSNotification() { clearANCSNotification(ancsUUID); }
  void setANCSNotification(const Notification *notification)
  {
    if ((notification->eventFlags & (ANCS::EventFlagPreExisting | ANCS::EventFlagSilent)) != 0)
      return;
    ancsUUID = notification->uuid;
    ancsShowTimeMS = millis() | 1;
    ancsFlags = (ANCS::EventFlags)notification->eventFlags;
    std::string notifType = notification->type;
    if (notifType.length() > 0)
    {
      uint32_t lastDotIndex = notifType.find_last_of('.', notifType.length() - 1);
      if (lastDotIndex != std::string::npos)
        notifType = notifType.substr(lastDotIndex + 1);
    }
    ancsString = notification->title + "\n" + notification->message + "\n" + notifType;
    invalidateImmediate();
  }
  void checkANCSNotificationTimeout()
  {
    if (!isShowingANCSNotification())
      return;
    uint32_t now = millis();
    if ((now - ancsShowTimeMS) > ANCS_NOTIFICATION_DELAY_MS || (now < ancsShowTimeMS))
      clearANCSNotification();
  }

  void addAMSPlayerAttribute(const AMSPlayerAttributeID_t attr, const std::string &value)
  {
    switch (attr)
    {
    case AMSPlayerAttributeIDName:
      amsPlayerName = value;
      break;
    case AMSPlayerAttributeIDPlaybackInfo:
      amsIsPlaying = value[0] == '1';
      break;
    case AMSPlayerAttributeIDVolume:
      amsVolume = (value.length() > 0) ? atof(value.c_str()) : -1;
      return invalidateImmediate();
      break;
    default:
      return;
    }
    invalidate();
  }
  void addAMSTrackAttribute(const AMSTrackAttributeID_t attr, const std::string &value)
  {
    switch (attr)
    {
    case AMSTrackAttributeIDArtist:
      amsTrackArtist = value;
      break;
    case AMSTrackAttributeIDAlbum:
      amsTrackAlbum = value;
      break;
    case AMSTrackAttributeIDTitle:
      amsTrackTitle = value;
      break;
    // case AMSTrackAttributeIDDuration:
    //   break;
    default:
      return;
    }
    invalidate();
  }

  void updateIfNeeded()
  {
    checkANCSNotificationTimeout();
    if (!needsUpdate)
      return;
    uint32_t now = millis();
    if ((now - needsUpdate) > SCREEN_UPDATE_DEBOUNCE_PERIOD_MS || (needsUpdate > now))
      updateDisplay();
  }

  void handleButtonPress(const uint8_t presses)
  {
    if (!bleConnected)
      return;
    if (!presses)
      return;
    if (isShowingANCSNotification())
    {
      if (buttonWasPressed(presses, 0) || buttonWasPressed(presses, 1))
        clearANCSNotification();
      if (buttonWasPressed(presses, 2) && ((ancsFlags & ANCS::EventFlagPositiveAction) != 0))
        notifications.actionPositive(ancsUUID);
      if (buttonWasPressed(presses, 3) && ((ancsFlags & ANCS::EventFlagNegativeAction) != 0))
        notifications.actionNegative(ancsUUID);
    }
    else
    {
      if (buttonWasPressed(presses, 0))
        notifications.amsCommand(AMSRemoteCommandIDTogglePlayPause);
      if (buttonWasPressed(presses, 1))
        notifications.amsCommand(AMSRemoteCommandIDNextTrack);
      if (buttonWasPressed(presses, 2))
        notifications.amsCommand(AMSRemoteCommandIDVolumeUp);
      if (buttonWasPressed(presses, 3))
        notifications.amsCommand(AMSRemoteCommandIDVolumeDown);
    }
  }
};

void WatchyDisplayState::alignTextTest(const GFXfont *font)
{
  alignText(font, "LT", ALIGN_NEAR, ALIGN_NEAR);
  alignText(font, "LM", ALIGN_NEAR, ALIGN_MIDDLE);
  alignText(font, "LB", ALIGN_NEAR, ALIGN_FAR);
  alignText(font, "MT", ALIGN_MIDDLE, ALIGN_NEAR);
  alignText(font, "MM", ALIGN_MIDDLE, ALIGN_MIDDLE);
  alignText(font, "MB", ALIGN_MIDDLE, ALIGN_FAR);
  alignText(font, "RT", ALIGN_FAR, ALIGN_NEAR);
  alignText(font, "RM", ALIGN_FAR, ALIGN_MIDDLE);
  alignText(font, "RB", ALIGN_FAR, ALIGN_FAR);
}

WatchyDisplayState displayState(display);

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
  static String result;
  result = "EventFlags:";
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
  delay(2000);
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
