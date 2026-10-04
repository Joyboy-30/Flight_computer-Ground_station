#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_BMP280.h>
#include <esp_task_wdt.h>
#include "Config.h"
#include "Protocol.h"
#include "SensorFilter.h"
#include "FlashLogger.h"
#include "BackupEjection.h"

#define WDT_TIMEOUT_SECONDS 5

// --- RTC RAM State Retention ---
RTC_DATA_ATTR FlightState rtc_state = STATE_BOOT;
RTC_DATA_ATTR float rtc_launch_pressure = 0.0f;
RTC_DATA_ATTR float rtc_max_altitude = 0.0f;

// --- Global Objects ---
Adafruit_BMP280 bmp;
SensorFilter altFilter(FILTER_EMA_ALPHA);
FlashLogger logger;

// --- State Variables ---
float currentAltitude = 0.0f;
float maxAltitude = 0.0f;
uint16_t packetSequence = 0;
int apogeePersistenceCounter = 0;
int apogeeBurstCounter = 0;
uint32_t lastTelemetryTime = 0;
uint32_t lastLogTime = 0;

// Endian swap utility
uint16_t swapEndian(uint16_t val) {
    return (val << 8) | (val >> 8);
}

void setup() {
    Serial.begin(115200);

    // Setup Watchdog Timer
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);

    pinMode(PIN_LED_SENS, OUTPUT);
    pinMode(PIN_LED_TX, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);

    // Initialize Flash Logger
    if (!logger.begin()) {
        Serial.println("WARNING: Flash logging disabled.");
    }

    // ONLY wait for DUMP commands if this is a fresh ground boot. 
    // If the battery bounced mid-flight, we need to bypass this instantly!
    if (rtc_state == STATE_BOOT || rtc_state == STATE_LANDED || rtc_state == STATE_READY) {
        delay(1000); // Give user time to open terminal
        Serial.println("Send 'DUMP' within 3 seconds to read blackbox data, or 'ERASE' to clear.");
        uint32_t bootTime = millis();
        while (millis() - bootTime < 3000) {
            if (Serial.available()) {
                String cmd = Serial.readStringUntil('\n');
                cmd.trim();
                if (cmd == "DUMP") {
                    logger.dumpLogToSerial();
                } else if (cmd == "ERASE") {
                    logger.eraseLog();
                }
            }
            esp_task_wdt_reset();
            delay(10); 
        }
    }

    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
    LoRa.setPins(PIN_LORA_NSS, PIN_LORA_RST, PIN_LORA_DIO0);
    
    if (!LoRa.begin(LORA_FREQ)) {
        Serial.println("CRITICAL: LoRa init failed.");
        while(1) {
            digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
            delay(100); 
        }
    }
    
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREADFACTOR);
    LoRa.setCodingRate4(LORA_CODINGRATE);
    LoRa.setPreambleLength(LORA_PREAMBLE);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setTxPower(LORA_TX_POWER, PA_OUTPUT_PA_BOOST_PIN);
    LoRa.enableCrc();

    Wire.begin(PIN_BMP_SDA, PIN_BMP_SCL);
    Wire.setTimeOut(100);
    
    if (!bmp.begin(0x76, BMP280_CHIPID)) {
        if (!bmp.begin(0x77, BMP280_CHIPID)) {
            Serial.println("CRITICAL: BMP280 init failed.");
            while(1) {
                digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
                delay(200);
            }
        }
    }

    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                    Adafruit_BMP280::SAMPLING_X2,     
                    Adafruit_BMP280::SAMPLING_X8,    
                    Adafruit_BMP280::FILTER_X4,      // Lower hardware IIR filter to prevent lag in fast rocket
                    Adafruit_BMP280::STANDBY_MS_1);   

    if (rtc_state == STATE_BOOT || rtc_state == STATE_LANDED) {
        Serial.println("Establishing new launch reference...");
        rtc_state = STATE_READY;
        float p_sum = 0;
        for (int i = 0; i < 50; i++) {
            p_sum += bmp.readPressure();
            delay(20);
        }
        rtc_launch_pressure = (p_sum / 50.0f) / 100.0f;
        rtc_max_altitude = 0.0f;
        Serial.printf("Launch Pressure Set: %.2f hPa\n", rtc_launch_pressure);
    } else {
        Serial.printf("Recovered Mid-Flight! State: %d, P0: %.2f\n", rtc_state, rtc_launch_pressure);
        maxAltitude = rtc_max_altitude;
    }

    initBackupEjection();
}

void transmitTelemetry(uint8_t type, float altitude) {
    digitalWrite(PIN_LED_TX, HIGH);
    TelemetryPacket pkt;
    pkt.rocket_id = ROCKET_ID;
    pkt.type = type;
    pkt.sequence = swapEndian(packetSequence++);
    
    // Prevent negative altitude noise on the launch pad from underflowing 
    // the unsigned 16-bit integer and wrapping around to 6553.5 meters.
    if (altitude < 0.0f) {
        altitude = 0.0f;
    }
    
    uint16_t alt_encoded = (uint16_t)round(altitude * 10.0f);
    pkt.altitude = swapEndian(alt_encoded);

    LoRa.beginPacket();
    LoRa.write((uint8_t*)&pkt, sizeof(TelemetryPacket));
    LoRa.endPacket(true);
    digitalWrite(PIN_LED_TX, LOW);
}

void loop() {
    esp_task_wdt_reset();
    uint32_t now = millis();
    
    checkBackupEjection();

    float p = bmp.readPressure() / 100.0f;
    float raw_altitude = 44330.0 * (1.0 - pow(p / rtc_launch_pressure, 0.1903));
    currentAltitude = altFilter.update(raw_altitude);
    
    digitalWrite(PIN_LED_SENS, !digitalRead(PIN_LED_SENS));

    if (currentAltitude > maxAltitude) {
        maxAltitude = currentAltitude;
        rtc_max_altitude = maxAltitude;
    }

    // High frequency internal logging (approx 20Hz, writing in batches of 10)
    if (now - lastLogTime >= 50) { 
        logger.log(now, rtc_state, currentAltitude, maxAltitude);
        lastLogTime = now;
    }

    switch (rtc_state) {
        case STATE_READY:
            if (currentAltitude > LAUNCH_ALT_THRESHOLD) {
                rtc_state = STATE_ASCENT;
                logger.forceFlush();
            }
            break;
            
        case STATE_ASCENT:
            if (currentAltitude < maxAltitude - APOGEE_FALL_THRESHOLD) {
                apogeePersistenceCounter++;
                if (apogeePersistenceCounter >= APOGEE_PERSISTENCE) {
                    rtc_state = STATE_APOGEE_LOCKED;
                    logger.forceFlush();
                    apogeeBurstCounter = 0;
                }
            } else {
                apogeePersistenceCounter = 0;
            }
            break;
            
        case STATE_APOGEE_LOCKED:
            fireEjectionRelay(EJECT_REASON_PRIMARY);
            rtc_state = STATE_DESCENT;
            break;
            
        case STATE_DESCENT:
            // Very basic landing detection: below launch threshold
            if (currentAltitude < 5.0f) {
                rtc_state = STATE_LANDED;
                logger.forceFlush();
            }
            break;
    }

    if (rtc_state == STATE_APOGEE_LOCKED || (apogeeBurstCounter > 0 && apogeeBurstCounter < APOGEE_BURST_COUNT)) {
        if (now - lastTelemetryTime >= APOGEE_BURST_INTERVAL) {
            transmitTelemetry(PKT_TYPE_APOGEE, maxAltitude);
            lastTelemetryTime = now;
            apogeeBurstCounter++;
        }
    } else {
        if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
            transmitTelemetry(PKT_TYPE_NORMAL_ALTITUDE, currentAltitude);
            lastTelemetryTime = now;
        }
    }
    
    // Check if user is asking for a log dump mid-flight (or while landed)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "DUMP") {
            logger.forceFlush();
            logger.dumpLogToSerial();
        }
    }

    delay(10); 
}