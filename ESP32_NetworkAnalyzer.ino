#include <string.h>
#include <stdlib.h>
#include <vector>
#include "SPI.h"
#include "Adafruit_ILI9341.h"
#include "BluetoothSerial.h"
#include "WiFi.h"

#include "Bitmaps.h"

#define TFT_DC 22
#define TFT_CS 5
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_CLK 18
#define TFT_RST 21

Adafruit_ILI9341 Display = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

BluetoothSerial SerialBT;

class WiFiNetwork{
public:
  String ssid;
  int32_t rssi;
  uint8_t encryptionType;
  uint8_t bssid[6];  // Added BSSID

  WiFiNetwork(String _ssid, int32_t _rssi, uint8_t _encryptionType)
    : ssid(_ssid), rssi(_rssi), encryptionType(_encryptionType) {}

  bool operator==(const WiFiNetwork& other) const {
    return (ssid == other.ssid) && (encryptionType == other.encryptionType);
  }
};

std::vector<WiFiNetwork> networks_list;
std::vector<WiFiNetwork> previous_networks;

enum Mode {NETWORK_SELECTION_MODE, NETWORK_EXACT_MODE, SIGNAL_STRENGTH_MODE, WAIT_MODE};
Mode currentMode = NETWORK_SELECTION_MODE;

int rssi_state = -1;
int prev_rssi_state = -11;

int selectedNetworkIndex = -1; 
int rssiThreshold = -72;

String selectedSSID = "NULL";

void DisplayNetworkInfo(const WiFiNetwork &net);
void DisplayNetworks();
void DisplaySignalStrength(const WiFiNetwork &net);
int findNetworkIndex(String ssid);

void setup() {
  SPISettings settings(40000000, MSBFIRST, SPI_MODE0);  // Example: 24 MHz SPI clock
  SPI.beginTransaction(settings);
  
  SerialBT.begin("ESP32");

  Display.begin();

  Display.setSPISpeed(40000000);

  ClearScreen();
}

void loop() {
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();

    if (command.startsWith("#")) {
      HandleCommand(command.substring(1));
    }
  }

  switch (currentMode) {
    case NETWORK_SELECTION_MODE:
      ScanNetworks();
      if (networks_list != previous_networks) {
        DisplayNetworks();
      }
      break;
    case SIGNAL_STRENGTH_MODE:
      ScanNetworks();
      DisplaySignalStrength(networks_list[selectedNetworkIndex]);
      break;
    case NETWORK_EXACT_MODE:
      ScanNetworks();
      DisplayNetworkInfo(networks_list[selectedNetworkIndex]);
      break;
    case WAIT_MODE:
      yield();
      delay(100);
      break;
  }
}

void HandleCommand(String command){
  if (command.startsWith("selectnetwork:")){
    selectedSSID = command.substring(14);
    selectedNetworkIndex = findNetworkIndex(selectedSSID);
    if (selectedNetworkIndex != -1){
      currentMode = NETWORK_EXACT_MODE;
      ClearScreen();
    }
  } else if (command == "signalstrength"){
    if (selectedNetworkIndex != -1){
      currentMode = SIGNAL_STRENGTH_MODE; 
      ClearScreen();
    }
  } else if (command == "exit"){
    currentMode = NETWORK_SELECTION_MODE;
    ClearScreen();
    DisplayNetworks(); 
  }
}

void ClearScreen(){
  yield();
  Display.fillScreen(ILI9341_BLACK);

  Display.setCursor(0, 0);
  Display.setTextColor(ILI9341_WHITE);
  Display.setTextSize(2);
}

void ScanNetworks() {
  int numNetworks = WiFi.scanNetworks();

  previous_networks = networks_list;

  networks_list.clear();

  for (int i = 0; i < numNetworks; i++) {
    int rssi = WiFi.RSSI(i);

    if (rssi >= rssiThreshold) {
      uint8_t bssid[6];
      memcpy(bssid, WiFi.BSSID(i), 6);

      WiFiNetwork network(WiFi.SSID(i), rssi, WiFi.encryptionType(i));
      memcpy(network.bssid, bssid, 6);

      networks_list.push_back(network);
    }
  }
}

void DisplayNetworks() {
  ClearScreen();

  for (size_t i = 0; i < networks_list.size(); i++) {
    Display.print(F("Network "));
    Display.print(i);
    Display.print(F(": "));
    Display.println(networks_list[i].ssid);
  }
}

void DisplayNetworkInfo(const WiFiNetwork &net) {
  if(selectedSSID != net.ssid){
    selectedNetworkIndex = findNetworkIndex(selectedSSID);
  }
  
  Display.fillRect(0, 0, 250, 120, ILI9341_BLACK);
  
  Display.setTextSize(2);
  Display.setCursor(0, 0);

  Display.print("SSID: ");
  Display.println(net.ssid);
  Display.print("RSSI: ");
  Display.println(net.rssi);
  Display.print("Encryption: ");
  Display.println(getEncryptionTypeString(net.encryptionType));
  Display.print("MAC Address: ");
  Display.println("");
  for (int i = 0; i < 6; i++) {
    Display.print(String(net.bssid[i], HEX));
    if (i < 5) Display.print(":");
  }
  Display.println();
  Display.print("Channel: ");
  Display.println(WiFi.channel(findNetworkIndex(net.ssid)));
}

void DisplaySignalStrength(const WiFiNetwork &net) {
  int rssi_value = net.rssi;

   if (rssi_value <= -70) {
      rssi_state = 0;
    } else if (rssi_value > -70 && rssi_value <= -60) {
      rssi_state = 1;
    } else if (rssi_value > -60 && rssi_value <= -50) {
      rssi_state = 2;
    } else {
      rssi_state = 3;
    }

  if (rssi_state != prev_rssi_state) {
    ClearScreen();

    if (rssi_state == 0) {
      Display.drawRGBBitmap(9, 0, WiFi_Bad_Signal, 220, 220);
    } else if (rssi_state == 1) {
      Display.drawRGBBitmap(9, 0, WiFi_Fair_Signal, 220, 220);
    } else if (rssi_state == 2) {
      Display.drawRGBBitmap(9, 0, WiFi_Good_Signal, 220, 220);
    } else if (rssi_state == 3) {
      Display.drawRGBBitmap(9, 0, WiFi_Excellent_Signal, 220, 220);
    }

    Display.setTextSize(4);
    Display.setCursor(15, 250);
    Display.print("RSSI: ");
    Display.print(rssi_value);

    prev_rssi_state = rssi_state;
  }else{

    Display.fillRect(0, 250, 250, 30, ILI9341_BLACK);

    Display.setTextSize(4);
    Display.setCursor(15, 250);
    Display.print("RSSI: ");
    Display.print(rssi_value);
  }
}


int findNetworkIndex(String ssid) {
  for (size_t i = 0; i < networks_list.size(); i++) {
    if (networks_list[i].ssid == ssid) {
      return i;
    }
  }
  return -1;  // Network not found
}

String getEncryptionTypeString(uint8_t encryptionType) {
  switch (encryptionType) {
    case 2:  // ENC_TYPE_WEP
      return "WEP";
    case 3:  // ENC_TYPE_TKIP
      return "WPA/TKIP";
    case 4:  // ENC_TYPE_CCMP
      return "WPA2/CCMP";
    case 7:  // ENC_TYPE_NONE
      return "Open";
    case 8:  // ENC_TYPE_AUTO
      return "Auto";
    default:
      return "Unknown";
  }
}
