// --- Full Final Version ---

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "CubeCell_NeoPixel.h"

// --- CONFIGURATION ---
#define LED_PIN    GPIO2
#define NUM_LEDS   8
#define RF_FREQUENCY        915000000
#define TX_OUTPUT_POWER     5
#define LORA_BANDWIDTH      2
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE     1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define BUFFER_SIZE 30
#define PACKET_TIMEOUT_MS 30000    // Timeout to fallback to default pattern (in ms)

#ifndef LoraWan_RGB
#define LoraWan_RGB 0
#endif

// --- OBJECTS ---
CubeCell_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- GLOBAL VARIABLES ---
char rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;
int16_t rssi, rxSize;
bool lora_idle = true;
bool newPacket = false;
bool usingDefaultPattern = false; // Track if fallback pattern is active

uint32_t currentColor;
unsigned long interval = 500;
int currentPattern = -1;
int defaultPattern = 5;            // Pattern 5 = flickerSteady
int currentLED = 0;
int podNumber = 2;
unsigned long syncTime = 0;        // Last packet time

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show();
  currentColor = strip.Color(255, 0, 0); // Default color: red

  syncTime = 0; // Ensure fallback happens on startup

  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  resumeListening();
}

// --- MAIN LOOP ---
void loop() {
  Radio.IrqProcess();

  if (lora_idle) {
    lora_idle = false;
    Serial.println("Listening...");
    resumeListening();
    Radio.IrqProcess();
  }

  unsigned long now = millis();

  // Smart fallback handling
  if ((now - syncTime > PACKET_TIMEOUT_MS) && !usingDefaultPattern) {
    // Timeout reached and not already using fallback
    currentPattern = defaultPattern;
    usingDefaultPattern = true;
    Serial.println("⏳ No packet in timeout — switching to default pattern!");
  }
  else if ((now - syncTime <= PACKET_TIMEOUT_MS) && usingDefaultPattern) {
    // Packet recently received, return to normal
    usingDefaultPattern = false;
    Serial.println("📩 Packet received recently — using normal pattern!");
  }

  // Run the correct pattern
  switch (currentPattern) {
    case 1: syncBlink(); break;
    case 2: alternateBlink(); break;
    case 3: flickerSyncBlink(); break;
    case 4: flickerAlternateBlink(); break;
    case 5: flickerSteady(); break;
    case 6: chaseBlink(); break;
    case 7: reverseChaseBlink(); break;
    case 8: steady(); break;
    default: flickerSteady(); break; // Safe fallback
  }

  Radio.IrqProcess();
}

// --- RADIO AND PACKET HANDLING ---
void resumeListening() {
  Radio.IrqProcess();
  Radio.Standby();
  delay(1);
  Radio.Rx(0); // Start receiving again
  Serial.println("🔁 Requested RX Mode");
  unsigned long start = millis();
  while ((millis() - start) < 10) {
    Radio.IrqProcess();
    delay(1);
  }
  Serial.println("📡 RX Mode should be active");
  Radio.IrqProcess();
  turnOnRGB(0,0); // (assuming you have this function elsewhere)
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssiVal, int8_t snr) {
  Radio.IrqProcess();
  turnOnRGB(COLOR_RECEIVED, 0); // (assuming you have this function elsewhere)

  rssi = rssiVal;
  rxSize = size;
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';

  Serial.printf("\nReceived packet: \"%s\" RSSI: %d Length: %d\n", rxpacket, rssi, rxSize);

  String packet = rxpacket;
  int comma1 = packet.indexOf(",");
  int comma2 = packet.indexOf(",", comma1 + 1);
  if (comma1 == -1 || comma2 == -1) {
    Serial.println("⚠️ Invalid packet format");
    lora_idle = true;
    return;
  }

  String voltage = packet.substring(0, comma1);
  String colorName = packet.substring(comma1 + 1, comma2);
  String patternName = packet.substring(comma2 + 1);

  setColorFromString(colorName);
  setPatternFromString(patternName);

  syncTime = millis();  // Reset timeout clock
  usingDefaultPattern = false; // We have a new packet

  lora_idle = true;
  newPacket = true;
  Radio.IrqProcess();
}

// --- HELPER FUNCTIONS ---

void setColorFromString(String name) {
  name.toLowerCase();
  if (name == "red")       currentColor = strip.Color(255, 0, 0);
  else if (name == "orange") currentColor = strip.Color(255, 75, 0);
  else if (name == "yellow") currentColor = strip.Color(255, 140, 0);
  else if (name == "green")  currentColor = strip.Color(0, 255, 0);
  else if (name == "blue")   currentColor = strip.Color(0, 0, 255);
  else if (name == "purple") currentColor = strip.Color(140, 0, 255);
  else                        currentColor = strip.Color(255, 255, 255); // default white
}

void setPatternFromString(String name) {
  name.toLowerCase();
  if (name == "sync")       { currentPattern = 1; interval = 6000; }
  else if (name == "altn")   { currentPattern = 2; interval = 6000; }
  else if (name == "f/sync") { currentPattern = 3; interval = 6000; }
  else if (name == "f/altn") { currentPattern = 4; interval = 6000; }
  else if (name == "f/stdy") { currentPattern = 5; interval = 6000; }
  else if (name == "chase")  { currentPattern = 6; interval = 150; }
  else if (name == "r/chase"){ currentPattern = 7; interval = 150; }
  else if (name == "stdy")   { currentPattern = 8; interval = 150; }
  else                      { currentPattern = -1; } // unknown
}

// --- PATTERN FUNCTIONS ---

void syncBlink() {
  unsigned long currentTime = millis();
  bool phase = ((currentTime - syncTime) / 500) % 2;

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, phase ? currentColor : strip.Color(0, 0, 0));
  }
  strip.show();
}

void alternateBlink() {
  unsigned long currentTime = millis();
  bool phase = ((currentTime - syncTime + (podNumber % 2 == 0 ? 0 : 500)) / 500) % 2;

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, phase ? currentColor : strip.Color(0, 0, 0));
  }
  strip.show();
}

void flickerSyncBlink() {
  unsigned long currentTime = millis();
  bool flickerPhase = ((currentTime - syncTime) / 500) % 2;

  static bool flickerState = false;
  static unsigned long lastFlickerTime = 0;

  if (flickerPhase) {
    if (currentTime - lastFlickerTime > 50) {
      lastFlickerTime = currentTime;
      flickerState = !flickerState;

      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, flickerState ? currentColor : strip.Color(0, 0, 0));
      }
      strip.show();
    }
  } else {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
  }
}

void flickerAlternateBlink() {
  unsigned long currentTime = millis();
  bool flickerPhase = ((currentTime - syncTime + (podNumber % 2 == 0 ? 0 : 500)) / 500) % 2;

  static bool flickerState = false;
  static unsigned long lastFlickerTime = 0;

  if (flickerPhase) {
    if (currentTime - lastFlickerTime > 50) {
      lastFlickerTime = currentTime;
      flickerState = !flickerState;

      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, flickerState ? currentColor : strip.Color(0, 0, 0));
      }
      strip.show();
    }
  } else {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
  }
}

void steady() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, currentColor);
  }
  strip.show();
}

void flickerSteady() {
  static unsigned long lastFlickerTime = 0;
  static bool state = false;
  unsigned long currentTime = millis();

  if (currentTime - lastFlickerTime >= random(50, 150)) {
    lastFlickerTime = currentTime;
    state = !state;

    uint8_t baseR = (currentColor >> 16) & 0xFF;
    uint8_t baseG = (currentColor >> 8) & 0xFF;
    uint8_t baseB = currentColor & 0xFF;

    for (int i = 0; i < NUM_LEDS; i++) {
      if (state) {
        float scale = random(60, 101) / 100.0;
        strip.setPixelColor(i, strip.Color(baseR * scale, baseG * scale, baseB * scale));
      } else {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
      }
    }
    strip.show();
  }
}

void chaseBlink() {
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - syncTime;

  int currentPod = (elapsed / 250) % 4;

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }

  if (podNumber == (currentPod + 1)) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, currentColor);
    }
  }

  strip.show();
  Radio.IrqProcess();
}

void reverseChaseBlink() {
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - syncTime;

  int currentPod = (3 - ((elapsed / 250) % 4) + 4) % 4;

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }

  if (podNumber == (currentPod + 1)) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, currentColor);
    }
  }

  strip.show();
  Radio.IrqProcess();
}


// ---- END OF FILE ----
