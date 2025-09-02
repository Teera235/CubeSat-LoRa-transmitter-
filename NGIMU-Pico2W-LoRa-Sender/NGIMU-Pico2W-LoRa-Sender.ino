/**
 * @file Pico2W-NGIMU-LoRa-Sender-BMP280-LED.ino
 * @brief NGIMU (Euler+Quat) + BMP280 (Temp/Pressure) + LoRa + Status LEDs (GP2 green, GP3 yellow)
 * @author Teerathep
 *
 * Device: Raspberry Pi Pico W (RP2040, Arduino core)
 * Wiring (proposed):
 *   NGIMU TX  -> Pico GP9 (UART1 RX)
 *   NGIMU GND -> Pico GND
 *   (Optional) NGIMU RX <- Pico GP8 (UART1 TX)
 *
 *   LoRa SX1278:
 *     MISO=GP16, MOSI=GP19, SCK=GP18, CS=GP17, RST=GP21, DIO0=GP20
 *
 *   BMP280 I2C @0x76 on default I2C0 pins (SDA=GP4, SCL=GP5) unless you moved them
 *
 *   LEDs:
 *     Green  -> GP2
 *     Yellow -> GP3
 */

#include "NgimuReceive.h"
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// ======= Config =======
namespace cfg {
  // LoRa
  constexpr long   LORA_FREQ_HZ = 433E6;
  constexpr int    LORA_SF      = 7;
  constexpr long   LORA_BW_HZ   = 500E3;
  constexpr int    LORA_CR      = 5;
  constexpr int    LORA_TX_PWR  = 20;     // dBm

  // UART (NGIMU)
  constexpr uint32_t NGIMU_BAUD = 115200;
  constexpr int      NGIMU_TX   = 8;      // Pico GP8  (UART1 TX)
  constexpr int      NGIMU_RX   = 9;      // Pico GP9  (UART1 RX)

  // BMP280
  constexpr uint8_t  BMP280_ADDR = 0x76;  // change to 0x77 if needed

  // LEDs
  constexpr int LED_GREEN = 2;   // GP2
  constexpr int LED_YELLOW= 3;   // GP3

  // Timing
  constexpr unsigned long SEND_INTERVAL_OK_MS   = 50;   // 20 Hz when data flows
  constexpr unsigned long SEND_INTERVAL_DEG_MS  = 200;  // 5 Hz when NGIMU missing
  constexpr unsigned long NGIMU_TIMEOUT_MS      = 1000; // declare NGIMU link lost after 1s
  constexpr unsigned long HEARTBEAT_PERIOD_MS   = 500;  // green LED heartbeat: 2 Hz
  constexpr unsigned long LORA_TX_BLINK_MS      = 60;   // yellow short blink on TX
}

// ======= Pins LoRa =======
#define LORA_MISO  16
#define LORA_MOSI  19
#define LORA_SCK   18
#define LORA_CS    17
#define LORA_RST   21
#define LORA_DIO0  20

// ======= BMP280 I2C =======
Adafruit_BMP280 bmp;
bool bmp_ok = false;

// ======= Data packet =======
struct TelemetryPacket {
  float roll, pitch, yaw;               // Euler [deg]
  float quat_w, quat_x, quat_y, quat_z; // Quaternion
  float tempC;                          // Temperature [°C]
  float pressurePa;                     // Pressure [Pa]
  uint8_t status;                       // bit flags: b0=NGIMU_OK, b1=BMP_OK
};

TelemetryPacket currentPacket;
volatile bool newEulerArrived = false;

unsigned long t_lastSend = 0;
unsigned long t_lastEuler = 0;
unsigned long t_lastHeartbeat = 0;
unsigned long t_yellowOffDue = 0;

// ======= Helpers: LED =======
inline void ledGreen(bool on){ digitalWrite(cfg::LED_GREEN, on ? HIGH : LOW); }
inline void ledYellow(bool on){ digitalWrite(cfg::LED_YELLOW, on ? HIGH : LOW); }

void heartbeatTask(unsigned long now, bool system_ok) {
  static bool hb_state = false;
  if (system_ok) {
    if (now - t_lastHeartbeat >= cfg::HEARTBEAT_PERIOD_MS/2) {
      hb_state = !hb_state;
      ledGreen(hb_state);
      t_lastHeartbeat = now;
    }
  } else {
    // fault: solid OFF for green
    ledGreen(false);
  }
}

// Short blink yellow on LoRa TX
void yellowBlinkOnTx(unsigned long now){
  ledYellow(true);
  t_yellowOffDue = now + cfg::LORA_TX_BLINK_MS;
}

void yellowBlinkMaintain(unsigned long now, bool ngimu_ok){
  // Priority of yellow LED:
  // - If NGIMU not OK => solid ON
  // - Else if BMP missing => slow blink 1 Hz
  // - Else maintain TX short-blink window
  static bool blink_state = false;
  static unsigned long t_lastBlink = 0;

  if (!ngimu_ok) {
    ledYellow(true);
    return;
  }

  if (!bmp_ok) {
    if (now - t_lastBlink >= 500) {
      blink_state = !blink_state;
      ledYellow(blink_state);
      t_lastBlink = now;
    }
    return;
  }

  // Normal: honor TX short blink window
  if (t_yellowOffDue != 0 && now >= t_yellowOffDue) {
    ledYellow(false);
    t_yellowOffDue = 0;
  }
}

// ======= NGIMU Callbacks =======
void ngimuQuaternionCallback(const NgimuQuaternion q) {
  currentPacket.quat_w = q.w;
  currentPacket.quat_x = q.x;
  currentPacket.quat_y = q.y;
  currentPacket.quat_z = q.z;
}

void ngimuEulerCallback(const NgimuEuler e) {
  currentPacket.roll  = e.roll;
  currentPacket.pitch = e.pitch;
  currentPacket.yaw   = e.yaw;
  newEulerArrived = true;
  t_lastEuler = millis();

  // Debug print light-weight
  Serial.print("Euler[deg] R=");
  Serial.print(e.roll, 2);
  Serial.print(" P=");
  Serial.print(e.pitch, 2);
  Serial.print(" Y=");
  Serial.print(e.yaw, 2);
  Serial.print(" | Quat=");
  Serial.print(currentPacket.quat_w, 3); Serial.print(",");
  Serial.print(currentPacket.quat_x, 3); Serial.print(",");
  Serial.print(currentPacket.quat_y, 3); Serial.print(",");
  Serial.print(currentPacket.quat_z, 3);
  Serial.print(" | T=");
  Serial.print(currentPacket.tempC, 2);
  Serial.print("C P=");
  Serial.print(currentPacket.pressurePa, 1);
  Serial.println("Pa");
}

// ======= LoRa TX =======
void transmitLoRaPacket() {
  LoRa.beginPacket();
  LoRa.write((uint8_t*)&currentPacket, sizeof(TelemetryPacket));
  LoRa.endPacket();

  Serial.print("TX ");
  Serial.print(sizeof(TelemetryPacket));
  Serial.println(" bytes");
}

// ======= Setup =======
void setup() {
  // Serial (USB)
  Serial.begin(115200);
  delay(200);
  Serial.println("Pico2W: NGIMU-LoRa-BMP280-LED Initializing...");

  // LEDs
  pinMode(cfg::LED_GREEN, OUTPUT);
  pinMode(cfg::LED_YELLOW, OUTPUT);
  ledGreen(true);   // solid ON during boot
  ledYellow(false);

  // UART1 for NGIMU
  Serial1.setTX(cfg::NGIMU_TX);
  Serial1.setRX(cfg::NGIMU_RX);
  Serial1.begin(cfg::NGIMU_BAUD);
  Serial.println("UART1 (NGIMU) started.");

  // NGIMU parser
  NgimuReceiveInitialise();
  NgimuReceiveSetQuaternionCallback(ngimuQuaternionCallback);
  NgimuReceiveSetEulerCallback(ngimuEulerCallback);
  Serial.println("NGIMU parser ready.");

  // BMP280
  Wire.begin(); // default I2C0 (GP4= SDA, GP5= SCL) unless board re-mapped
  if (bmp.begin(cfg::BMP280_ADDR)) {
    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_1
    );
    bmp_ok = true;
    Serial.println("BMP280 OK.");
  } else {
    bmp_ok = false;
    Serial.println("ERROR: BMP280 not found!");
  }

  // LoRa
  SPI.setMOSI(LORA_MOSI);
  SPI.setMISO(LORA_MISO);
  SPI.setSCK(LORA_SCK);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(cfg::LORA_FREQ_HZ)) {
    Serial.println("ERROR: LoRa.begin failed!");
    // fault: green OFF, yellow ON
    ledGreen(false);
    ledYellow(true);
    while (1) { delay(1000); }
  }
  LoRa.setSpreadingFactor(cfg::LORA_SF);
  LoRa.setSignalBandwidth(cfg::LORA_BW_HZ);
  LoRa.setCodingRate4(cfg::LORA_CR);
  LoRa.setTxPower(cfg::LORA_TX_PWR);
  Serial.println("LoRa OK.");

  // Init packet defaults
  currentPacket = {0,0,0, 1,0,0,0, NAN, NAN, 0};

  Serial.println("System Ready - waiting NGIMU...");
  // Boot complete → allow heartbeat
  ledGreen(false);
  t_lastHeartbeat = millis();
  t_lastEuler = millis(); // start timer so timeout logic runs
}

// ======= Loop =======
void loop() {
  // 1) Feed NGIMU parser
  while (Serial1.available() > 0) {
    NgimuReceiveProcessSerialByte(Serial1.read());
  }

  // 2) Read BMP280
  if (bmp_ok) {
    currentPacket.tempC = bmp.readTemperature();
    currentPacket.pressurePa = bmp.readPressure();
  } else {
    currentPacket.tempC = NAN;
    currentPacket.pressurePa = NAN;
  }

  // 3) Decide TX interval based on NGIMU link
  unsigned long now = millis();
  bool ngimu_ok = (now - t_lastEuler) <= cfg::NGIMU_TIMEOUT_MS;
  unsigned long interval = ngimu_ok ? cfg::SEND_INTERVAL_OK_MS : cfg::SEND_INTERVAL_DEG_MS;

  // 4) Status flags
  currentPacket.status = 0;
  if (ngimu_ok) currentPacket.status |= 0x01; // b0 NGIMU OK
  if (bmp_ok)   currentPacket.status |= 0x02; // b1 BMP OK

  // 5) Heartbeat & yellow LED policy
  heartbeatTask(now, /*system_ok=*/true);
  yellowBlinkMaintain(now, ngimu_ok);

  // 6) Transmit policy:
  //    - When NGIMU is flowing: send only when new Euler arrived and interval elapsed
  //    - When NGIMU missing: send at degraded rate (interval) anyway with last-known data
  bool timeToSend = (now - t_lastSend >= interval);
  if ((ngimu_ok && newEulerArrived && timeToSend) ||
      (!ngimu_ok && timeToSend)) {

    transmitLoRaPacket();
    yellowBlinkOnTx(now);

    t_lastSend = now;
    newEulerArrived = false;
  }

  // Free up CPU
  delay(1);
}
