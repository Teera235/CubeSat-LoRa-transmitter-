# CubeSat-LoRa-Transmitter

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Communication](https://img.shields.io/badge/Communication-LoRa-green.svg)](https://lora-alliance.org/)

A comprehensive LoRa-based communication system designed specifically for CubeSat missions, enabling reliable long-range telemetry transmission with minimal power consumption.

##  Project Overview

The CubeSat-LoRa-Transmitter project implements a complete communication solution that enables CubeSats to transmit critical telemetry and sensor data to ground stations using LoRa (Long Range) modulation technology. This system is optimized for small satellite platforms with stringent power and size constraints.

###  Key Features

- **Long-Range Communication**: Utilizes LoRa technology for transmission ranges up to 20km+ (line of sight)
- **Ultra-Low Power Consumption**: Optimized for battery-powered CubeSat operations
- **Real-time Telemetry**: Continuous transmission of IMU data, system status, and environmental parameters
- **Robust Signal Processing**: Advanced error correction and signal reliability mechanisms
- **Ground Station Interface**: Professional mission control visualization system
- **Modular Architecture**: Easy integration with existing CubeSat platforms

##  Repository Structure

```
CubeSat-LoRa-Transmitter/
├── IMU_UI/                          
│   ├── CubeSat_Visualizer.pde      
│   ├── model/                      
├── NGIMU-Pico2W-LoRa-Sender/       
│   ├── main.cpp                    
├── R4-LoRa-Receiver/              
    ├── receiver.ino                

```
- **Software Requirements:**
  - Arduino IDE 2.0+
  - Processing 4.0+
  - Required libraries (see `requirements.txt`)

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yourusername/CubeSat-LoRa-Transmitter.git
   cd CubeSat-LoRa-Transmitter
   ```

## 📊 System Architecture

### Communication Protocol

The system implements a custom packet structure optimized for telemetry transmission:

```
| Header | Packet ID | Timestamp | IMU Data | System Status | Checksum |
|   2B   |    1B     |    4B     |   24B    |      8B       |    2B    |
```

### Data Flow

```mermaid
graph TD
    A[CubeSat IMU] → B[Pico2W Processor]
    B → C[LoRa Transmitter]
    C → D[Ground Station Receiver]
    D → E[Data Processing]
    E → F[Mission Control UI]
    F → G[Telemetry Database]
```

## Configuration

### LoRa Parameters

Key configuration parameters in `lora_config.h`:

```cpp
#define LORA_FREQUENCY      433.0E6    // Frequency in Hz
#define LORA_BANDWIDTH      125E3      // Bandwidth in Hz
#define LORA_SPREADING_FACTOR  12      // Spreading factor (7-12)
#define LORA_CODING_RATE    5          // Coding rate (5-8)
#define LORA_TX_POWER       20         // Transmission power (dBm)
#define LORA_SYNC_WORD      0x12       // Sync word
```


