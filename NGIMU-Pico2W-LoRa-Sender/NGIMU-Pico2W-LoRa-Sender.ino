/**
 * @file NGIMU-Pico2W-LoRa-Sender.ino
 * @brief Ultra-Fast Sender: NGIMU data transmission via LoRa
 * Optimized for maximum speed and minimum latency by teerathap yaisungnoen
 */

#include <SPI.h>
#include <LoRa.h>
#include "NgimuReceive.h"

// LoRa Pinout for Pico 2W 
#define LORA_MISO  16
#define LORA_MOSI  19
#define LORA_SCK   18
#define LORA_CS    17
#define LORA_RST   21
#define LORA_DIO0  20

// Ultra-Fast 
const unsigned long LORA_SEND_INTERVAL = 20; // 20ms = 50Hz 
const unsigned long MIN_SEND_INTERVAL = 10;  // 10ms (100Hz max)
unsigned long lastSendTime = 0;

struct ImuDataPacket {
  float roll;
  float pitch;
  float yaw;
  uint32_t timestamp;    
  uint16_t packetId;     
};

volatile float roll = 0.0f;
volatile float pitch = 0.0f;
volatile float yaw = 0.0f;
volatile bool newDataAvailable = false;

uint16_t packetCounter = 0;
unsigned long lastStatsTime = 0;
uint32_t packetsSent = 0;

void handleEulerCallback(const NgimuEuler ngimuEuler) {
    roll = ngimuEuler.roll;
    pitch = ngimuEuler.pitch;
    yaw = ngimuEuler.yaw;
    newDataAvailable = true;  
}

void handleErrorCallback(const char* const errorMessage) {
    Serial.print("NGIMU Error: ");
    Serial.println(errorMessage);
}

void setup() {
    Serial.begin(115200);
    SPI.setRX(LORA_MISO);
    SPI.setTX(LORA_MOSI);
    SPI.setSCK(LORA_SCK);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa failed");
        while (1) delay(1000);
    }
    
    LoRa.setSpreadingFactor(6);          
    LoRa.setSignalBandwidth(500E3);      // Bandwidth Max
    LoRa.setCodingRate4(5);              // Error correction low
    LoRa.setTxPower(17);                 
    LoRa.setPreambleLength(6);           // Preamble low
    LoRa.setSyncWord(0x12);              // Custom sync word
    Serial1.setTX(4);
    Serial1.setRX(5);
    Serial1.begin(115200);
    NgimuReceiveInitialise();
    NgimuReceiveSetEulerCallback(handleEulerCallback);
    NgimuReceiveSetReceiveErrorCallback(handleErrorCallback);
    lastStatsTime = millis();
}


void loop() {
    while (Serial1.available() > 0) {
        NgimuReceiveProcessSerialByte(Serial1.read());
    }
    unsigned long currentTime = millis();
    unsigned long timeSinceLastSend = currentTime - lastSendTime;
    bool shouldSend = false;

    if (newDataAvailable && timeSinceLastSend >= MIN_SEND_INTERVAL) {
        shouldSend = true;
        newDataAvailable = false;
    }
    else if (timeSinceLastSend >= LORA_SEND_INTERVAL) {
        shouldSend = true;
    }
    if (shouldSend) {
        sendImuData(currentTime, timeSinceLastSend);
        lastSendTime = currentTime;
        packetsSent++;
    }
    if (currentTime - lastStatsTime >= 1000) {
        showStats(currentTime);
        lastStatsTime = currentTime;
    }
}

void sendImuData(unsigned long currentTime, unsigned long sendInterval) {
    ImuDataPacket packet;
    packet.roll = roll;
    packet.pitch = pitch;
    packet.yaw = yaw;
    packet.timestamp = currentTime;
    packet.packetId = ++packetCounter;
    
    // non-blocking
    LoRa.beginPacket();
    LoRa.write((uint8_t*)&packet, sizeof(packet));
    LoRa.endPacket(false); 
    
    // CSV format
    Serial.print(packet.packetId);
    Serial.print(",");
    Serial.print(packet.roll, 3);
    Serial.print(",");
    Serial.print(packet.pitch, 3);
    Serial.print(",");
    Serial.print(packet.yaw, 3);
    Serial.print(",");
    Serial.print(sendInterval);
    Serial.print(",");
    Serial.println((float)packetsSent * 1000.0 / (currentTime - (lastStatsTime == 0 ? currentTime : lastStatsTime)));
}

void showStats(unsigned long currentTime) {
    float packetsPerSecond = (float)packetsSent * 1000.0 / 1000.0; 
    
    Serial.println("=== PERFORMANCE STATS ===");
    Serial.print("Packets/sec: ");
    Serial.println(packetsPerSecond, 1);
    Serial.print("Total packets: ");
    Serial.println(packetsSent);
    Serial.print("Uptime: ");
    Serial.print(currentTime / 1000);
    Serial.println(" seconds");
    Serial.println("========================");
    
    packetsSent = 0;
}