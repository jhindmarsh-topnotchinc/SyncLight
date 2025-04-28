# SyncLight
Heltec Cubecell transmitter and receiver for synchronized LED beacons

## Transmitter is Heltec Cubecell HTCC-AB02S with GPS:
  Runs an internal clock to send a LoRa packet every 5 seconds.  When GPS is available, PPS is utilized to count to 5 for a packet transmit.
  Button is attached to control transmit parameters:
    Short press changes blink pattern sent,
    Long press changes color sent.

## Receiver is either Cubecell Board V1 or V2 (HTCC-AB01)
  Utilizes Cubecell NeoPixel library to control a strip of 8 WS2812B LEDS.
  Runs a default pattern and color on startup.
  Resets pattern each time a packet is received ensuring a consistent LED sync between devices (pods).
  Design assumes there will be 4 pods for chase sequences.
  Defaults to a specific pattern if a new packet is not received within 30 seconds after last packet received.
  
