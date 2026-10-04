#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include "Protocol.h"

#define LOG_FILENAME "/flight_data.bin"
#define LOG_BUFFER_SIZE 10 // Batch write 10 samples at a time to reduce Flash wear and delay

// Binary struct for the blackbox log
#pragma pack(push, 1)
struct LogEntry {
    uint32_t timestamp;
    uint8_t state;
    float current_altitude;
    float max_altitude;
};
#pragma pack(pop)

class FlashLogger {
private:
    LogEntry buffer[LOG_BUFFER_SIZE];
    uint8_t bufferIndex;
    bool initialized;
    File logFile;

    void flushBuffer() {
        if (bufferIndex == 0 || !initialized || !logFile) return;

        // Safety limit: Stop logging if file exceeds 1.3MB to prevent lfs_alloc crash
        if (logFile.size() > 1300000) { 
            Serial.println("WARNING: Flash memory limit reached. Logger disabled to protect radio.");
            initialized = false;
            bufferIndex = 0;
            logFile.close();
            return;
        }

        logFile.write((uint8_t*)buffer, sizeof(LogEntry) * bufferIndex);
        logFile.flush(); // Force sync to physical flash without closing!
        bufferIndex = 0;
    }

public:
    FlashLogger() : bufferIndex(0), initialized(false) {}

    bool begin() {
        // Format on fail = true. First time it runs it will format the partition.
        if (!LittleFS.begin(true)) {
            Serial.println("CRITICAL: LittleFS Mount Failed");
            return false;
        }
        
        // Open file ONCE and keep it open. Opening in FILE_APPEND on a huge file 
        // takes a long time because LittleFS has to walk the block chain!
        logFile = LittleFS.open(LOG_FILENAME, FILE_APPEND);
        if (!logFile) {
            Serial.println("CRITICAL: Failed to open log file.");
            return false;
        }

        initialized = true;
        Serial.println("Flash Logger Initialized (File kept open for speed).");
        return true;
    }

    void log(uint32_t ts, uint8_t state, float alt, float max_alt) {
        if (!initialized) return;

        buffer[bufferIndex].timestamp = ts;
        buffer[bufferIndex].state = state;
        buffer[bufferIndex].current_altitude = alt;
        buffer[bufferIndex].max_altitude = max_alt;
        bufferIndex++;

        // When buffer is full, flush to physical flash memory
        if (bufferIndex >= LOG_BUFFER_SIZE) {
            flushBuffer();
        }
    }

    // Force flush for emergency or landing
    void forceFlush() {
        flushBuffer();
    }

    // Dumps the entire binary log over Serial in CSV format
    void dumpLogToSerial() {
        if (!initialized) return;
        
        logFile.close(); // Close append handle
        File readHandle = LittleFS.open(LOG_FILENAME, FILE_READ);
        if (!readHandle) {
            Serial.println("No log file found.");
            return;
        }

        Serial.println("\n--- BEGIN FLIGHT LOG DUMP ---");
        Serial.println("Timestamp_ms,State,CurrentAlt_m,MaxAlt_m");

        LogEntry entry;
        while (readHandle.available() >= sizeof(LogEntry)) {
            readHandle.read((uint8_t*)&entry, sizeof(LogEntry));
            Serial.printf("%lu,%d,%.2f,%.2f\n", 
                entry.timestamp, entry.state, entry.current_altitude, entry.max_altitude);
        }
        
        readHandle.close();
        Serial.println("--- END FLIGHT LOG DUMP ---\n");
        
        // Reopen for appending
        logFile = LittleFS.open(LOG_FILENAME, FILE_APPEND);
    }
    
    void eraseLog() {
        if (logFile) logFile.close();
        if (LittleFS.exists(LOG_FILENAME)) {
            LittleFS.remove(LOG_FILENAME);
            Serial.println("Log file erased.");
        }
        // Reopen a fresh one
        logFile = LittleFS.open(LOG_FILENAME, FILE_APPEND);
        bufferIndex = 0;
    }
};
