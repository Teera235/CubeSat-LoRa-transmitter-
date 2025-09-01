/**
 * @file Pico2W-NGIMU-Reader.ino
 * @brief Read quaternion data from NGIMU via Serial1 on Pico W
 * 
 * Device: Raspberry Pi Pico W
 * 
 * Connection (UART1 remap to GP8/GP9):
 *   NGIMU TX (Pin 5 on Serial connector) -> Pico GP9 (RX1, pin 12)
 *   NGIMU GND (Pin 1) -> Pico GND
 *   (Optional) NGIMU RX (Pin 4) <- Pico GP8 (TX1, pin 11) if you want to send commands
 * 
 * Baud Rate: 115200
 * Libraries: OSC99 + NgimuReceive
 */

#include "NgimuReceive.h"

struct OrientationPacket {
  float roll, pitch, yaw;
  float quat_w, quat_x, quat_y, quat_z;
};

OrientationPacket currentPacket;
bool newDataAvailable = false;

// Convert Quaternion -> Euler (deg)
void quaternionToEuler(float w, float x, float y, float z,
                       float &roll, float &pitch, float &yaw) {
  float sinr_cosp = 2 * (w * x + y * z);
  float cosr_cosp = 1 - 2 * (x * x + y * y);
  roll = atan2(sinr_cosp, cosr_cosp) * 180.0 / PI;

  float sinp = 2 * (w * y - z * x);
  if (abs(sinp) >= 1)
    pitch = copysign(PI / 2, sinp) * 180.0 / PI;
  else
    pitch = asin(sinp) * 180.0 / PI;

  float siny_cosp = 2 * (w * z + x * y);
  float cosy_cosp = 1 - 2 * (y * y + z * z);
  yaw = atan2(siny_cosp, cosy_cosp) * 180.0 / PI;
}

void setup() {
  Serial.begin(115200);      // USB monitor

  // Remap Serial1 to GP8 (TX) and GP9 (RX)
  Serial1.setTX(8);
  Serial1.setRX(9);
  Serial1.begin(115200);

  delay(2000);
  Serial.println("=== Pico2W NGIMU Reader Started ===");

  NgimuReceiveInitialise();
  NgimuReceiveSetQuaternionCallback(ngimuQuaternionCallback);

  currentPacket = {0, 0, 0, 1, 0, 0, 0};
}

void loop() {
  // Feed NGIMU stream into parser
  while (Serial1.available() > 0) {
    NgimuReceiveProcessSerialByte(Serial1.read());
  }

  // Print if new data available
  if (newDataAvailable) {
    Serial.print("Roll: ");   Serial.print(currentPacket.roll, 2);
    Serial.print("  Pitch: "); Serial.print(currentPacket.pitch, 2);
    Serial.print("  Yaw: ");   Serial.print(currentPacket.yaw, 2);
    Serial.print(" | Quat: ");
    Serial.print(currentPacket.quat_w, 4); Serial.print(", ");
    Serial.print(currentPacket.quat_x, 4); Serial.print(", ");
    Serial.print(currentPacket.quat_y, 4); Serial.print(", ");
    Serial.println(currentPacket.quat_z, 4);

    newDataAvailable = false;
  }
}

// Callback when quaternion received
void ngimuQuaternionCallback(const NgimuQuaternion q) {
  currentPacket.quat_w = q.w;
  currentPacket.quat_x = q.x;
  currentPacket.quat_y = q.y;
  currentPacket.quat_z = q.z;

  quaternionToEuler(q.w, q.x, q.y, q.z,
                    currentPacket.roll, currentPacket.pitch, currentPacket.yaw);

  newDataAvailable = true;
}
