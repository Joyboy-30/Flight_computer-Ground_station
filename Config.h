#pragma once

// --- Hardware Pins (Standardized per spec) ---
#define PIN_BMP_SDA  21
#define PIN_BMP_SCL  22

#define PIN_LORA_MISO 19
#define PIN_LORA_MOSI 23
#define PIN_LORA_SCK  18
#define PIN_LORA_NSS  5
#define PIN_LORA_RST  14
#define PIN_LORA_DIO0 26

#define PIN_LED_SENS   27
#define PIN_LED_TX     25
#define PIN_LED_STATUS 33
#define PIN_BUZZER     4

// --- LoRa Configuration (ABSOLUTE MAX RANGE MODE) ---
#define LORA_FREQ         434.5E6 // 434.500 MHz
#define LORA_BANDWIDTH    125E3   // 125 kHz
#define LORA_SPREADFACTOR 7       // SF7 (Fastest transmission)
#define LORA_CODINGRATE   8       // 4/8 Maximum Error Correction to recover corrupted packets
#define LORA_PREAMBLE     8
#define LORA_SYNC_WORD    0x12    // Standard LoRa Sync Word
#define LORA_TX_POWER     20      // Max power (20dBm requires PA_BOOST pin)

// --- Flight Dynamics & Filtering Config ---
// Barometric moving average weight (0.0 to 1.0). Lower = more smoothing, higher = less lag.
// For a fast moving rocket, we want less software lag!
#define FILTER_EMA_ALPHA       0.4f
#define MEDIAN_WINDOW_SIZE     5

// Launch Detection (TBD in spec, establishing baseline)
#define LAUNCH_ALT_THRESHOLD   10.0f  // meters above P0 to declare launch
#define LAUNCH_ACCEL_THRESHOLD 15.0f  // m/s estimated velocity to confirm launch

// Apogee Detection
// Require X consecutive falling samples before declaring apogee to prevent false positives from noise.
#define APOGEE_FALL_THRESHOLD  2.0f   // meters below max altitude
#define APOGEE_PERSISTENCE     10     // samples

// Telemetry Timing
// 10 Hz = 100 ms per packet (safer overhead for SF7 125kHz ToA)
#define TELEMETRY_INTERVAL_MS  100     // 10Hz telemetry interval
#define APOGEE_BURST_INTERVAL  100     // 10Hz
#define APOGEE_BURST_COUNT     10     // 10 packets for apogee lock mark

// IDs
#define ROCKET_ID              37     // Standard test ID
