/******************************************************************************
 * ULTRA-FAST CUBESAT SENDER - REAL-TIME OPTIMIZATION
 * 
 * Range: 100m maximum
 * Target: Maximum real-time transmission speed
 * MCU: ESP32 (240MHz)
 * Sensors: NGIMU  
 * Communication: LoRa Ra-01
 ******************************************************************************/

#include <SPI.h>
#include <NgimuReceive.h>
#include <esp_task_wdt.h>
#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// LoRa Pin Definitions
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

// LoRa Registers
#define LORA_REG_FIFO 0x00
#define LORA_REG_OP_MODE 0x01
#define LORA_REG_FRF_MSB 0x06
#define LORA_REG_FRF_MID 0x07
#define LORA_REG_FRF_LSB 0x08
#define LORA_REG_PA_CONFIG 0x09
#define LORA_REG_FIFO_ADDR_PTR 0x0D
#define LORA_REG_FIFO_TX_BASE_ADDR 0x0E
#define LORA_REG_FIFO_RX_BASE_ADDR 0x0F
#define LORA_REG_IRQ_FLAGS 0x12
#define LORA_REG_MODEM_CONFIG1 0x1D
#define LORA_REG_MODEM_CONFIG2 0x1E
#define LORA_REG_MODEM_CONFIG3 0x26
#define LORA_REG_PAYLOAD_LENGTH 0x22
#define LORA_REG_DIO_MAPPING1 0x40
#define LORA_REG_VERSION 0x42

// LoRa Modes
#define LORA_MODE_SLEEP 0x00
#define LORA_MODE_STDBY 0x01
#define LORA_MODE_TX 0x03
#define LORA_MODE_RXCONT 0x05

// Ultra-fast settings for 100m range
#define PACKET_SIZE 12
#define MAX_TRANSMISSION_RATE_HZ 200  // 200Hz = 5ms interval
#define LORA_FREQUENCY 433000000      // 433MHz

// Ultra-fast data structures with cache optimization
typedef struct __attribute__((packed, aligned(4))) {
    float pitch;
    float roll; 
    float yaw;
} EulerPacket;

// High-speed globals with proper alignment
static volatile EulerPacket __attribute__((aligned(32))) currentData;
static volatile EulerPacket __attribute__((aligned(32))) transmitData;
static volatile bool __attribute__((aligned(4))) newDataAvailable = false;
static volatile bool __attribute__((aligned(4))) transmissionActive = false;
static volatile uint32_t __attribute__((aligned(4))) packetsSent = 0;
static volatile uint32_t __attribute__((aligned(4))) transmissionErrors = 0;

// Hardware SPI handle
static spi_device_handle_t spi_handle;

// Synchronization primitives
static SemaphoreHandle_t dataMutex;
static SemaphoreHandle_t txCompleteSemaphore;

// NGIMU objects
NgimuSensors ngimuSensors;
NgimuQuaternion ngimuQuaternion;
NgimuEuler ngimuEuler;

// Performance monitoring
static uint64_t lastTransmissionTime = 0;
static uint32_t averageTransmissionInterval = 0;

// Ultra-fast SPI setup with maximum speed
esp_err_t setupUltraFastSPI() {
    // SPI bus configuration
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = 23;
    buscfg.miso_io_num = 19;
    buscfg.sclk_io_num = 18;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 64;
    
    // SPI device configuration - Maximum speed for short range
    spi_device_interface_config_t devcfg = {};
    devcfg.command_bits = 0;
    devcfg.address_bits = 0;
    devcfg.dummy_bits = 0;
    devcfg.mode = 0;                    // SPI mode 0
    devcfg.duty_cycle_pos = 0;
    devcfg.cs_ena_pretrans = 0;
    devcfg.cs_ena_posttrans = 0;
    devcfg.clock_speed_hz = 10000000;   // 10MHz SPI clock
    devcfg.input_delay_ns = 0;
    devcfg.spics_io_num = LORA_SS;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    devcfg.queue_size = 7;
    devcfg.pre_cb = NULL;
    devcfg.post_cb = NULL;
    
    esp_err_t ret = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) return ret;
    
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi_handle);
    return ret;
}

// Ultra-fast LoRa register operations
static inline esp_err_t __attribute__((always_inline)) loraWriteReg(uint8_t reg, uint8_t value) {
    spi_transaction_t trans = {};
    trans.flags = SPI_TRANS_USE_TXDATA;
    trans.length = 16;
    trans.tx_data[0] = reg | 0x80;  // Write bit
    trans.tx_data[1] = value;
    trans.tx_data[2] = 0;
    trans.tx_data[3] = 0;
    
    return spi_device_transmit(spi_handle, &trans);
}

static inline esp_err_t __attribute__((always_inline)) loraReadReg(uint8_t reg, uint8_t* value) {
    spi_transaction_t trans = {};
    trans.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    trans.length = 16;
    trans.tx_data[0] = reg & 0x7F;  // Read bit
    trans.tx_data[1] = 0x00;
    trans.tx_data[2] = 0;
    trans.tx_data[3] = 0;
    
    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret == ESP_OK) {
        *value = trans.rx_data[1];
    }
    return ret;
}

// Burst write to FIFO - optimized for speed
static inline esp_err_t __attribute__((always_inline)) loraWriteFIFO(const uint8_t* data, uint8_t length) {
    static uint8_t __attribute__((aligned(4))) txBuffer[32];
    txBuffer[0] = LORA_REG_FIFO | 0x80;
    memcpy(&txBuffer[1], data, length);
    
    spi_transaction_t trans = {};
    trans.length = (1 + length) * 8;
    trans.tx_buffer = txBuffer;
    
    return spi_device_transmit(spi_handle, &trans);
}

// Initialize LoRa with ultra-fast settings for 100m range
esp_err_t initLoRaUltraFast() {
    // Reset LoRa module
    gpio_set_level((gpio_num_t)LORA_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)LORA_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Check version
    uint8_t version;
    if (loraReadReg(LORA_REG_VERSION, &version) != ESP_OK || version != 0x12) {
        return ESP_FAIL;
    }
    
    // Enter sleep mode
    loraWriteReg(LORA_REG_OP_MODE, LORA_MODE_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Set LoRa mode
    loraWriteReg(LORA_REG_OP_MODE, 0x80);
    
    // Set frequency (433MHz)
    uint64_t frf = ((uint64_t)LORA_FREQUENCY << 19) / 32000000;
    loraWriteReg(LORA_REG_FRF_MSB, (uint8_t)(frf >> 16));
    loraWriteReg(LORA_REG_FRF_MID, (uint8_t)(frf >> 8));
    loraWriteReg(LORA_REG_FRF_LSB, (uint8_t)(frf >> 0));
    
    // Ultra-fast settings for 100m range
    // BW=500kHz, CR=4/5, SF=7 (fastest possible)
    loraWriteReg(LORA_REG_MODEM_CONFIG1, 0x92);  // BW=500kHz, CR=4/5, Implicit header off
    loraWriteReg(LORA_REG_MODEM_CONFIG2, 0x74);  // SF=7, TxContinuousMode=0, RxPayloadCrcOn=1
    loraWriteReg(LORA_REG_MODEM_CONFIG3, 0x00);  // LowDataRateOptimize=0, AgcAutoOn=0
    
    // Set maximum output power for 100m range
    loraWriteReg(LORA_REG_PA_CONFIG, 0xFF);      // Max power
    
    // Set base addresses
    loraWriteReg(LORA_REG_FIFO_TX_BASE_ADDR, 0x00);
    loraWriteReg(LORA_REG_FIFO_RX_BASE_ADDR, 0x80);
    
    // Configure DIO0 for TxDone
    loraWriteReg(LORA_REG_DIO_MAPPING1, 0x40);
    
    // Enter standby mode
    loraWriteReg(LORA_REG_OP_MODE, LORA_MODE_STDBY);
    
    return ESP_OK;
}

// Critical: Ultra-fast LoRa transmission
void IRAM_ATTR sendLoRaPacket() {
    if (transmissionActive) return;
    
    transmissionActive = true;
    uint64_t startTime = esp_timer_get_time();
    
    // Atomic data copy
    EulerPacket localData;
    portDISABLE_INTERRUPTS();
    memcpy(&localData, (void*)&transmitData, sizeof(EulerPacket));
    portENABLE_INTERRUPTS();
    
    // Prepare for transmission
    loraWriteReg(LORA_REG_OP_MODE, LORA_MODE_STDBY);
    loraWriteReg(LORA_REG_FIFO_ADDR_PTR, 0x00);
    loraWriteReg(LORA_REG_PAYLOAD_LENGTH, PACKET_SIZE);
    
    // Clear IRQ flags
    loraWriteReg(LORA_REG_IRQ_FLAGS, 0xFF);
    
    // Write data to FIFO
    if (loraWriteFIFO((uint8_t*)&localData, PACKET_SIZE) != ESP_OK) {
        transmissionErrors++;
        transmissionActive = false;
        return;
    }
    
    // Start transmission
    loraWriteReg(LORA_REG_OP_MODE, LORA_MODE_TX);
    
    // Update timing statistics
    if (lastTransmissionTime > 0) {
        uint32_t interval = (uint32_t)(startTime - lastTransmissionTime);
        averageTransmissionInterval = (averageTransmissionInterval * 7 + interval) / 8;
    }
    lastTransmissionTime = startTime;
}

// Hardware interrupt for TX completion
void IRAM_ATTR onLoRaTxDone() {
    // Clear IRQ flags
    loraWriteReg(LORA_REG_IRQ_FLAGS, 0xFF);
    
    // Return to standby
    loraWriteReg(LORA_REG_OP_MODE, LORA_MODE_STDBY);
    
    transmissionActive = false;
    packetsSent++;
    
    // Signal completion
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(txCompleteSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// NGIMU callbacks - ultra-fast processing
void IRAM_ATTR ngimuSensorsCallback(const NgimuSensors ngimuSensorsData) {
    // Skip sensors data for maximum speed
}

void IRAM_ATTR ngimuQuaternionCallback(const NgimuQuaternion ngimuQuaternionData) {
    // Skip quaternion data for maximum speed  
}

void IRAM_ATTR ngimuEulerCallback(const NgimuEuler ngimuEulerData) {
    // Ultra-fast atomic update with mutex
    if (xSemaphoreTakeFromISR(dataMutex, NULL) == pdTRUE) {
        currentData.pitch = ngimuEulerData.pitch;
        currentData.roll = ngimuEulerData.roll;
        currentData.yaw = ngimuEulerData.yaw;
        newDataAvailable = true;
        xSemaphoreGiveFromISR(dataMutex, NULL);
    }
}

// High-priority transmission task - Real-time optimized
void IRAM_ATTR transmissionTask(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(5);  // 5ms = 200Hz maximum
    
    for(;;) {
        // Check for new data
        if (newDataAvailable && !transmissionActive) {
            // Copy data for transmission
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
                memcpy((void*)&transmitData, (void*)&currentData, sizeof(EulerPacket));
                newDataAvailable = false;
                xSemaphoreGive(dataMutex);
                
                // Send packet
                sendLoRaPacket();
            }
        }
        
        // Precise timing for real-time performance
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// IMU processing task - Optimized for throughput
void IRAM_ATTR imuTask(void* parameter) {
    const TickType_t xBlockTime = pdMS_TO_TICKS(1);
    
    for(;;) {
        // Process available bytes in batches
        int available = Serial1.available();
        if (available > 0) {
            // Process up to 32 bytes at once for optimal throughput
            int processCount = (available > 32) ? 32 : available;
            for (int i = 0; i < processCount; i++) {
                NgimuReceiveProcessSerialByte(Serial1.read());
            }
        } else {
            // Yield CPU if no data available
            vTaskDelay(xBlockTime);
        }
    }
}

// Statistics and monitoring task
void statisticsTask(void* parameter) {
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
        
        // Calculate transmission rate
        static uint32_t lastPacketCount = 0;
        uint32_t packetsThisSecond = packetsSent - lastPacketCount;
        lastPacketCount = packetsSent;
        
        // Print statistics
        Serial.printf("TX Rate: %lu Hz, Total: %lu, Errors: %lu, Avg Interval: %lu us\n",
                     packetsThisSecond, packetsSent, transmissionErrors, 
                     averageTransmissionInterval);
        
        // Print current data
        Serial.printf("Pitch: %.2f, Roll: %.2f, Yaw: %.2f\n",
                     transmitData.pitch, transmitData.roll, transmitData.yaw);
    }
}

void setup() {
    // Disable watchdog for setup
    esp_task_wdt_delete(NULL);
    
    // Set CPU to maximum frequency
    setCpuFrequencyMhz(240);
    
    // Initialize serial communications
    Serial.begin(921600);
    Serial1.begin(460800, SERIAL_8N1, 16, 17);
    
    Serial.println("Initializing Ultra-Fast CubeSat Transmitter...");
    
    // Setup GPIO
    gpio_set_direction((gpio_num_t)LORA_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)LORA_DIO0, GPIO_MODE_INPUT);
    
    // Initialize SPI
    if (setupUltraFastSPI() != ESP_OK) {
        Serial.println("ERROR: SPI initialization failed!");
        return;
    }
    
    // Initialize LoRa
    if (initLoRaUltraFast() != ESP_OK) {
        Serial.println("ERROR: LoRa initialization failed!");
        return;
    }
    
    Serial.println("LoRa initialized successfully!");
    
    // Create synchronization objects
    dataMutex = xSemaphoreCreateMutex();
    txCompleteSemaphore = xSemaphoreCreateBinary();
    
    if (dataMutex == NULL || txCompleteSemaphore == NULL) {
        Serial.println("ERROR: Failed to create synchronization objects!");
        return;
    }
    
    // Setup interrupt for TX completion
    attachInterrupt(digitalPinToInterrupt(LORA_DIO0), onLoRaTxDone, RISING);
    
    // Initialize NGIMU
    NgimuReceiveInitialise();
    NgimuReceiveSetSensorsCallback(ngimuSensorsCallback);
    NgimuReceiveSetQuaternionCallback(ngimuQuaternionCallback);
    NgimuReceiveSetEulerCallback(ngimuEulerCallback);
    
    // Create high-priority tasks with proper stack sizes
    xTaskCreatePinnedToCore(
        transmissionTask,
        "TX_Task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Highest priority
        NULL,
        1  // Core 1
    );
    
    xTaskCreatePinnedToCore(
        imuTask,
        "IMU_Task", 
        4096,
        NULL,
        configMAX_PRIORITIES - 2,  // Second highest priority
        NULL,
        0  // Core 0
    );
    
    // Statistics task (lower priority)
    xTaskCreatePinnedToCore(
        statisticsTask,
        "Stats_Task", 
        3072,
        NULL,
        1,  // Low priority
        NULL,
        0  // Core 0
    );
    
    Serial.println("Ultra-Fast CubeSat Transmitter Ready!");
    Serial.println("Configuration:");
    Serial.printf("- Max Range: 100m\n");
    Serial.printf("- Target Rate: %d Hz\n", MAX_TRANSMISSION_RATE_HZ);
    Serial.printf("- LoRa Settings: SF7, BW500kHz, CR4/5\n");
    Serial.printf("- Packet Size: %d bytes\n", PACKET_SIZE);
    Serial.printf("- Time on Air: ~2ms per packet\n");
}

void loop() {
    // Main loop does minimal work - everything handled by tasks
    vTaskDelay(pdMS_TO_TICKS(10000));  // 10 second delay
    
    // Watchdog management
    esp_task_wdt_reset();
}

// Power management functions for space environment
void enterHighPerformanceMode() {
    setCpuFrequencyMhz(240);
    Serial.println("Entered High Performance Mode (240MHz)");
}

void enterNormalMode() {
    setCpuFrequencyMhz(160);
    Serial.println("Entered Normal Mode (160MHz)");
}

void enterLowPowerMode() {
    setCpuFrequencyMhz(80);
    loraWriteReg(LORA_REG_OP_MODE, LORA_MODE_SLEEP);
    Serial.println("Entered Low Power Mode (80MHz)");
}
