# Firmware explanation

## Main loop

The Arduino firmware is kept in `firmware/esp32_controller/src/main.cpp` and builds through PlatformIO. Sensor acquisition, HTTP request handling, control evaluation and OLED updates are non-blocking in the main loop. The DHT22 is sampled on a roughly 2.5-second cadence. The DS18B20 conversion runs asynchronously. Each reading has a validity flag and last-sample timestamp; the control logic rejects values older than the configured freshness window.

## Configuration storage

The ESP32 `Preferences` API is used for compact non-volatile state. A JSON serialisation includes the schema version, revision, active phase, committed automatic/manual mode, persistent safety latch, calibration offsets and six phase profiles. Each save writes to the alternate slot (`a` or `b`), checks serialised content and CRC-32, then verifies that the new slot can be read back and passes semantic validation. Startup chooses the highest-revision valid slot. CRC-32 detects accidental corruption; it is not a cryptographic signature and does not provide adversarial tamper protection.

A fresh device does not silently save compiled defaults as though they were previously committed. It presents `CONFIG_RECOVERY_REQUIRED`; an authenticated, intentional commissioning action creates the initial profile set. Factory reset is a separate action requiring a phrase. A valid previous slot remains available if the newest slot is incomplete or has a bad checksum, provided that previous slot has not been overwritten.

## Recovery

All configured load GPIOs are driven LOW at boot. The firmware reads persisted configuration, validates it, starts sensors and networking, then waits for fresh readings. In automatic mode, the code requires the cooling driver, pump/fan, flow feedback and hot-side probe to be explicitly configured and verified before any Peltier command is permitted. When that cooling hardware is configured, it primes the loop, verifies the flow switch and checks the hot-side sensor before permitting TEC commands. When the cooling pathway is simply uncommissioned, the saved phase and automatic controller mode can recover while the cooling outputs remain independently locked; a separately verified misting pathway may still operate. Misting has its own driver, reservoir-level and humidity-sensor checks. A failed live flow/thermal test triggers a persistent safety lockout rather than bypassing the failed interlock. The supplied flags/pins are not commissioned, so the stock firmware does not energise Peltiers or the mist maker.

If the previous mode was manual, the controller returns with manual output demands off. A fresh timed operator request is required. If a persistent E-stop/flow/thermal fault is latched, a reboot does not clear it. Fault clearance requires valid sensor data and an intentional API action after the configured hardware stop-circuit record has been verified.

## Control policy

Temperature and relative humidity use per-phase hysteresis. Cooling demand and misting demand are logical requests; they are not the same as energised outputs. The output gate evaluates independent subsystem interlocks. Both Peltier enable signals are commanded together, because no independently qualified driver topology is identified. The code uses on/off driver signalling and minimum timing rather than pretending a 0–100% command is linear cooling capacity. Mist pulses are time-limited and depend on the RH validity, reservoir switch and verified driver.

The default profiles are demonstration/commissioning placeholders. They are not automatically promoted to research-validated settings. The code does not implement a proven saffron crop model, nutrient dosing, pH/EC control, root coverage sensor, light/PAR measurement, actual current sensing, verified flow metering or a redundant thermal cutoff.

## API and network

The ESP32 starts a local access point for direct access. Station mode is optional if credentials are supplied. The local API uses a shared `X-API-Key` for writes. The fallback UI prompts for that key instead of embedding it in the page. The simple shared-key model suits a small trusted local prototype; it is not a hardened internet-facing service. Keep the AP password and API key secret, do not port-forward the device, and change sample credentials before use.

## Event logging

NVS is reserved for configuration, not every live sensor sample. Events are written to an in-RAM recent buffer and appended to LittleFS CSV when available; the active event file rotates to a previous file after its configured size. The CSV uses boot-relative milliseconds. Without an RTC with backup power or a time source, the ESP32 may not know correct wall-clock time after a long power loss, and no data is available while the whole system is unpowered.

## Persistent schema policy

The initial build uses configuration schema version `1`. No migration from an earlier SAFFRON-AERO NVS schema is claimed because no prior released schema was supplied. If a payload has a missing, corrupt, or unsupported schema, it fails validation and the controller enters `CONFIG_RECOVERY_REQUIRED`; it must not silently replace the selected phase or reinterpret old fields. When the schema changes, a future release must add a version-specific migration function, validate the migrated candidate, write it to the inactive slot, read it back and verify checksum/revision before declaring migration successful. If any step fails, preserve the last valid older slot and require intentional operator recovery. This fail-closed strategy is safer than guessing at fields or silently migrating an unrelated earlier sketch's compile-time constant.
