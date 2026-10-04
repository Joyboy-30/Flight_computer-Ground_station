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

// --- Backup / Redundant Ejection Detector -----------------------------------
// Independent second decision path ported from ejection_code.ino. Fires the
// same relay as the primary state machine if the primary detector never
// reaches STATE_APOGEE_LOCKED. See BackupEjection.h for the detection logic.
// !! VERIFY BACKUP_RELAY_PIN AGAINST YOUR ACTUAL WIRING BEFORE FLYING !!
// (GPIO4 matches the original ejection_code.ino relay pin; it also matches
// this file's PIN_BUZZER above, which is currently unused in rocket_flight.ino
// -- if GPIO4 is genuinely your buzzer, change the value below instead.)
#define BACKUP_RELAY_PIN         4
#define BACKUP_SEA_LEVEL_HPA     1004.0f // same reference pressure as ejection_code.ino
#define BACKUP_BASE_READS        2000    // calibration samples (spread non-blocking across loop())
#define BACKUP_READS_PER_CYCLE   5       // averaged samples per evaluation (reduced from the
                                          // original's 50 so the main 10Hz loop is never stalled)
#define BACKUP_LAUNCH_ALT_M      5.0f    // meters above baseline => launch detected
#define BACKUP_APOGEE_DROP_M     1.5f    // meters fallen from max => backup ejection trigger
#define BACKUP_EEPROM_SIZE       512
