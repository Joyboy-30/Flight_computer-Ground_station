#pragma once
#include <Arduino.h>

class SensorFilter {
private:
    float medianBuffer[5];
    uint8_t bufferIndex;
    bool bufferFilled;
    float emaAlpha;
    float currentFiltered;

    // Helper to sort and find median
    float calculateMedian() {
        float sorted[5];
        memcpy(sorted, medianBuffer, sizeof(medianBuffer));
        // Simple insertion sort for 5 elements
        for (int i = 1; i < 5; i++) {
            float key = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j] > key) {
                sorted[j + 1] = sorted[j];
                j = j - 1;
            }
            sorted[j + 1] = key;
        }
        return sorted[2]; // Middle element
    }

public:
    SensorFilter(float alpha) : bufferIndex(0), bufferFilled(false), emaAlpha(alpha), currentFiltered(0.0f) {
        for(int i=0; i<5; i++) medianBuffer[i] = 0;
    }

    float update(float newRawValue) {
        medianBuffer[bufferIndex] = newRawValue;
        bufferIndex++;
        if (bufferIndex >= 5) {
            bufferIndex = 0;
            bufferFilled = true;
        }

        if (!bufferFilled) {
            currentFiltered = newRawValue; // Not enough data yet
            return currentFiltered;
        }

        // 1. Median Filter to eliminate sudden 1-sample spikes (venturi noise, sensor glitches)
        float medianVal = calculateMedian();

        // 2. EMA Filter to smooth the remaining curve
        currentFiltered = (emaAlpha * medianVal) + ((1.0f - emaAlpha) * currentFiltered);
        return currentFiltered;
    }

    float get() { return currentFiltered; }
};
