/*
   LoRaLib Transmit Example - configured to be used with the receiver
   in this project. 

   This example transmits LoRa packets with one second delays
   between them. Each packet contains up to 256 bytes
   of data, in the form of:
    - Arduino String
    - null-terminated char array (C-string)
    - arbitrary binary data (byte array)

   For more detailed information, see the LoRaLib Wiki
   https://github.com/jgromes/LoRaLib/wiki

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/LoRaLib/
*/

// include the library
#include <LoRaLib.h>

// create instance of LoRa class using SX1278 module
// this pinout corresponds to RadioShield
// https://github.com/jgromes/RadioShield
// NSS pin:   10 (4 on ESP32/ESP8266 boards)
// DIO0 pin:  2
// DIO1 pin:  3

#define DIO0 5  // GPIO5
#define NSS 15  // GPIO15 = CHIP SELECT
#define DIO1 16 // GPIO16
SX1278 lora = new LoRa(NSS, DIO0, DIO1);

static unsigned long pktNum = 0;

void setup() {
  Serial.begin(115200);

  // initialize SX1278 with default settings
  Serial.print(F("Initializing ... "));
  // carrier frequency:           434.0 MHz
  // bandwidth:                   125.0 kHz
  // spreading factor:            7
  // coding rate:                 4/5
  // sync word:                   0x34
  // output power:                17 dBm
  // current limit:               100 mA
  // preamble length:             8 symbols
  // amplifier gain:              0 (automatic gain control)
  // int state = lora.begin();

  int state = lora.begin(434.0, 125.0, 7, 5, 0x34); // freq 434.0mhz bw 125khz sf 7 cr 5 sync-word 0x34 = lorawan
  if (state == ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
}

void loop() {
  String s = "Hello World! #";
  s += pktNum++;
  
  Serial.print(F("Sending packet ... "));
  Serial.println(s);

  // you can transmit C-string or Arduino string up to
  // 256 characters long
  // NOTE: transmit() is a blocking method!
  //       See example TransmitInterrupt for details
  //       on non-blocking transmission method.
  
  int state = lora.transmit(s);

  // you can also transmit byte array up to 256 bytes long
  /*
    size_t len = 8;
    byte byteArr[len] = {0x01, 0x23, 0x45, 0x56,
                         0x78, 0xAB, 0xCD, 0xEF};
    int state = lora.transmit(byteArr, len);
  */

  if (state == ERR_NONE) {
    // the packet was successfully transmitted
    Serial.println(F(" success!"));

    // print measured data rate
    Serial.print(F("Datarate:\t"));
    Serial.print(lora.getDataRate());
    Serial.println(F(" bps"));

  } else if (state == ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 256 bytes
    Serial.println(F(" too long!"));

  } else if (state == ERR_TX_TIMEOUT) {
    // timeout occurred while transmitting packet
    Serial.println(F(" timeout!"));

  }

  // wait a second before transmitting again
  delay(1000);
}
