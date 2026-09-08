#pragma once

#include <cstdint>
#include <string>

class WeatherPayload {
public:
    uint32_t sequence = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    float pressure = 0.0f;
    float windSpeed = 0.0f;
    float windGust = 0.0f;
    int windDirection = 0;
    float rain = 0.0f;
    float lux = 0.0f;
    float battery = 0.0f;
    uint64_t timestamp = 0;

    static bool parse(const std::string& payload, WeatherPayload& weather);

    std::string toJson() const;
};