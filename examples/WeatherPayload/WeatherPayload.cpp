#include "WeatherPayload.hpp"

#include <cstdlib>
#include <sstream>
#include <iomanip>

namespace {

bool parseFloat(const std::string& value, float& result) {
    char* end = nullptr;
    result = std::strtof(value.c_str(), &end);

    return end != value.c_str();
}

bool parseInt(const std::string& value, int& result) {
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);

    if (end == value.c_str()) {
        return false;
    }

    result = static_cast<int>(parsed);

    return true;
}

bool parseUInt32(const std::string& value, uint32_t& result) {
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);

    if (end == value.c_str()) {
        return false;
    }

    result = static_cast<uint32_t>(parsed);

    return true;
}

bool parseUInt64(const std::string& value, uint64_t& result) {
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);

    if (end == value.c_str()) {
        return false;
    }

    result = static_cast<uint64_t>(parsed);

    return true;
}

}

bool WeatherPayload::parse(const std::string& payload, WeatherPayload& weather) {
    std::istringstream stream(payload);
    std::string token;

    bool haveSequence = false;
    bool haveTemperature = false;
    bool haveHumidity = false;
    bool havePressure = false;
    bool haveWindSpeed = false;
    bool haveWindGust = false;
    bool haveWindDirection = false;
    bool haveRain = false;
    bool haveLux = false;
    bool haveBattery = false;
    bool haveTimestamp = false;

    while (stream >> token) {
        const auto separator = token.find('=');

        if (separator == std::string::npos) {
            continue;
        }

        const std::string name = token.substr(0, separator);
        const std::string value = token.substr(separator + 1);

        if (name == "seq") {
            haveSequence = parseUInt32(value, weather.sequence);
        } else if (name == "temp") {
            haveTemperature = parseFloat(value, weather.temperature);
        } else if (name == "humidity") {
            haveHumidity = parseFloat(value, weather.humidity);
        } else if (name == "pressure") {
            havePressure = parseFloat(value, weather.pressure);
        } else if (name == "wind") {
            haveWindSpeed = parseFloat(value, weather.windSpeed);
        } else if (name == "gust") {
            haveWindGust = parseFloat(value, weather.windGust);
        } else if (name == "direction") {
            haveWindDirection = parseInt(value, weather.windDirection);
        } else if (name == "rain") {
            haveRain = parseFloat(value, weather.rain);
        } else if (name == "lux") {
            haveLux = parseFloat(value, weather.lux);
        } else if (name == "battery") {
            haveBattery = parseFloat(value, weather.battery);
        } else if (name == "timestamp") {
            haveTimestamp = parseUInt64(value, weather.timestamp);
        }
    }

    return haveSequence &&
           haveTemperature &&
           haveHumidity &&
           havePressure &&
           haveWindSpeed &&
           haveWindGust &&
           haveWindDirection &&
           haveRain &&
           haveLux &&
           haveBattery &&
           haveTimestamp;
}

std::string WeatherPayload::toJson() const {
    std::ostringstream json;

    json << "{"
         << "\"sequence\":" << sequence << ","
         << "\"temperature\":" << std::fixed << std::setprecision(1) << temperature << ","
         << "\"humidity\":" << std::fixed << std::setprecision(1) << humidity << ","
         << "\"pressure\":" << std::fixed << std::setprecision(1) << pressure << ","
         << "\"windSpeed\":" << std::fixed << std::setprecision(1) << windSpeed << ","
         << "\"windGust\":" << std::fixed << std::setprecision(1) << windGust << ","
         << "\"windDirection\":" << windDirection << ","
         << "\"rain\":" << std::fixed << std::setprecision(1) << rain << ","
         << "\"lux\":" << std::fixed << std::setprecision(1) << lux << ","
         << "\"battery\":" << std::fixed << std::setprecision(2) << battery << ","
         << "\"timestamp\":" << timestamp
         << "}";

    return json.str();
}
