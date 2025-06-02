#include "Arduino.h"
#include "Audio.h"
#include <btAudio.h>
#include <WiFiManager.h>
#include <ezButton.h>

using namespace std;

// ESP32 I2S digital output pins
#define I2S_DOUT 25 // GPIO 25 (DATA Output - the digital output. connects to DIN pin on I2S DAC)
#define I2S_BCLK 26 // GPIO 26 (CLOCK Output - serial clock. connects to BCLK pin on  I2S DAC)
#define I2S_LRC 27  // GPIO 27 (SELECT Output - left/right control. connects to LRC/LCK/WS/WSEL pin on I2S DAC)

string deviceName = "Rams RT-20 Radio";

btAudio bluetoothAudio = btAudio(deviceName.c_str());
Audio radioAudio;

enum MODE
{
  BLUETOOTH,
  RADIO,
  INVALID
};

MODE currentMode = RADIO;
bool useMono = true;

int volumePercentage = 100;
int currentRadioChannel = 1;

string channels[4] = {
    "https://stream-relay-geo.ntslive.net/stream",
    "https://stream-relay-geo.ntslive.net/stream2",
    "https://stream-relay-geo.ntslive.net/stream3",
    "https://stream-mixtape-geo.ntslive.net/mixtape"};

WiFiManager wifiManager;
bool hasWifiConnection = false;



void setVolume()
{
  if (currentMode == BLUETOOTH)
  {
    bluetoothAudio.volume(volumePercentage / 100.0f);
  }
  else if (currentMode == RADIO)
  {
    radioAudio.setVolume(volumePercentage);
  }
}

void enterRadioMode()
{
  if (currentMode == RADIO)
  {
    return;
  }

  radioAudio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  radioAudio.forceMono(useMono); // Force mono for single speaker;
  setVolume();
  currentMode = RADIO;
}

void exitRadioMode()
{
  // radioAudio.setPinout(I2S_LRC + 1, I2S_LRC + 2, I2S_LRC + 3); // move off the I2S pins;
  currentMode = INVALID;
}

void enterBluetoothMode()
{
  if (currentMode == BLUETOOTH)
  {
    return;
  }
  bluetoothAudio.begin();
  bluetoothAudio.reconnect(); // Re-connects to last connected device
  bluetoothAudio.I2S(I2S_BCLK, I2S_DOUT, I2S_LRC);
  setVolume();
  currentMode = BLUETOOTH;
}

void disconnectBluetooth()
{
  if (currentMode != BLUETOOTH)
  {
    return;
  }
  if (bluetoothAudio.hasClient)
  {
    bluetoothAudio.disconnect();
  }
}

void exitBluetoothMode()
{
  if (currentMode != BLUETOOTH)
  {
    return;
  }
  bluetoothAudio.end();
  currentMode = INVALID;
}

bool changeMode(MODE newMode)
{
  if (currentMode == newMode)
  {
    return true;
  }
  // exit current mode
  if (currentMode == BLUETOOTH)
  {
    disconnectBluetooth();
    exitBluetoothMode();
  }
  else if (currentMode == RADIO)
  {
    exitRadioMode();
  }
  // enter new mode
  if (newMode == BLUETOOTH)
  {
    enterBluetoothMode();
  }
  else if (newMode == RADIO)
  {
    if (!hasWifiConnection)
    {
      return false;
    }
    enterRadioMode();
  }

  return true;
}

void connectToChannel(int newChannel)
{
  if (currentMode == RADIO && currentRadioChannel == newChannel)
  {
    return;
  }

  if (changeMode(RADIO))
  {
    currentRadioChannel = newChannel;
    radioAudio.connecttohost(channels[currentRadioChannel].c_str());
  }
}


void checkButtons() {}

void checkPots() {}

void loopRadio()
{
  radioAudio.loop();
  vTaskDelay(1);
}

void setup()
{
  wifiManager.setConfigPortalTimeout(180);
  hasWifiConnection = wifiManager.autoConnect(deviceName.c_str());

  if (!hasWifiConnection)
  {
    changeMode(BLUETOOTH);
  }
  else{
    connectToChannel(currentRadioChannel);
  }
}

void loop()
{

  checkButtons(); // sweeps the buttons for presses
  checkPots();    // sweeps the pots for new values

  if (currentMode == RADIO)
  {
    loopRadio();
  }
}
