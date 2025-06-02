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

#define USE_MONO true // Use mono audio

char* DEVICE_NAME = "Rams RT-20 Radio";

enum class MODE
{
  OFF,
  BLUETOOTH,
  RADIO,
  INVALID
};

string CHANNELS[4] = {
    "https://stream-relay-geo.ntslive.net/stream",
    "https://stream-relay-geo.ntslive.net/stream2",
    "https://stream-relay-geo.ntslive.net/stream3",
    "https://stream-mixtape-geo.ntslive.net/mixtape"};

class Radio
{
private:
  int _volumePercentage = 100;
  char *_accessPointName;
  WiFiManager _wifiManager;
  bool _hasWifiConnection;
  MODE _currentMode;

  Audio _radioAudio;
  string _playingRadioStream;

  btAudio _bluetoothAudio = btAudio("ESP32 Bluetooth Device");

  int _BCLK_PIN, _LRC_PIN, _DOUT_PIN;

  void _initialiseBluetooth(bool reconnect = false)
  {
    _bluetoothAudio.begin();
    if (reconnect)
    {
      _bluetoothAudio.reconnect(); // Re-connects to last connected device
    }
    _bluetoothAudio.I2S(_BCLK_PIN, _DOUT_PIN, _LRC_PIN);
    _bluetoothAudio.volume(_volumePercentage / 100.0f);
    _currentMode = MODE::BLUETOOTH;
  }
  void _stopBluetooth()
  {
    Serial.println("Stopping bluetooth");
  }

  void _startRadio()
  {
    _radioAudio.setPinout(_BCLK_PIN, _LRC_PIN, _DOUT_PIN);
    _radioAudio.forceMono(USE_MONO); // Force mono for single speaker;
    _radioAudio.setVolume(_volumePercentage);
    _currentMode = MODE::RADIO;
    Serial.println("Starting radio");
  }

  void _stopRadio()
  {
    if (_radioAudio.isRunning())
    {
      _radioAudio.stopSong();
    }
  }

public:
  Radio(char *accessPointName, int BCLK_PIN, int LRC_PIN, int DOUT_PIN)
  {

    _BCLK_PIN = BCLK_PIN;
    _LRC_PIN = LRC_PIN;
    _DOUT_PIN = DOUT_PIN;

    _currentMode = MODE::OFF;
    _accessPointName = accessPointName;
    _bluetoothAudio = btAudio(accessPointName);

    _wifiManager.setConfigPortalTimeout(180);
    _hasWifiConnection = _wifiManager.autoConnect(accessPointName);
  }

  bool playRadioStream(string newChannelStream)
  {
    if (_playingRadioStream == newChannelStream)
    {
      Serial.println("Already connected to stream");
      return true;
    }

    if (!_hasWifiConnection)
    {
      Serial.println("Cannot play stream as no valid wifi connection");
      return false;
    }

    if (_currentMode == MODE::BLUETOOTH)
    {
      _stopBluetooth();
    }

    _startRadio();
    Serial.println("Connecting to new stream:");
    Serial.println(newChannelStream.c_str());
    return _radioAudio.connecttohost(newChannelStream.c_str());
  }

  void playBluetooth()
  {
    if (_currentMode == MODE::BLUETOOTH)
    {
      return;
    }
    if (_currentMode == MODE::RADIO)
    {
      _stopRadio();
    }
    _initialiseBluetooth(true);
  }

  void enableBluetoothPairingMode()
  {
    if (_bluetoothAudio.hasClient)
    {
      _bluetoothAudio.disconnect();
      _bluetoothAudio.end();
    }
    _initialiseBluetooth(false);
  }

  void enableWifiPairingMode()
  {
    _hasWifiConnection = _wifiManager.startConfigPortal(_accessPointName);
  }

  bool hasWifiConnection()
  {
    return _hasWifiConnection;
  }

  void setVolume(int newVolumePercentage)
  {
    if (newVolumePercentage == _volumePercentage)
    {
      return;
    }
    _volumePercentage = newVolumePercentage;

    if (_currentMode == MODE::BLUETOOTH)
    {
      _bluetoothAudio.volume(_volumePercentage / 100.0f);
    }
    else if (_currentMode == MODE::RADIO)
    {
      _radioAudio.setVolume(_volumePercentage);
    }
  }

  void stop()
  {
    if (_currentMode == MODE::RADIO)
    {
      _stopRadio();
    }
    else if (_currentMode == MODE::BLUETOOTH)
    {
      _stopBluetooth();
    }
    _currentMode == MODE::OFF;
  }

  void loop()
  {
    if (_currentMode == MODE::RADIO)
    {
      _radioAudio.loop();
    }
    vTaskDelay(1);
  }
};

void checkButtons() {}

void checkPots() {}

Radio radio(DEVICE_NAME, I2S_BCLK, I2S_LRC, I2S_DOUT);

void setup()
{
}

void loop()
{

  checkButtons(); // sweeps the buttons for presses
  checkPots();    // sweeps the pots for new values
  radio.loop();
}
