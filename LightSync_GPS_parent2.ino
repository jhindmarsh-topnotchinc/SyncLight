#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "GPS_Air530Z.h"

SSD1306Wire OLEDdisplay(0x3c, 500000, SDA, SCL, GEOMETRY_128_64, GPIO10);

#ifndef LoraWan_RGB
#define LoraWan_RGB 0
#endif

// LoRa parameters
#define RF_FREQUENCY          915000000 // Hz
#define TX_OUTPUT_POWER       5         // dBm
#define LORA_BANDWIDTH        2         // 500 kHz
#define LORA_SPREADING_FACTOR 7         // SF7
#define LORA_CODINGRATE       1         // 4/5
#define LORA_PREAMBLE_LENGTH  8
#define LORA_SYMBOL_TIMEOUT   0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON  false
#define RX_TIMEOUT_VALUE      1000
#define BUFFER_SIZE           30        // Payload size
#define gpsPPSpin             GPIO12

Air530ZClass GPS;
char txPacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;

// Global sync toggles for c/SYNC patterns
static unsigned long cSyncLastToggle = 0;
static bool cSyncToggle = false;

typedef enum {
    LOWPOWER,
    ReadVoltage,
    TX
} States_t;

States_t state;
bool sleepMode = false;
int16_t rssi, rxSize;
uint16_t voltage;
int counter = 0;
bool gpsSync = false;
uint32_t starttime;
int gpsLEDstate = 0;

const uint32_t GPS_UPDATE_TIMEOUT_MS = 1000;

int fracPart(double val, int n) {
  return (int)((val - (int)(val))*pow(10,n));
}

const int buttonPin = GPIO7;
const unsigned long debounceDelay = 50;
const unsigned long longPressTime = 1000;

bool buttonState = LOW;
bool lastReading = LOW;
unsigned long lastDebounceTime = 0;

bool buttonPressed = false;
unsigned long pressStartTime = 0;

// Color cycling setup
const char* colors[] = {"Red", "Orange", "Yellow", "Green", "Blue", "Purple", "White"};
const int colorCount = sizeof(colors) / sizeof(colors[0]);
int currentColorIndex = 0;
// Pattern cycling setup
const char* pattern[] = {"SYNC", "ALTN", "f/SYNC", "f/ALTN", "f/STDY", "CHASE", "R/CHASE", "STDY", "c/SYNC", "c/f/SYNC"};
const int patternCount = sizeof(pattern) / sizeof(pattern[0]);
int currentPatternIndex = 0;

// Utility Functions
void VextON() {
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);
}

void VextOFF() {
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, HIGH);
}

void printGPSInfo() {
    Serial.print("LAT: "); Serial.print(GPS.location.lat(), 6);
    Serial.print(", LON: "); Serial.print(GPS.location.lng(), 6);
    Serial.print(", ALT: "); Serial.print(GPS.altitude.meters());
    Serial.print(", AGE: "); Serial.print(GPS.location.age());
    Serial.print(", SATS: "); Serial.println(GPS.satellites.value());
}

void updateGPSNonBlocking() {
  static uint32_t lastGPSEncode = 0;
  if (millis() - lastGPSEncode >= 200) {
    while (GPS.available()) {
      GPS.encode(GPS.read());
    }
    lastGPSEncode = millis();
  }
}

void setup() {
    Serial.begin(115200);
    GPS.begin();
    Serial.println("\n\nStarting...");

    VextON();
    delay(100);
    OLEDdisplay.init();
    OLEDdisplay.clear();
    OLEDdisplay.display();
  
    OLEDdisplay.setTextAlignment(TEXT_ALIGN_CENTER);
    OLEDdisplay.setFont(ArialMT_Plain_16);
    OLEDdisplay.drawString(64, 32-16/2, "SyncLight");
    OLEDdisplay.display();

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                      true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

    state = ReadVoltage;
    voltage = 0;
    rssi = 0;
    starttime = millis();
    pinMode(gpsPPSpin, INPUT);
    pinMode(buttonPin, INPUT);
    Serial.print("Current color: ");
    Serial.println(colors[currentColorIndex]);
    Serial.print("Current pattern: ");
    Serial.println(pattern[currentPatternIndex]);
    delay(3000);
}

void loop() {
    updateGPSNonBlocking();
    bool gpsPPS = digitalRead(gpsPPSpin);
    if (gpsPPS && gpsLEDstate == 0) {
        gpsLEDstate = 1;
        gpsSync = true;
        counter++;
        Serial.print("GPS PPS "); Serial.println(counter);
        printGPSInfo();
    } else if (!gpsPPS && gpsSync) {
        gpsLEDstate = 0;
    } else if (!gpsPPS && !gpsSync && (millis() - starttime) > 1000) {
        counter++;
        Serial.print("Internal clock "); Serial.println(counter);
        printGPSInfo();
        starttime = millis();
    }

    if (counter > 4) {
        state = TX;
        counter = 0;
        starttime = millis();
    }

    char str[30];
    OLEDdisplay.clear();
    OLEDdisplay.setFont(ArialMT_Plain_10);
    int index = sprintf(str,"%02d-%02d-%02d",GPS.date.year(),GPS.date.day(),GPS.date.month());
    str[index] = 0;
    OLEDdisplay.setTextAlignment(TEXT_ALIGN_LEFT);
    OLEDdisplay.drawString(0, 0, str);
  
    index = sprintf(str,"%02d:%02d:%02d",GPS.time.hour(),GPS.time.minute(),GPS.time.second());
    str[index] = 0;
    OLEDdisplay.drawString(60, 0, str);
    
    if (GPS.location.age() < GPS_UPDATE_TIMEOUT_MS) {
        OLEDdisplay.drawString(120, 0, "A");
        gpsSync = true;
    } else {
        OLEDdisplay.drawString(120, 0, "V");
        gpsSync = false;
    }

    index = sprintf(str,"alt: %d.%d",(int)GPS.altitude.meters(),fracPart(GPS.altitude.meters(),2));
    str[index] = 0;
    OLEDdisplay.drawString(5, 16, str);

    OLEDdisplay.drawString(5, 32, "sat: " + String(GPS.satellites.value()));
 
    index = sprintf(str,"lat :  %d.%d",(int)GPS.location.lat(),fracPart(GPS.location.lat(),4));
    str[index] = 0;
    OLEDdisplay.drawString(60, 16, str);   
  
    index = sprintf(str,"lon:%d.%d",(int)GPS.location.lng(),fracPart(GPS.location.lng(),4));
    str[index] = 0;
    OLEDdisplay.drawString(60, 32, str);

    OLEDdisplay.drawString(5, 49, (colors[currentColorIndex]));
    OLEDdisplay.drawString(60, 49, (pattern[currentPatternIndex]));
    OLEDdisplay.drawRect(0, 48, 128, 16);
    OLEDdisplay.display();

    bool reading = digitalRead(buttonPin);

    // Debounce
    if (reading != lastReading) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == HIGH) {
                pressStartTime = millis();
                buttonPressed = true;
            } else if (buttonPressed) {
                unsigned long pressDuration = millis() - pressStartTime;
                if (pressDuration >= longPressTime) {
                    currentColorIndex = (currentColorIndex + 1) % colorCount;
                    Serial.print("Long press detected - New color: ");
                    Serial.println(colors[currentColorIndex]);
                } else {
                    currentPatternIndex = (currentPatternIndex + 1) % patternCount;
                    Serial.print("Short press detected - New pattern: ");
                    Serial.println(pattern[currentPatternIndex]);
                }
                buttonPressed = false;
            }
        }
    }
    lastReading = reading;

    switch (state) {
        case TX:
            // Handle c/SYNC and c/f/SYNC toggling
            if (strcmp(pattern[currentPatternIndex], "c/SYNC") == 0) {
                if (millis() - cSyncLastToggle >= 10000) {
                    cSyncToggle = !cSyncToggle;
                    cSyncLastToggle = millis();
                }
                snprintf(txPacket, BUFFER_SIZE, "%d,%s,%s", voltage, colors[currentColorIndex], cSyncToggle ? "ALTN" : "SYNC");
            } else if (strcmp(pattern[currentPatternIndex], "c/f/SYNC") == 0) {
                if (millis() - cSyncLastToggle >= 10000) {
                    cSyncToggle = !cSyncToggle;
                    cSyncLastToggle = millis();
                }
                snprintf(txPacket, BUFFER_SIZE, "%d,%s,%s", voltage, colors[currentColorIndex], cSyncToggle ? "f/ALTN" : "f/SYNC");
            } else {
                snprintf(txPacket, BUFFER_SIZE, "%d,%s,%s", voltage, colors[currentColorIndex], pattern[currentPatternIndex]);
            }
            turnOnRGB(COLOR_SEND, 0);
            Serial.printf("\nSending packet \"%s\", length %d\n", txPacket, strlen(txPacket));
            Radio.Send((uint8_t *)txPacket, strlen(txPacket));
            state = LOWPOWER;
            return;

        case LOWPOWER:
            state = ReadVoltage;
            break;

        case ReadVoltage:
            pinMode(VBAT_ADC_CTL, OUTPUT);
            digitalWrite(VBAT_ADC_CTL, LOW);
            voltage = analogRead(ADC);
            pinMode(VBAT_ADC_CTL, INPUT);
            break;

        default:
            break;
    }

    Radio.IrqProcess();
}

void OnTxDone() {
    Serial.println("TX done!");
    turnOnRGB(0,0);
    turnOffRGB();
}

void OnTxTimeout() {
    Radio.Sleep();
    Serial.println("TX Timeout...");
    state = ReadVoltage;
}
