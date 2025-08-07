/**
 * @file R4-LoRa-Receiver-ULTRA-FAST.ino
 * @brief MAXIMUM SPEED receiver for 5m range - Ultra-low latency
 */

//==============================================================================
// Includes & Definitions
//==============================================================================
#include <SPI.h>
#include <LoRa.h>

// LoRa Pinout for Arduino R4
#define SS_PIN    10
#define RST_PIN   9
#define DIO0_PIN  2

//==============================================================================
// Minimal Data Packet (Must match sender exactly)
//==============================================================================
struct __attribute__((packed)) FastOrientationPacket {
  float roll, pitch, yaw;
  float quat_w, quat_x, quat_y, quat_z;
  uint16_t counter;
};

//==============================================================================
// Global variables - Minimal for speed
//==============================================================================
FastOrientationPacket packet;
unsigned long lastReceiveTime = 0;
uint16_t lastCounter = 0;
unsigned long packetCount = 0;

//==============================================================================
// Setup - Ultra-fast mode
//==============================================================================
void setup() {
  Serial.begin(230400); // Faster serial output
  Serial.println("ULTRA-FAST LoRa Receiver - 5m Range Mode");

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa FAILED!");
    while (1);
  }
  
  // EXACT same settings as sender
  LoRa.setSpreadingFactor(6);      // FASTEST
  LoRa.setSignalBandwidth(500E3);  // WIDEST
  LoRa.setCodingRate4(5);          // LEAST error correction
  LoRa.setPreambleLength(6);       // SHORTEST
  LoRa.setSyncWord(0xF1);          // Match sender
  LoRa.crc();                      // Enable CRC
  
  LoRa.receive();
  Serial.println("READY - Expecting ~200Hz data rate");
  Serial.println("Roll,Pitch,Yaw,QW,QX,QY,QZ,Interval,Counter,RSSI");
}

//==============================================================================
// ULTRA-FAST Main Loop - Zero latency
//==============================================================================
void loop() {
  // Check for packet immediately - NO polling delays
  int size = LoRa.parsePacket();
  
  if (size == sizeof(FastOrientationPacket)) {
    unsigned long now = millis();
    
    // Read packet directly
    LoRa.readBytes((uint8_t*)&packet, size);
    
    // Calculate interval
    unsigned long interval = now - lastReceiveTime;
    lastReceiveTime = now;
    packetCount++;
    
    // IMMEDIATE output - no buffering, no formatting delays
    Serial.print(packet.roll, 2);        Serial.print(",");
    Serial.print(packet.pitch, 2);       Serial.print(",");
    Serial.print(packet.yaw, 2);         Serial.print(",");
    Serial.print(packet.quat_w, 3);      Serial.print(",");
    Serial.print(packet.quat_x, 3);      Serial.print(",");
    Serial.print(packet.quat_y, 3);      Serial.print(",");
    Serial.print(packet.quat_z, 3);      Serial.print(",");
    Serial.print(interval);              Serial.print(",");
    Serial.print(packet.counter);        Serial.print(",");
    Serial.println(LoRa.rssi());
    
    // Check for missed packets
    if (lastCounter > 0 && packet.counter != lastCounter + 1) {
      Serial.print("MISS: "); Serial.println(packet.counter - lastCounter - 1);
    }
    lastCounter = packet.counter;
    
  } else if (size > 0) {
    // Wrong size packet
    Serial.print("ERR:Size="); Serial.println(size);
  }
  
  // Minimal stats every 2 seconds
  static unsigned long statsTime = 0;
  unsigned long now = millis();
  if (now - statsTime >= 2000) {
    statsTime = now;
    float rate = packetCount / 2.0;
    Serial.print("Rate: "); Serial.print(rate); Serial.println(" Hz");
    packetCount = 0;
  }
  
  // ABSOLUTELY NO DELAYS - process at maximum CPU speed
}
