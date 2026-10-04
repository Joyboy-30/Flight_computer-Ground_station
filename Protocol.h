#pragma once
#include <stdint.h>

// Packet Types
#define PKT_TYPE_NORMAL_ALTITUDE 0x01
#define PKT_TYPE_APOGEE          0x02
#define PKT_TYPE_EJECTION_EVENT  0x03 // sent once, by whichever detector (primary or backup) fires the relay

// Exact 6-byte payload specified in the reference document.
// Using compiler attributes to prevent padding bytes.
#pragma pack(push, 1)
struct TelemetryPacket {
    uint8_t  rocket_id;
    uint8_t  type;
    uint16_t sequence;   // Stored in Big-Endian format per spec!
    uint16_t altitude;   // Stored in Big-Endian format, 0.1m resolution
};
#pragma pack(pop)

// Flight States
enum FlightState {
    STATE_BOOT = 0,
    STATE_READY,
    STATE_ASCENT,
    STATE_NEAR_APOGEE,
    STATE_APOGEE_LOCKED,
    STATE_DESCENT,
    STATE_LANDED
};
