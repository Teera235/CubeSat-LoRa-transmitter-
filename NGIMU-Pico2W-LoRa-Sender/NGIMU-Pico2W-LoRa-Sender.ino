/**
 * @file Pico2W-NGIMU-LoRa-Sender.ino
 * @brief Receives quaternion data from NGIMU and transmits via LoRa to Arduino R4
 * @Teerathap Yaisungnoen Modified for Project xxxxxxxxx
 * 
 * Device: Pico2W
 * 
 * The OSC99 source files (i.e. the "Osc99" directory) must be added to the
 * Arduino libraries folder. See: https://www.arduino.cc/en/guide/libraries
 **/

#include "NgimuReceive.h"
#include <SPI.h>
#include <LoRa.h>

#define LORA_MISO  16
#define LORA_MOSI  19
#define LORA_SCK   18
#define LORA_CS    17
#define LORA_RST   21
#define LORA_DIO0  20

struct OrientationPacket {
  float roll, pitch, yaw;
  float quat_w, quat_x, quat_y, quat_z;
};

OrientationPacket currentPacket;
bool newDataAvailable = false;
unsigned long lastTransmissionTime = 0;
const unsigned long TRANSMISSION_INTERVAL = 100; // 100ms = 10Hz

// Quaternion to Euler conversion
void quaternionToEuler(float w, float x, float y, float z, float &roll, float &pitch, float &yaw) {
  // Roll (x-axis rotation)
  float sinr_cosp = 2 * (w * x + y * z);
  float cosr_cosp = 1 - 2 * (x * x + y * y);
  roll = atan2(sinr_cosp, cosr_cosp) * 180.0 / PI;
  // Pitch (y-axis rotation)
  float sinp = 2 * (w * y - z * x);
  if (abs(sinp) >= 1)
    pitch = copysign(PI / 2, sinp) * 180.0 / PI; // Use 90 degrees if out of range
  else
    pitch = asin(sinp) * 180.0 / PI;
  // Yaw (z-axis rotation)
  float siny_cosp = 2 * (w * z + x * y);
  float cosy_cosp = 1 - 2 * (y * y + z * z);
  yaw = atan2(siny_cosp, cosy_cosp) * 180.0 / PI;
}

void setup() {
  Serial.begin(115200);  // Pico2W 
  Serial1.begin(115200); // NGIMU serial 
  
  delay(2000); 
  Serial.println("Teensy: NGIMU-LoRa Sender Initializing...");

  SPI.setMOSI(LORA_MOSI);
  SPI.setMISO(LORA_MISO);
  SPI.setSCK(LORA_SCK);
  
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("ERROR: Starting LoRa failed!");
    while (1) {
      delay(1000);
    }
  }
  
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(500E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(20); 
  
  Serial.println("LoRa initialized successfully!");

  NgimuReceiveInitialise();
  NgimuReceiveSetQuaternionCallback(ngimuQuaternionCallback);
  
  Serial.println("NGIMU receiver initialized!");
  Serial.println("System Ready - Waiting for NGIMU data...");
  
  currentPacket.roll = 0.0;
  currentPacket.pitch = 0.0;
  currentPacket.yaw = 0.0;
  currentPacket.quat_w = 1.0;
  currentPacket.quat_x = 0.0;
  currentPacket.quat_y = 0.0;
  currentPacket.quat_z = 0.0;
}

void loop() {
  while (Serial1.available() > 0) {
    NgimuReceiveProcessSerialByte(Serial1.read());
  }
  
  unsigned long currentTime = millis();
  if (newDataAvailable && (currentTime - lastTransmissionTime >= TRANSMISSION_INTERVAL)) {
    transmitLoRaPacket();
    lastTransmissionTime = currentTime;
    newDataAvailable = false;
  }
  delay(1);
}

void ngimuQuaternionCallback(const NgimuQuaternion ngimuQuaternion) {
  currentPacket.quat_w = ngimuQuaternion.w;
  currentPacket.quat_x = ngimuQuaternion.x;
  currentPacket.quat_y = ngimuQuaternion.y;
  currentPacket.quat_z = ngimuQuaternion.z;
  
  // Convert quaternion to Euler angles
  quaternionToEuler(ngimuQuaternion.w, ngimuQuaternion.x, 
                   ngimuQuaternion.y, ngimuQuaternion.z,
                   currentPacket.roll, currentPacket.pitch, currentPacket.yaw);
  
  newDataAvailable = true;
  
  Serial.print("NGIMU Data - Roll: ");
  Serial.print(currentPacket.roll, 2);
  Serial.print(", Pitch: ");
  Serial.print(currentPacket.pitch, 2);
  Serial.print(", Yaw: ");
  Serial.print(currentPacket.yaw, 2);
  Serial.print(" | Quat: ");
  Serial.print(currentPacket.quat_w, 4);
  Serial.print(", ");
  Serial.print(currentPacket.quat_x, 4);
  Serial.print(", ");
  Serial.print(currentPacket.quat_y, 4);
  Serial.print(", ");
  Serial.println(currentPacket.quat_z, 4);
}


// LoRa Transmission Function
void transmitLoRaPacket() {
  LoRa.beginPacket();
  LoRa.write((uint8_t*)&currentPacket, sizeof(OrientationPacket));
  LoRa.endPacket();
  Serial.print("LoRa packet sent - Size: ");
  Serial.print(sizeof(OrientationPacket));
  Serial.println(" bytes");
}
