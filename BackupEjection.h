#pragma once
// =====================================================================================
// BackupEjection.h -- Redundant / backup apogee-ejection detector
// -------------------------------------------------------------------------------------
// Ported from the user's original standalone ejection_code.ino (ESP32-S2-DevKitM-1
// ground-up ejection controller) and integrated as a SECOND, ALGORITHMICALLY
// INDEPENDENT decision path alongside the primary flight-state-machine detector
// already in rocket_flight.ino.
//
// Design intent: if the primary apogee detector (rtc_state reaching
// STATE_APOGEE_LOCKED in rocket_flight.ino) never fires -- a stuck state, a
// filter/EMA bug, a stalled transition -- this module keeps running its OWN
// calibration, its OWN max-altitude tracking, and its OWN apogee-drop threshold,
// using the same formula/constants as the original ejection_code.ino
// (bmp.readAltitude(SEA_LEVEL_HPA), 5 m launch threshold, 1.5 m apogee-drop
// threshold, EEPROM blackbox write), and can command the relay entirely on its own.
//
// Both paths converge on ONE relay-firing function (fireEjectionRelay) so the relay
// is only ever commanded once no matter which path trips first, and a telemetry
// event is sent reporting which path fired it.
//
// ADAPTATIONS MADE TO INTEGRATE SAFELY (flagged here, not hidden):
//   1. The original's blocking 2000-sample calibration for-loop and 50-sample
//      per-loop averaging would stall rocket_flight.ino's 10Hz LoRa telemetry and
//      its hardware watchdog (5s timeout) if run inline. Both are restructured to
//      accumulate a few samples per loop() pass instead of blocking -- same total
//      sample counts and same math, just spread out over time (calibration still
//      takes on the order of the original's own startup delay).
//   2. Backup state (baseline, running max, launch flag, fired latch) is stored in
//      RTC_DATA_ATTR RAM, mirroring the rtc_* pattern the primary detector already
//      uses, so a mid-flight brownout/reset doesn't wipe out backup progress (it
//      re-arms fresh only on a real ground boot, exactly when the primary does).
//
// SHARED-SENSOR CAVEAT: both detectors read the same physical BMP280. This gives
// software/algorithmic redundancy (two independent decision paths), not
// sensor-hardware redundancy -- a wiring or sensor failure can still defeat both.
// True hardware redundancy needs a second, independently wired barometer.
//
// !! VERIFY BACKUP_RELAY_PIN AGAINST YOUR ACTUAL WIRING BEFORE FLYING !! See below.
// =====================================================================================

#include <Arduino.h>
#include <EEPROM.h>
#include <Adafruit_BMP280.h>
#include "Protocol.h"

// ---- Relay pin --------------------------------------------------------------------
// The original ejection_code.ino drove its relay on GPIO4. This project's Config.h
// defines PIN_BUZZER as GPIO4, but nothing in rocket_flight.ino currently does a
// pinMode()/digitalWrite() on it -- so GPIO4 is unused at the software level today.
// CONFIRM against your actual wiring before flashing: if GPIO4 is genuinely your
// buzzer, override BACKUP_RELAY_PIN in Config.h to whichever GPIO the ejection
// relay is really wired to.
#ifndef BACKUP_RELAY_PIN
#define BACKUP_RELAY_PIN 4
#endif

// ---- Backup detection tuning (ported 1:1 from ejection_code.ino unless noted) -----
#ifndef BACKUP_SEA_LEVEL_HPA
#define BACKUP_SEA_LEVEL_HPA 1004.0f   // same fixed reference the original used
#endif
#ifndef BACKUP_BASE_READS
#define BACKUP_BASE_READS 2000         // calibration samples -- spread non-blocking
                                        // across many loop() passes (see note above)
#endif
#ifndef BACKUP_READS_PER_CYCLE
#define BACKUP_READS_PER_CYCLE 5       // averaged samples per evaluation (original
                                        // used 50; reduced so this module never
                                        // stalls the 10Hz main loop -- "alright to
                                        // calculate extra for the readings", but not
                                        // at the cost of radio/watchdog timing)
#endif
#ifndef BACKUP_LAUNCH_ALT_M
#define BACKUP_LAUNCH_ALT_M 5.0f       // identical to original LAUNCH_ALT_M
#endif
#ifndef BACKUP_APOGEE_DROP_M
#define BACKUP_APOGEE_DROP_M 1.5f      // identical to original APOGEE_DROP_M
#endif
#ifndef BACKUP_EEPROM_SIZE
#define BACKUP_EEPROM_SIZE 512         // identical to original EEPROM_SIZE
#endif

// ---- State retained across resets/brownouts (mirrors the rtc_* pattern already
//      used for the primary detector in rocket_flight.ino) -------------------------
RTC_DATA_ATTR bool  backupBaselineSet  = false;
RTC_DATA_ATTR float backupBaseline     = 0.0f;   // averaged calibration altitude
RTC_DATA_ATTR float backupMaxAltitude  = 0.0f;
RTC_DATA_ATTR int   backupLaunchDetect = 0;
RTC_DATA_ATTR bool  backupFired        = false;
RTC_DATA_ATTR int   backupEepromAddr   = 0;

// ---- Runtime-only accumulators -----------------------------------------------------
static long  backupCalSampleCount = 0;
static float backupCalSum         = 0.0f;
static float backupCycleSum       = 0.0f;
static int   backupCycleCount     = 0;
static int   backupEepromSlot     = 0;

// ---- Cross-path flags / event reason codes ("specific flags and event" requested) --
volatile bool    ejectionRelayFired = false;  // latched true once EITHER path fires
volatile uint8_t ejectionFiredBy    = 0;      // 0 = not fired yet
#define EJECT_REASON_PRIMARY 1
#define EJECT_REASON_BACKUP  2

// ---- External hooks into rocket_flight.ino -----------------------------------------
extern Adafruit_BMP280 bmp;
extern FlightState rtc_state;
extern float maxAltitude;
extern void transmitTelemetry(uint8_t type, float altitude);

// Single source of truth for "fire the relay". Called from EITHER the primary
// STATE_APOGEE_LOCKED transition in rocket_flight.ino, or from this module's own
// independent detector below. Whichever trips first wins -- the other call becomes
// a no-op because of the ejectionRelayFired latch, so the relay only ever fires once.
void fireEjectionRelay(uint8_t reason) {
    if (ejectionRelayFired) return;
    ejectionRelayFired = true;
    ejectionFiredBy = reason;

    // Physical relay command FIRST, before any logging/radio work.
    digitalWrite(BACKUP_RELAY_PIN, LOW);   // active-low, same polarity as ejection_code.ino
    backupFired = true;

    Serial.print("EJECTION RELAY FIRED -- reason: ");
    Serial.println(reason == EJECT_REASON_PRIMARY ? "PRIMARY (state machine)" : "BACKUP (independent detector)");

    float reportAlt = (reason == EJECT_REASON_PRIMARY) ? maxAltitude : backupMaxAltitude;
    transmitTelemetry(PKT_TYPE_EJECTION_EVENT, reportAlt);
}

// Call once from setup(), after the primary rtc_state boot/recovery decision has
// already been made, so this module makes the identical "fresh boot vs. recovered
// mid-flight" call as the primary detector.
void initBackupEjection() {
    pinMode(BACKUP_RELAY_PIN, OUTPUT);
    digitalWrite(BACKUP_RELAY_PIN, HIGH);  // idle/safe

    EEPROM.begin(BACKUP_EEPROM_SIZE);
    backupEepromSlot = EEPROM.read(backupEepromAddr);

    if (rtc_state == STATE_BOOT || rtc_state == STATE_LANDED) {
        // Fresh ground boot: reset every backup variable, exactly like the primary
        // detector resets rtc_launch_pressure/rtc_max_altitude.
        backupBaselineSet    = false;
        backupBaseline        = 0.0f;
        backupMaxAltitude     = 0.0f;
        backupLaunchDetect    = 0;
        backupFired            = false;
        backupCalSampleCount  = 0;
        backupCalSum          = 0.0f;
        ejectionRelayFired    = false;
        ejectionFiredBy       = 0;
        Serial.println("Backup ejection detector: fresh boot, will recalibrate baseline.");
    } else {
        // Recovered mid-flight: keep whatever was retained in RTC RAM, same as the
        // primary path does with rtc_launch_pressure / rtc_max_altitude.
        Serial.println("Backup ejection detector: recovered mid-flight, keeping retained baseline.");
    }
}

// Call every loop() pass. Non-blocking: does a little work each call instead of the
// original's single big blocking loop, so it can share the loop with LoRa telemetry,
// flash logging and the watchdog reset in rocket_flight.ino. Fully independent of
// the primary detector's variables -- reads the shared sensor itself.
void checkBackupEjection() {
    if (backupFired) return;  // latched, nothing left to do

    float r = bmp.readAltitude(BACKUP_SEA_LEVEL_HPA);
    if (isnan(r)) return;     // identical NaN guard to ejection_code.ino

    // ---- Phase 1: baseline calibration (ported BASE_READS logic) ----
    if (!backupBaselineSet) {
        backupCalSum += r;
        backupCalSampleCount++;
        if (backupCalSampleCount >= BACKUP_BASE_READS) {
            backupBaseline = backupCalSum / backupCalSampleCount;
            backupBaselineSet = true;
            Serial.print("Backup ejection baseline calibrated: ");
            Serial.println(backupBaseline);
        }
        return;
    }

    // ---- Phase 2: averaged reading per evaluation (ported NUM_READS logic) ----
    backupCycleSum += r;
    backupCycleCount++;
    if (backupCycleCount < BACKUP_READS_PER_CYCLE) return;

    float average = backupCycleSum / backupCycleCount;
    backupCycleSum = 0.0f;
    backupCycleCount = 0;

    float rAlti = average - backupBaseline;  // identical to original's rAlti = average - baverage

    if (rAlti > backupMaxAltitude) {
        backupMaxAltitude = rAlti;
    }

    if (rAlti > BACKUP_LAUNCH_ALT_M) {
        backupLaunchDetect = 1;
    }

    if (backupLaunchDetect == 1) {
        if (rAlti + BACKUP_APOGEE_DROP_M < backupMaxAltitude) {
            // Independent blackbox record, same intent as the original's
            // EEPROM.write(loco, maxalti) -- kept separate from the LittleFS
            // FlashLogger so it survives even if flash logging is disabled.
            EEPROM.write(backupEepromSlot, (uint8_t)constrain(backupMaxAltitude, 0.0f, 255.0f));
            int newAddr = backupEepromSlot + 1;
            EEPROM.write(backupEepromAddr, newAddr);
            EEPROM.commit();

            fireEjectionRelay(EJECT_REASON_BACKUP);
        }
    }
}
