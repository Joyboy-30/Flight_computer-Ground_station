#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "Protocol.h"
#include "DashboardHTML.h"

// --- WiFi AP Credentials ---
#define AP_SSID_PREFIX "RocketGND"
#define AP_PASSWORD    "rocket2024"
#define AP_CHANNEL     6
#define AP_MAX_CLIENTS 4

// --- Packet Ring Buffer ---
#define RING_BUFFER_SIZE 50

struct ReceivedPacket {
    uint8_t  rocket_id;
    uint8_t  type;
    uint16_t seq;
    float    alt;
    int      rssi;
    float    snr;
    char     ts[12]; // HH:MM:SS
};

ReceivedPacket ringBuffer[RING_BUFFER_SIZE];
uint16_t ringHead = 0;
uint16_t ringCount = 0;
portMUX_TYPE ringMux = portMUX_INITIALIZER_UNLOCKED;

// --- Web Server ---
WebServer server(80);

// Endian swap
uint16_t swapEndian(uint16_t val) {
    return (val << 8) | (val >> 8);
}

// Get current time as HH:MM:SS string (seconds since boot, formatted)
void getTimeStr(char* buf) {
    uint32_t s = millis() / 1000;
    uint32_t h = s / 3600;
    uint32_t m = (s % 3600) / 60;
    uint32_t sec = s % 60;
    snprintf(buf, 12, "%02lu:%02lu:%02lu", h, m, sec);
}

// ---- Web Server Handlers ----
void handleRoot() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handlePackets() {
    // Build JSON array from ring buffer — ArduinoJson v7 API
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    portENTER_CRITICAL(&ringMux);
    uint16_t count = ringCount;
    uint16_t head = ringHead;
    portEXIT_CRITICAL(&ringMux);

    // Iterate in chronological order
    uint16_t start = (count < RING_BUFFER_SIZE) ? 0 : head;
    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = (start + i) % RING_BUFFER_SIZE;
        JsonObject obj = arr.add<JsonObject>();
        obj["rocket_id"] = ringBuffer[idx].rocket_id;
        obj["type"]      = ringBuffer[idx].type;
        obj["seq"]       = ringBuffer[idx].seq;
        obj["alt"]       = ringBuffer[idx].alt;
        obj["rssi"]      = ringBuffer[idx].rssi;
        obj["snr"]       = ringBuffer[idx].snr;
        obj["ts"]        = ringBuffer[idx].ts;
    }

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

// ---- LoRa Receive Task (runs on Core 0) ----
void loraTask(void* pvParams) {
    while (true) {
        int packetSize = LoRa.parsePacket();
        if (packetSize == sizeof(TelemetryPacket)) {
            TelemetryPacket pkt;
            LoRa.readBytes((uint8_t*)&pkt, sizeof(TelemetryPacket));

            uint16_t seq = swapEndian(pkt.sequence);
            uint16_t alt_encoded = swapEndian(pkt.altitude);
            float    decoded_alt = (float)alt_encoded / 10.0f;
            int      rssi = LoRa.packetRssi();
            float    snr  = LoRa.packetSnr();

            // Store in ring buffer (thread safe)
            portENTER_CRITICAL(&ringMux);
            ReceivedPacket& slot = ringBuffer[ringHead];
            slot.rocket_id = pkt.rocket_id;
            slot.type      = pkt.type;
            slot.seq       = seq;
            slot.alt       = decoded_alt;
            slot.rssi      = rssi;
            slot.snr       = snr;
            getTimeStr(slot.ts);
            ringHead = (ringHead + 1) % RING_BUFFER_SIZE;
            if (ringCount < RING_BUFFER_SIZE) ringCount++;
            portEXIT_CRITICAL(&ringMux);

            // Also output JSON to USB Serial for ground_logger.py
            Serial.printf("{\"rocket_id\":%d,\"type\":%d,\"seq\":%d,\"alt\":%.1f,\"rssi\":%d,\"snr\":%.2f}\n",
                pkt.rocket_id, pkt.type, seq, decoded_alt, rssi, snr);

            // Flash RX LED and Beep Buzzer
            digitalWrite(PIN_LED_TX, HIGH);
            digitalWrite(PIN_BUZZER, HIGH);
            delay(30);
            digitalWrite(PIN_LED_TX, LOW);
            digitalWrite(PIN_BUZZER, LOW);

        } else if (packetSize > 0) {
            Serial.printf("{\"warning\":\"Unknown packet size: %d bytes\"}\n", packetSize);
        }

        vTaskDelay(1); // Yield to RTOS
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_TX, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW); // Ensure it starts quiet

    // ---- Start WiFi AP ----
    // Append last 2 bytes of MAC to SSID for uniqueness
    uint8_t mac[6];

    // Explicit AP mode + static IP config.
    // Clean up any old corrupted state from NVS first:
    WiFi.disconnect(true, true);
    WiFi.softAPdisconnect(true);
    delay(100);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPmacAddress(mac); // Use AP MAC specifically

    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);

    char ssid[24];
    snprintf(ssid, sizeof(ssid), "%s-%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);

    // ssid, password, channel, hidden, max_connections
    WiFi.softAP(ssid, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS);

    // Must wait for AP to settle before reading IP or starting server —
    // skipping this is the #1 cause of "access denied" on Android/Windows.
    delay(500);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("{\"log\":\"AP Started. SSID: %s | Password: %s | Dashboard: http://%s\"}\n",
                  ssid, AP_PASSWORD, ip.toString().c_str());

    // ---- Start Web Server ----
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/packets", HTTP_GET, handlePackets);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.printf("{\"log\":\"Web Dashboard: http://%s\"}\n", ip.toString().c_str());

    // ---- Start LoRa ----
    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
    LoRa.setPins(PIN_LORA_NSS, PIN_LORA_RST, PIN_LORA_DIO0);

    bool loraOk = LoRa.begin(LORA_FREQ);
    if (!loraOk) {
        Serial.println("{\"error\":\"CRITICAL: LoRa init failed. Check wiring!\"}");
        // Do not use while(1) here! We want the Web Server to keep running
        // so the user can still connect and we don't get 'Site can't be reached'.
    } else {
        LoRa.setSignalBandwidth(LORA_BANDWIDTH);
        LoRa.setSpreadingFactor(LORA_SPREADFACTOR);
        LoRa.setCodingRate4(LORA_CODINGRATE);
        LoRa.setPreambleLength(LORA_PREAMBLE);
        LoRa.setSyncWord(LORA_SYNC_WORD);
        LoRa.enableCrc();
        Serial.printf("{\"log\":\"LoRa Ready. Listening on 434.500 MHz\"}\n");

        // ---- Pin LoRa receive task to Core 0 ----
        xTaskCreatePinnedToCore(loraTask, "LoRaRX", 4096, NULL, 2, NULL, 0);
    }

    digitalWrite(PIN_LED_STATUS, HIGH);
}

void loop() {
    // Core 1 handles HTTP clients
    server.handleClient();
    delay(1);
}
