/**
 * @file R4-LoRa-Receiver.ino
 * @brief Ultra-Fast Receiver: LoRa IMU data receiver (No LCD version)
 * Serial output optimized for IMU processing
 */

#include <SPI.h>
#include <LoRa.h>

// LoRa Pinout for Arduino R4 
#define SS_PIN    10
#define RST_PIN   9
#define DIO0_PIN  2

struct ImuDataPacket {
  float roll;
  float pitch;
  float yaw;
  uint32_t timestamp;
  uint16_t packetId;
};

uint16_t lastPacketId = 0;
uint32_t packetsReceived = 0;
uint32_t packetsLost = 0;
unsigned long lastPacketTime = 0;
unsigned long lastStatsTime = 0;
int minRssi = 0, maxRssi = -200;
uint32_t minLatency = 999999, maxLatency = 0;
uint32_t totalLatency = 0;
uint32_t latencyCount = 0;

void setup() {
  Serial.begin(115200);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa failed");
    while (1) delay(1000);
  }
  
  LoRa.setSpreadingFactor(6);
  LoRa.setSignalBandwidth(500E3);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(6);
  LoRa.setSyncWord(0x12);
  LoRa.receive();
  
  lastStatsTime = millis();
}

void loop() {
  int packetSize = LoRa.parsePacket();
  
  if (packetSize == sizeof(ImuDataPacket)) {
    processIncomingPacket();
  }
  unsigned long currentTime = millis();
  if (currentTime - lastStatsTime >= 5000) {
    showPerformanceStats();
    lastStatsTime = currentTime;
  }
}

void processIncomingPacket() {
  ImuDataPacket packet;
  unsigned long receiveTime = millis();
  int bytesRead = LoRa.readBytes((uint8_t*)&packet, sizeof(packet));
  if (bytesRead != sizeof(packet)) {
    Serial.println("ERROR,Invalid packet size");
    return;
  }
  int rssi = LoRa.packetRssi();
  uint32_t latency = receiveTime - packet.timestamp;
  uint16_t expectedId = lastPacketId + 1;
  uint16_t packetsLostThisTime = 0;
  
  if (packet.packetId != expectedId && lastPacketId != 0) {
    if (packet.packetId > expectedId) {
      packetsLostThisTime = packet.packetId - expectedId;
      packetsLost += packetsLostThisTime;
    }
  }
  
  lastPacketId = packet.packetId;
  packetsReceived++;
  
  updateQualityMetrics(rssi, latency);
  float receiveRate = 0;
  if (lastPacketTime > 0) {
    receiveRate = 1000.0 / (receiveTime - lastPacketTime);
  }
  lastPacketTime = receiveTime;
  
  Serial.print(packet.packetId);
  Serial.print(",");
  Serial.print(packet.roll, 4);     
  Serial.print(",");
  Serial.print(packet.pitch, 4);
  Serial.print(",");
  Serial.print(packet.yaw, 4);
  Serial.print(",");
  Serial.print(rssi);
  Serial.print(",");
  Serial.print(latency);
  Serial.print(",");
  Serial.print(packetsLostThisTime);
  Serial.print(",");
  Serial.println(receiveRate, 1);
}


void updateQualityMetrics(int rssi, uint32_t latency) {
  if (rssi > maxRssi) maxRssi = rssi;
  if (rssi < minRssi) minRssi = rssi;
  
  if (latency < minLatency) minLatency = latency;
  if (latency > maxLatency) maxLatency = latency;
  totalLatency += latency;
  latencyCount++;
}

void showPerformanceStats() {
  Serial.println();
  Serial.println("=== PERFORMANCE STATISTICS ===");
  
  uint32_t totalPackets = packetsReceived + packetsLost;
  float lossRate = totalPackets > 0 ? (float)packetsLost / totalPackets * 100.0 : 0;
  
  Serial.print("Packets: Received=");
  Serial.print(packetsReceived);
  Serial.print(", Lost=");
  Serial.print(packetsLost);
  Serial.print(", Loss Rate=");
  Serial.print(lossRate, 1);
  Serial.println("%");
  
  Serial.print("RSSI: Min=");
  Serial.print(minRssi);
  Serial.print("dBm, Max=");
  Serial.print(maxRssi);
  Serial.println("dBm");
  
  if (latencyCount > 0) {
    float avgLatency = (float)totalLatency / latencyCount;
    Serial.print("Latency: Min=");
    Serial.print(minLatency);
    Serial.print("ms, Max=");
    Serial.print(maxLatency);
    Serial.print("ms, Avg=");
    Serial.print(avgLatency, 1);
    Serial.println("ms");
  }
  
  Serial.print("Connection Quality: ");
  if (lossRate < 1.0 && (latencyCount > 0 ? (float)totalLatency/latencyCount < 50 : true)) {
    Serial.println("EXCELLENT");
  } else if (lossRate < 5.0 && (latencyCount > 0 ? (float)totalLatency/latencyCount < 100 : true)) {
    Serial.println("GOOD");
  } else if (lossRate < 10.0) {
    Serial.println("FAIR");
  } else {
    Serial.println("POOR");
  }
  
  Serial.println("==============================");
  Serial.println();
  
  minRssi = 0;
  maxRssi = -200;
  minLatency = 999999;
  maxLatency = 0;
  totalLatency = 0;
  latencyCount = 0;
}