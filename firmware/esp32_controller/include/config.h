#pragma once
#include <Arduino.h>

// Review every pin and driver assumption against the physical board before wiring.
// The supplied values for sensor pins come from the project brief; output pins are
// suggestions only. The firmware starts every load output LOW and leaves high-current
// outputs locked until the commissioning constants below are deliberately verified.
namespace Pins {
  constexpr int DS18B20_DATA = 4;
  constexpr int DHT22_DATA = 16;
  constexpr int PELTIER_1_ENABLE = -1; // proposed GPIO 25; leave disabled until polarity/topology is verified
  constexpr int PELTIER_2_ENABLE = -1; // proposed GPIO 26; shares one logical cooling demand
  constexpr int MIST_ENABLE = -1;      // proposed GPIO 27; leave disabled until driver polarity is verified
  constexpr int OLED_SDA = 21;
  constexpr int OLED_SCL = 22;

  // Optional inputs / outputs remain disabled until actual hardware is verified.
  constexpr int COOLANT_PUMP_ENABLE = -1;
  constexpr int HOT_SIDE_FAN_ENABLE = -1;
  constexpr int FLOW_SWITCH_INPUT = -1;       // configured active HIGH means flow proven
  constexpr int RESERVOIR_OK_INPUT = -1;      // configured active HIGH means level OK
  constexpr int HOT_SIDE_SENSOR_INDEX = -1;   // DallasTemperature device index; verify ROM/location
  constexpr int LIGHT_SENSOR_INPUT = -1;      // no light sensor installed by default
}

namespace Commissioning {
  // Keep false for first boot and simulation. Change only after written hardware checks.
  constexpr bool PELTIER_DRIVER_VERIFIED = false;
  constexpr bool MIST_DRIVER_VERIFIED = false;
  constexpr bool COOLANT_PUMP_VERIFIED = false;
  constexpr bool HOT_SIDE_FAN_VERIFIED = false;
  constexpr bool FLOW_SENSOR_VERIFIED = false;
  constexpr bool RESERVOIR_SENSOR_VERIFIED = false;
  constexpr bool HOT_SIDE_SENSOR_VERIFIED = false;
  constexpr bool HARDWARE_ESTOP_CIRCUIT_VERIFIED = false;
  constexpr float MAX_HOT_SIDE_C = 55.0f;  // provisional protection limit; confirm TEC/heatsink specs
  constexpr uint32_t PUMP_PRIME_MS = 5000;
  constexpr uint32_t PUMP_COOLDOWN_MS = 10000;
}

namespace NetworkConfig {
  // CHANGE BOTH STRINGS before connecting real loads. Use a unique local password/key.
  constexpr char AP_SSID[] = "SAFFRON-AERO";
  constexpr char AP_PASSWORD[] = "CHANGE_THIS_AP_PASSWORD"; // >= 8 chars; example only
  constexpr char API_KEY[] = "CHANGE_THIS_TO_A_RANDOM_API_KEY";
  constexpr char STA_SSID[] = "";      // optional; leave empty for standalone AP operation
  constexpr char STA_PASSWORD[] = "";  // optional; do not commit real credentials
}

constexpr uint32_t SENSOR_STALE_MS = 10000;
constexpr uint32_t SENSOR_SAMPLE_MS = 2500;
constexpr uint32_t MANUAL_OVERRIDE_MAX_MS = 300000;
constexpr uint32_t MIST_MAX_CONTINUOUS_MS = 8000;
constexpr uint32_t MIST_REST_MS = 60000;
constexpr uint32_t COOLING_MIN_OFF_MS = 180000;
constexpr uint32_t COOLING_MIN_ON_MS = 60000;
constexpr uint32_t OLED_UPDATE_MS = 1000;
constexpr uint16_t CONFIG_SCHEMA_VERSION = 1;
