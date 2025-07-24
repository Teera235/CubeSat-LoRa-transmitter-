# Ultra-Fast CubeSat Transmitter 🛰️

Real-time LoRa transmitter สำหรับ CubeSat ด้วย NGIMU sensor และ ESP32 ที่ปรับแต่งให้ความเร็วสูงสุดในระยะ 100 เมตร

## 📋 Table of Contents
- [ข้อมูลเบื้องต้น](#ข้อมูลเบื้องต้น)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [การเชื่อมต่อฮาร์ดแวร์](#การเชื่อมต่อฮาร์ดแวร์)
- [การติดตั้ง](#การติดตั้ง)
- [การใช้งาน](#การใช้งาน)
- [Performance](#performance)
- [Troubleshooting](#troubleshooting)
- [การพัฒนาต่อ](#การพัฒนาต่อ)

## ข้อมูลเบื้องต้น

โปรเจกต์นี้เป็นระบบส่งข้อมูลความเร็วสูงสำหรับ CubeSat ที่ใช้:
- **ESP32** เป็น main controller (240MHz)
- **NGIMU** สำหรับ attitude sensing
- **LoRa Ra-01** สำหรับ RF communication

### 🎯 เป้าหมาย
- **Real-time transmission**: < 5ms latency
- **High frequency**: 150-200 Hz transmission rate
- **Short range optimized**: 100 เมตร maximum
- **Ultra-low latency**: เหมาะสำหรับ control applications

## Hardware Requirements

### 🔧 Main Components
| Component | Model | Quantity |
|-----------|-------|----------|
| Microcontroller | ESP32 DevKit V1 | 1 |
| IMU Sensor | NGIMU | 1 |
| LoRa Module | Ra-01 (SX1278) | 1 |
| Antenna | 433MHz (λ/4 monopole) | 1 |

### 📊 Specifications
- **Frequency**: 433MHz (ISM Band)
- **Range**: Up to 100m (optimized)
- **Power**: 5V/3.3V compatible
- **Current**: ~200mA (transmission)

## Software Requirements

### 🔨 Development Environment
- **Arduino IDE** 1.8.19+ หรือ **PlatformIO**
- **ESP32 Arduino Core** 2.0.5+

### 📚 Required Libraries
```bash
# Arduino Library Manager ติดตั้ง:
1. ESP32 Board Package
2. NgimuReceive Library (จาก x-io Technologies)

# หรือ Manual Download:
# https://github.com/xioTechnologies/NGIMU-Arduino-Example
```

### 🛠️ Arduino IDE Setup
1. File → Preferences → Additional Board Manager URLs:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
2. Tools → Board → Boards Manager → ติดตั้ง "ESP32"
3. Tools → Board → ESP32 Arduino → "ESP32 Dev Module"

## การเชื่อมต่อฮาร์ดแวร์

### 🔌 ESP32 Pin Connections

#### LoRa Ra-01 Module
| Ra-01 Pin | ESP32 Pin | Function |
|-----------|-----------|----------|
| VCC | 3.3V | Power |
| GND | GND | Ground |
| MISO | GPIO 19 | SPI MISO |
| MOSI | GPIO 23 | SPI MOSI |
| SCK | GPIO 18 | SPI Clock |
| NSS | GPIO 5 | Chip Select |
| RST | GPIO 14 | Reset |
| DIO0 | GPIO 2 | IRQ |

#### NGIMU Connections
| NGIMU Pin | ESP32 Pin | Function |
|-----------|-----------|----------|
| VCC | 5V | Power |
| GND | GND | Ground |
| TX | GPIO 16 (RX2) | UART TX |
| RX | GPIO 17 (TX2) | UART RX |

### 🔧 Circuit Diagram
```
ESP32 DevKit V1
┌─────────────────┐
│              3V3├── Ra-01 VCC
│              GND├── Ra-01 GND, NGIMU GND
│                 │
│           GPIO19├── Ra-01 MISO
│           GPIO23├── Ra-01 MOSI  
│           GPIO18├── Ra-01 SCK
│            GPIO5├── Ra-01 NSS
│           GPIO14├── Ra-01 RST
│            GPIO2├── Ra-01 DIO0
│                 │
│           GPIO16├── NGIMU TX
│           GPIO17├── NGIMU RX
│               5V├── NGIMU VCC
└─────────────────┘
```

## การติดตั้ง

### 📥 Step 1: Download Code
```bash
git clone https://github.com/Teera235/CubeSat-LoRa-transmitter-
cd ultra-fast-cubesat-transmitter
```

### 📝 Step 2: Arduino IDE Configuration
1. เปิด Arduino IDE
2. File → Open → เลือกไฟล์ `.ino`
3. Tools → Board → ESP32 Dev Module
4. Tools → Upload Speed → 921600
5. Tools → CPU Frequency → 240MHz
6. Tools → Flash Size → 4MB
7. Tools → Partition Scheme → Default

### 🔄 Step 3: Library Installation
```cpp
// ใน Library Manager ค้นหาและติดตั้ง:
- "NGIMU" by x-io Technologies
```

### ⬆️ Step 4: Upload Code
1. เชื่อมต่อ ESP32 กับ USB
2. เลือก COM Port ที่ถูกต้อง
3. กด Upload (Ctrl+U)

## การใช้งาน

### 🚀 Quick Start
1. **Power On**: เชื่อมต่อ ESP32 กับ USB หรือ external power
2. **Monitor**: เปิด Serial Monitor (921600 baud)
3. **Check Status**: จะเห็นข้อความ initialization

### 📊 Serial Monitor Output
```
Initializing Ultra-Fast CubeSat Transmitter...
LoRa initialized successfully!
Ultra-Fast CubeSat Transmitter Ready!
Configuration:
- Max Range: 100m
- Target Rate: 200 Hz
- LoRa Settings: SF7, BW500kHz, CR4/5
- Packet Size: 12 bytes
- Time on Air: ~2ms per packet

TX Rate: 187 Hz, Total: 1247, Errors: 0, Avg Interval: 5347 us
Pitch: 12.34, Roll: -5.67, Yaw: 89.12
```

### 🎛️ Control Functions
โค้ดมี functions สำหรับควบคุม performance:

```cpp
// Performance Modes
enterHighPerformanceMode();  // 240MHz CPU
enterNormalMode();           // 160MHz CPU  
enterLowPowerMode();        // 80MHz CPU + LoRa sleep
```

### 📡 Data Format
แต่ละ packet ประกอบด้วย:
```cpp
struct EulerPacket {
    float pitch;  // 4 bytes
    float roll;   // 4 bytes  
    float yaw;    // 4 bytes
};
// Total: 12 bytes per packet
```

## Performance

### 📈 Expected Performance
| Parameter | Value |
|-----------|-------|
| **Transmission Rate** | 150-200 Hz |
| **Latency** | < 5ms |
| **Range** | 100m |
| **Packet Loss** | < 1% |
| **Time on Air** | ~2ms per packet |
| **Throughput** | ~14.4 kbps |

### 🔧 LoRa Configuration
- **Spreading Factor**: SF7 (fastest)
- **Bandwidth**: 500kHz (maximum)  
- **Coding Rate**: 4/5 (minimal overhead)
- **Output Power**: Maximum (20dBm)

## Troubleshooting

### ❌ Common Issues

#### 1. "LoRa initialization failed!"
**สาเหตุ**: การเชื่อมต่อ LoRa module ผิดพลาด
**แก้ไข**:
- ตรวจสอบการเชื่อมต่อสาย
- ตรวจสอบ power supply (3.3V)
- ลอง reset ESP32

#### 2. "SPI initialization failed!"
**สาเหตุ**: ปัญหา SPI pins
**แก้ไข**:
- ตรวจสอบ pin connections
- อาจมี pin conflict กับ peripherals อื่น

#### 3. No NGIMU data
**สาเหตุ**: การเชื่อมต่อ UART หรือ baud rate ผิด
**แก้ไข**:
- ตรวจสอบ TX/RX pins (16, 17)
- ตรวจสอบ NGIMU configuration
- ตรวจสอบ power supply (5V)

#### 4. Low transmission rate
**สาเหตุ**: Performance bottlenecks
**แก้ไข**:
- ตรวจสอบ CPU frequency (240MHz)
- ปิด unnecessary debugging
- ตรวจสอบ interrupt conflicts

### 🐛 Debug Tips
```cpp
// เพิ่ม debug code:
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
Serial.printf("CPU freq: %d MHz\n", getCpuFrequencyMhz());

// Monitor task status:
vTaskList(taskStatusBuffer);
Serial.println(taskStatusBuffer);
```

## การพัฒนาต่อ

### 🔧 Customization Options

#### 1. เปลี่ยน Transmission Rate
```cpp
// ใน transmissionTask function:
const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 10ms = 100Hz
```

#### 2. เพิ่ม Sensors
```cpp
// เพิ่ม GPS data:
typedef struct __attribute__((packed)) {
    float pitch, roll, yaw;
    float lat, lon;           // เพิ่ม GPS
    uint16_t checksum;        // เพิ่ม error detection
} ExtendedPacket;
```

#### 3. เปลี่ยน LoRa Settings
```cpp
// สำหรับ range มากกว่า 100m:
loraWriteReg(LORA_REG_MODEM_CONFIG1, 0x72);  // BW=125kHz, SF=8
loraWriteReg(LORA_REG_MODEM_CONFIG2, 0x84);  // SF=8
```

### 📈 Performance Tuning
1. **CPU Frequency**: เพิ่มเป็น 240MHz สำหรับ max performance
2. **Task Priorities**: ปรับ priority ตาม application
3. **Buffer Sizes**: เพิ่ม buffer สำหรับ burst data
4. **DMA**: ใช้ DMA สำหรับ SPI transfers

### 🔗 Integration Examples
- **Ground Station**: สร้าง receiver ด้วย ESP32 + LoRa
- **Data Logging**: บันทึกข้อมูลลง SD card
- **Web Interface**: สร้าง web server สำหรับ monitoring
- **Multiple Sensors**: เพิ่ม GPS, magnetometer, pressure sensors

## 📞 Support

### 🆘 Need Help?
- **GitHub Issues**: สำหรับ bug reports
- **Documentation**: ดู comments ในโค้ด
- **Community**: Arduino forums, ESP32 community

### 📚 Useful Resources
- [ESP32 Arduino Core Documentation](https://docs.espressif.com/projects/arduino-esp32/)
- [LoRa SX1278 Datasheet](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1278)
- [NGIMU User Manual](https://x-io.co.uk/ngimu/)

---

**⚠️ หมายเหตุ**: โค้ดนี้ปรับแต่งสำหรับ performance สูงสุด อาจต้องปรับแต่งเพิ่มเติมตาม hardware และ environment จริง

**🚀 Happy Coding!**
