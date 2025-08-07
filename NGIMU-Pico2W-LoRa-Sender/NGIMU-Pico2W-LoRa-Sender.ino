/**
 * @file NGIMU-Pico-LoRa-Sender-ULTRA-FAST.ino
 * @brief MAXIMUM SPEED for 5m range - Ultra-fast real-time transmission
 */

//==============================================================================
// Includes & Definitions
//==============================================================================
#include <SPI.h>
#include <LoRa.h>
#include "NgimuReceive.h"

// LoRa Pinout for Pico W
#define LORA_MISO  16
#define LORA_MOSI  19
#define LORA_SCK   18
#define LORA_CS    17
#define LORA_RST   21
#define LORA_DIO0  20

// ULTRA-FAST settings for 5m range
const unsigned long MIN_SEND_INTERVAL = 5;   // 200 Hz !! (5ms minimum)
unsigned long lastSendTime = 0;

//==============================================================================
// Minimal Data Packet (Optimized for speed)
//==============================================================================
struct __attribute__((packed)) FastOrientationPacket {
  float roll, pitch, yaw;
  float quat_w, quat_x, quat_y, quat_z;
  uint16_t counter;  // Smaller counter instead of timestamp
};

//==============================================================================
// Global variables - Optimized
//==============================================================================
FastOrientationPacket currentData;
volatile bool dataReady = false;
uint16_t packetCounter = 0;

//==============================================================================
// ULTRA-FAST NGIMU Callbacks
//==============================================================================
void handleEulerData(const NgimuEuler ngimuEuler) {
    currentData.roll = ngimuEuler.roll;
    currentData.pitch = ngimuEuler.pitch;
    currentData.yaw = ngimuEuler.yaw;
    dataReady = true;  // Trigger immediate send
}

void handleQuaternionData(const NgimuQuaternion ngimuQuaternion) {
    currentData.quat_w = ngimuQuaternion.w;
    currentData.quat_x = ngimuQuaternion.x;
    currentData.quat_y = ngimuQuaternion.y;
    currentData.quat_z = ngimuQuaternion.z;
}

void handleErrorCallback(const char* const errorMessage) { /* Minimal error handling */ }
void handleSensorsCallback(const NgimuSensors ngimuSensors) { /* Ignore */ }

//==============================================================================
// ULTRA-FAST Send Function
//==============================================================================
inline void fastSend() {
    currentData.counter = ++packetCounter;
    
    // FASTEST LoRa transmission
    LoRa.beginPacket();
    LoRa.write((uint8_t*)&currentData, sizeof(currentData));
    LoRa.endPacket(false); // Non-blocking
    
    dataReady = false;
}

//==============================================================================
// Setup - Optimized for 5m range
//==============================================================================
void setup() {
    Serial.begin(230400); // Faster serial
    Serial.println("ULTRA-FAST LoRa Sender - 5m Range Mode");

    // Initialize LoRa with MAXIMUM SPEED settings
    SPI.setRX(LORA_MISO); SPI.setTX(LORA_MOSI); SPI.setSCK(LORA_SCK);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6)) { while (1); }
    
    // ULTRA-FAST LoRa settings for 5m range
    LoRa.setSpreadingFactor(6);      // FASTEST (minimum SF)
    LoRa.setSignalBandwidth(500E3);  // WIDEST bandwidth
    LoRa.setCodingRate4(5);          // LEAST error correction
    LoRa.setTxPower(2);              // LOW power for 5m (reduces interference)
    LoRa.setPreambleLength(6);       // SHORTEST preamble
    LoRa.setSyncWord(0xF1);          // Unique sync word
    LoRa.crc();                      // Enable CRC for reliability
    
    // FASTEST Serial for NGIMU
    Serial2.begin(230400); // Higher baud rate
    NgimuReceiveInitialise();
    NgimuReceiveSetEulerCallback(handleEulerData);
    NgimuReceiveSetQuaternionCallback(handleQuaternionData);
    NgimuReceiveSetReceiveErrorCallback(handleErrorCallback);
    NgimuReceiveSetSensorsCallback(handleSensorsCallback);

    memset(&currentData, 0, sizeof(currentData));
    Serial.println("READY - ULTRA-FAST MODE: ~200Hz");
}

//==============================================================================
// ULTRA-FAST Main Loop
//==============================================================================
void loop() {
    // Process NGIMU data immediately
    while (Serial2.available()) {
        NgimuReceiveProcessSerialByte(Serial2.read());
    }
    
    // Send IMMEDIATELY when data is ready AND interval passed
    unsigned long now = millis();
    if (dataReady && (now - lastSendTime >= MIN_SEND_INTERVAL)) {
        fastSend();
        lastSendTime = now;
    }
    
    // Minimal stats (every 2 seconds to avoid slowing down)
    static unsigned long lastStats = 0;
    if (now - lastStats > 2000) {
        lastStats = now;
        Serial.print("Packets: "); Serial.println(packetCounter);
        packetCounter = 0;
    }
    
    // NO DELAYS - Maximum responsiveness
}
