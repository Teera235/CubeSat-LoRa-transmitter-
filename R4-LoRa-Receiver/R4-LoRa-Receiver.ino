/**
 * @file R4-LoRa-Serial-Receiver-Euler-Quat-TempPress.ino
 * @brief Receives Euler + Quaternion + Temp + Pressure via LoRa and outputs to Serial
 * @Teerathap Yaisungnoen Modified for Project xxxxxxxxx
 *
 * Device: Arduino R4 WiFi
 */

#include <SPI.h>
#include <LoRa.h>

#define SS_PIN    10
#define RST_PIN   9
#define DIO0_PIN  2

// ต้องตรงกับฝั่งส่ง
struct TelemetryPacket {
  float roll, pitch, yaw;              // Euler
  float quat_w, quat_x, quat_y, quat_z; // Quaternion
  float tempC;                         // Temperature
  float pressurePa;                    // Pressure
};

TelemetryPacket latestPacket;

unsigned long lastPacketTime = 0;
unsigned long packetCount = 0;
unsigned long lostPacketCount = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("R4: LoRa-Serial Receiver (Euler+Quat+Temp+Pressure) Initializing...");

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("ERROR: Starting LoRa failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(500E3);
  LoRa.setCodingRate4(5);

  LoRa.receive();
  Serial.println("System Ready. Waiting for data...");
  Serial.println("Format: Roll,Pitch,Yaw,QuatW,QuatX,QuatY,QuatZ,TempC,Pressure(Pa),TimeDiff(ms),RSSI,SNR");
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize > 0) {
    if (packetSize == sizeof(TelemetryPacket)) {
      LoRa.readBytes((uint8_t*)&latestPacket, packetSize);

      unsigned long currentTime = millis();
      unsigned long timeSinceLastPacket = currentTime - lastPacketTime;
      lastPacketTime = currentTime;
      packetCount++;

      // CSV output
      Serial.print(latestPacket.roll, 3);      Serial.print(",");
      Serial.print(latestPacket.pitch, 3);     Serial.print(",");
      Serial.print(latestPacket.yaw, 3);       Serial.print(",");
      Serial.print(latestPacket.quat_w, 4);    Serial.print(",");
      Serial.print(latestPacket.quat_x, 4);    Serial.print(",");
      Serial.print(latestPacket.quat_y, 4);    Serial.print(",");
      Serial.print(latestPacket.quat_z, 4);    Serial.print(",");
      Serial.print(latestPacket.tempC, 2);     Serial.print(",");
      Serial.print(latestPacket.pressurePa, 2);Serial.print(",");
      Serial.print(timeSinceLastPacket);       Serial.print(",");
      Serial.print(LoRa.rssi());               Serial.print(",");
      Serial.println(LoRa.packetSnr());

      if (timeSinceLastPacket > 100 && packetCount > 1) {
        Serial.print("WARNING: Long gap detected: ");
        Serial.print(timeSinceLastPacket);
        Serial.println(" ms");
      }

    } else {
      Serial.print("ERROR: Wrong packet size: ");
      Serial.print(packetSize);
      Serial.print(" (expected: ");
      Serial.print(sizeof(TelemetryPacket));
      Serial.println(")");
      lostPacketCount++;
    }
  }

  // Show stats every 10s
  static unsigned long lastStatsTime = 0;
  if (millis() - lastStatsTime > 10000) {
    lastStatsTime = millis();
    Serial.print("STATS - Packets: ");
    Serial.print(packetCount);
    Serial.print(", Lost: ");
    Serial.print(lostPacketCount);
    Serial.print(", Rate: ");
    Serial.print(packetCount / 10.0);
    Serial.println(" Hz");

    packetCount = 0;
    lostPacketCount = 0;
  }
}
