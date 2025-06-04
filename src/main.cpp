#include "Arduino.h"
#include "Audio.h"
#include <btAudio.h>
#include <WiFiManager.h>
#include <ezButton.h>

// ESP32 I2S digital output pins
#define I2S_DOUT 25 // GPIO 25 (DATA Output - the digital output. connects to DIN pin on I2S DAC)
#define I2S_BCLK 26 // GPIO 26 (CLOCK Output - serial clock. connects to BCLK pin on  I2S DAC)
#define I2S_LRC 27  // GPIO 27 (SELECT Output - left/right control. connects to LRC/LCK/WS/WSEL pin on I2S DAC)

#define USE_MONO true // Use mono audio

ezButton RADIO_CHANNEL_0_BUTTON = ezButton(18);
// ezButton RADIO_CHANNEL_1_BUTTON = ezButton(11);
// ezButton RADIO_CHANNEL_3_BUTTON = ezButton(12);
// ezButton WIFI_RESET_BUTTON = ezButton(13);
ezButton BLUETOOTH_BUTTON = ezButton(19);

std::string DEVICE_NAME = "Rams RT-20 Radio";

enum class MODE
{
  OFF,
  BLUETOOTH,
  RADIO,
  INVALID
};

std::string CHANNELS[4] = {
    "https://stream-relay-geo.ntslive.net/stream",
    "https://stream-relay-geo.ntslive.net/stream2",
    "https://stream-relay-geo.ntslive.net/stream3",
    "https://stream-mixtape-geo.ntslive.net/mixtape"};

WiFiManager wifiManager;
bool hasWifiConnection = false;

bool connectToWifi()
{
  if (hasWifiConnection)
  {
    return true;
  }
  esp_err_t results = esp_wifi_start();
  wifiManager.setConfigPortalTimeout(180);
  hasWifiConnection = wifiManager.autoConnect(DEVICE_NAME.c_str());
  return hasWifiConnection;
}

bool disconnectFromWifi()
{

  if (!hasWifiConnection)
  {
    return true;
  }
  wifiManager.disconnect();
  esp_err_t results = esp_wifi_stop();
  hasWifiConnection = false;

  return hasWifiConnection;
  Serial.println("Disconnected");
  return true;

  // directly call to disable the wifi's control of the radio;
  bool successfultDisconnect = wifiManager.disconnect();
  if (successfultDisconnect)
  {
    WiFi.mode(WIFI_OFF);
    Serial.println("Disconnected");
    hasWifiConnection = !successfultDisconnect;
  }
  return successfultDisconnect;
}

class Radio
{
private:
  int _volumePercentage = 100;

  int _targetVolumePercentage = 100; // used for fades;

  MODE _currentMode;

  Audio _radioAudio;
  int _playingRadioStreamIndex = -1;

  btAudio _bluetoothAudio = btAudio(DEVICE_NAME.c_str());

  bool _startBluetooth(bool reconnect = false)
  {

    if (_currentMode == MODE::BLUETOOTH)
    {
      return true;
    }

    // disconnect the wifi on bluetooth to free the raido;
    if (hasWifiConnection)
    {
      disconnectFromWifi();
      delay(2000);
    }

    Serial.println("Starting bluetooth...");
    _bluetoothAudio.begin();
    Serial.println("Bluetooth stated");
    if (reconnect)
    {
      Serial.println("Reconnecting to last device...");
      _bluetoothAudio.reconnect(); // Re-connects to last connected device
    }
    _bluetoothAudio.I2S(I2S_BCLK, I2S_DOUT, I2S_LRC);
    _bluetoothAudio.volume(_volumePercentage / 100.0f);
    _currentMode = MODE::BLUETOOTH;
    return true;
  }
  void _stopBluetooth()
  {
    Serial.println("Stopping bluetooth");
    _bluetoothAudio.disconnect();
    delay(500);
    _bluetoothAudio.end();
    delay(1000);
  }

  bool _startRadio()
  {
    if (!connectToWifi())
    {
      Serial.println("Failed to connect to wifi");
      return false;
    }
    if (_currentMode == MODE::RADIO)
    {
      return true;
    }
    _radioAudio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    _radioAudio.forceMono(USE_MONO); // Force mono for single speaker;
    _radioAudio.setTone(20, 10, 0);
    _radioAudio.setVolume(_volumePercentage);
    _currentMode = MODE::RADIO;

    Serial.println("Starting radio");
    return true;
  }

  void _stopRadio()
  {
    _playingRadioStreamIndex = -1;
    _radioAudio.stopSong();
    // i2s_stop((i2s_port_t)_radioAudio.getI2sPort());
    _currentMode = MODE::OFF;
  }

public:
  Radio() {}

  bool playRadioStream(int newChannelStreamIndex)
  {

    if (_playingRadioStreamIndex == newChannelStreamIndex)
    {
      Serial.println("Already connected to stream");
      return true;
    }

    if (_currentMode == MODE::BLUETOOTH)
    {
      _stopBluetooth();
    }

    if (!_startRadio())
    {
      return false;
    }

    Serial.println("Connecting to new stream:");
    Serial.println(CHANNELS[newChannelStreamIndex].c_str());
    _radioAudio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de"); //
    bool isConnected = _radioAudio.connecttohost(CHANNELS[newChannelStreamIndex].c_str());
    _playingRadioStreamIndex = newChannelStreamIndex;
    return isConnected;
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
    _startBluetooth(true);
    if (_bluetoothAudio.hasClient)
    {
    }
  }

  void enableBluetoothPairingMode()
  {
    if (_bluetoothAudio.hasClient)
    {
      _bluetoothAudio.disconnect();
      _bluetoothAudio.end();
    }
    _startBluetooth(false);
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
    if (_targetVolumePercentage > _volumePercentage)
    {
      setVolume(_volumePercentage++);
    }
    else if (_targetVolumePercentage < _volumePercentage)
    {
      setVolume(_volumePercentage--);
    }

    if (_currentMode == MODE::RADIO)
    {
      _radioAudio.loop();
    }
    vTaskDelay(1);
  }
};

Radio radio;

void checkButtons()
{
  RADIO_CHANNEL_0_BUTTON.loop();
  BLUETOOTH_BUTTON.loop();

  if (RADIO_CHANNEL_0_BUTTON.isPressed())
  {
    radio.playRadioStream(0);
  }
  else if (BLUETOOTH_BUTTON.isPressed())
  {
    radio.playRadioStream(1);
  }
}

void checkPots()
{
  // vTaskDelay(1);
  //  Serial.println("Check Pots");
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Boot");
  // connectToWifi();

  RADIO_CHANNEL_0_BUTTON.setDebounceTime(100); // set debounce time to 50 milliseconds
  BLUETOOTH_BUTTON.setDebounceTime(100);       // set debounce time to 50 milliseconds
}

void loop()
{

  checkButtons(); // sweeps the buttons for presses
  checkPots();    // sweeps the pots for new values
  radio.loop();
}
