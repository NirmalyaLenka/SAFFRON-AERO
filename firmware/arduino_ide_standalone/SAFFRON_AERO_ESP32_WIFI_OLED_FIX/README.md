# SAFFRON-AERO — Standalone Arduino IDE Sketch

This folder preserves the standalone ESP32 sketch supplied for the Wi-Fi/OLED build:

- `SAFFRON_AERO_ESP32_WIFI_OLED_FIX.ino`

It is separate from the main safety-locked PlatformIO implementation in `firmware/esp32_controller/`. The two versions have different configuration formats, control behavior, pin assignments, and safety provisions. Do not merge their pin maps or assume the PlatformIO output lockout is active in this sketch.

## Hardware map used by this sketch

| Function | Hardware / pin | Meaning |
|---|---|---|
| Chamber air temperature + RH | DHT22, GPIO 16 | Primary air-temperature feedback for automatic control; humidity measurement |
| Peltier cold-plate temperature | DS18B20, GPIO 4 | Separate cold-plate reading and cutoff input; not chamber air temperature |
| OLED | SSD1306 I²C, SDA GPIO 21 / SCL GPIO 22, address 0x3C | Local status display |
| Peltier controller input | PWM GPIO 25 | Logic signal to a suitably rated external driver; never connect this pin directly to a Peltier power lead |
| Humidifier relay input | GPIO 26, active HIGH by default | Relay/driver control signal, not the humidifier's load supply |
| Optional RTC | DS3231 on SDA 21 / SCL 22 | Offline wall-clock scheduling when detected |

See [the standalone pin map](../../../hardware/arduino_ide_standalone_pin_map.csv) and the project-level [hardware and wiring notes](../../../docs/hardware_and_wiring.md). Confirm the actual board, sensor locations, relay polarity, driver topology, and supply wiring before using hardware outputs.

## Required Arduino libraries

Install these libraries through Arduino IDE's Library Manager:

- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- OneWire
- DallasTemperature
- Adafruit GFX Library
- Adafruit SSD1306
- RTClib by Adafruit

Select the correct ESP32 board and ESP32 Arduino core. The sketch includes compatibility code for Arduino-ESP32 core 2.x and 3.x, but that is not a substitute for compiling it with the exact version installed on your machine.

## Upload and connect

1. Open `SAFFRON_AERO_ESP32_WIFI_OLED_FIX.ino` from this directory in Arduino IDE.
2. Verify the pin map and change the Wi-Fi constants near the top of the sketch if required. The configured router network is currently the open SSID `Dihhh`; the code therefore connects without a password.
3. Review the fallback AP credentials in the sketch before deployment. The current example AP SSID is `SAFFRON-AERO`; change the example password to a unique value before relying on it.
4. Upload the sketch and open Serial Monitor at **115200 baud**.
5. Read the IP address printed by the ESP32. Open `http://<ESP32-IP>/` in a phone or computer browser on the same network. The sketch also starts its fallback access point while trying the router; use the fallback IP printed to Serial if the router connection fails (commonly `http://192.168.4.1/`).
6. Keep the ESP32 powered and within Wi-Fi range. The climate-control loop runs on the ESP32; a browser is not required for the control loop.

If the open router network is visible but the connection fails, check that it really is an open 2.4 GHz network and does not require a captive portal. The fallback access point is intended to preserve local access when the router is unavailable.

## Current dashboard and control behavior

- Edit temperature and relative-humidity targets for each of the six configured phases.
- Switch between AUTO and MANUAL operation.
- Set the cooling percentage limit / manual PWM request.
- Configure the scheduled humidifier pulse count and pulse duration.
- Monitor chamber-air temperature, humidity, cold-plate temperature, cooling command, humidifier state, network status, and safety message.
- Settings and phase targets are stored in ESP32 Preferences/NVS.

The OLED refresh is rate-limited to reduce unnecessary screen redraws. The cold-plate probe is required by the sketch's sensor interlock: missing or invalid chamber or DS18B20 readings force outputs off.

## Temperature terminology — important

- **Chamber temperature:** DHT22 air-temperature measurement on GPIO 16. This is the primary automatic-control feedback reading.
- **Cold-plate temperature:** DS18B20 measurement on GPIO 4. Place this probe on the Peltier cold plate only if the cold-plate cutoff is to represent that physical temperature.
- **Target temperature:** the selected phase's requested chamber-air setpoint.
- **Achievable chamber temperature:** **not implemented in this sketch**. The dashboard does not currently estimate or show a minimum achievable chamber temperature, and no separate ambient-temperature sensor or calibrated thermal-performance model is used. A target setpoint must not be presented as proof that the chamber can reach it.

Adding a defensible achievable-temperature display needs a separate design change: appropriate ambient measurement and calibration data from the actual chamber/thermal assembly, or an explicitly user-entered estimate clearly labelled as such. Do not invent a live estimate from Peltier duty percentage alone.

## Safety and known limitations

- The ESP32 pins are control signals only. Use properly rated Peltier and humidifier drivers, power supplies, fuses, connectors and wiring.
- Provide a suitable heatsink and hot-side fan for Peltier modules. This sketch reads the cold plate; it does not provide a verified hot-side temperature interlock or prove that hot-side cooling/flow is operating.
- The code's cold-plate cutoff is derived from the target chamber temperature minus a fixed offset. Confirm this scheme is appropriate before using real loads.
- With no detected DS3231, the humidifier scheduler uses elapsed time since boot. It cannot maintain a true clock-based daily schedule through power loss.
- A saved AUTO preference does not establish that the chamber has reached its target. Watch actual chamber measurements during supervised tests.
- Phase setpoints are configurable starting values, not validated recommendations for every cultivar, chamber, or cultivation method.

The project maintainer reports this sketch is working on their setup. The current repository update preserves the supplied source; it is not a new compile report or a substitute for supervised hardware testing.
