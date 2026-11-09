# Local API protocol

Base address is the ESP32's local AP address or its station address. The Python dashboard uses `SAFFRON_DEVICE_URL`. Requests should use short timeouts and must not be called in a way that blocks the firmware control loop. Responses are JSON. Mutating endpoints require `X-API-Key` that exactly matches `NetworkConfig::API_KEY` in firmware configuration. The sample API key is intentionally a placeholder and is rejected until changed.

## Endpoints

| Method / path | Auth | Purpose |
|---|---|---|
| `GET /api/status` | Read-only in prototype | Current phase/mode, measurements and ages, config revision, recovery stage, lockout reason, actuator commands, interlocks and recent RAM events |
| `GET /api/config` | Read-only in prototype | Persisted configuration/profile snapshot and save status |
| `POST /api/commission` | API key | First-time configuration only. Body must include `{"confirm":true,"commissioningAcknowledged":true}`. Does not commission hardware or unlock output drivers. |
| `POST /api/phase` | API key | Body `{"phaseId":0}` through `{"phaseId":5}`; stores the phase and revision. |
| `POST /api/mode` | API key | Body `{"automaticMode":true}` or `false`; manual mode forces load commands off. |
| `POST /api/config` | API key | Updates a single profile. Body: `{"phaseId":1,"profile":{"airTargetC":22,"airMinC":18,"airMaxC":26,"rhTarget":60,"rhMin":35,"rhMax":80,"tempHyst":1,"rhHyst":5,"mistPulseMs":2000,"mistRestMs":60000}}`. All values are validated on firmware. The firmware marks changed profiles provisional. |
| `POST /api/cooling` | API key | Timed manual request in manual mode only: `{"enabled":true,"durationMs":30000}`. Request does not bypass interlocks. Use `{"enabled":false}` to cancel. |
| `POST /api/mist` | API key | Timed misting output request with the same body form and independent reservoir/RH interlocks. |
| `POST /api/estop` | API key | Forces configured GPIO commands off and latches emergency-stop state in saved config. Not a replacement for physical isolation. |
| `POST /api/clear-fault` | API key | Body `{"confirm":true}`; requires fresh sensors and `HARDWARE_ESTOP_CIRCUIT_VERIFIED` to be true in firmware after the hardware has actually been reviewed. |
| `POST /api/factory-reset` | API key | Destructive action requiring `{"confirm":true,"phrase":"RESET_SAFFRON_AERO"}`. Clears the `saffron` NVS namespace and requires explicit first-time commissioning again. Export/review configuration before use. |
| `GET /api/events` | Read-only in prototype | Recent events kept in RAM for this boot only. |
| `GET /api/events.csv` | Read-only in prototype | Current LittleFS event CSV, if filesystem is available. Rotated previous file remains internal in this version. |
| `GET /api/diagnostics` | Read-only in prototype | Firmware, storage, recovery, configured safety pins and network diagnostics. |

## Units and semantics

- Temperature: degrees Celsius (`C`).
- Relative humidity: percent RH.
- Time ages/durations: milliseconds (`ms`).
- `phaseId`: `0` preparation, `1` floral induction, `2` flowering, `3` vegetative growth, `4` daughter-corm development, `5` dormancy/cycle transition.
- `peltier1CommandedOn`, `peltier2CommandedOn`, `pumpCommandedOn`, `fanCommandedOn`, and `mistCommandedOn` describe firmware/GPIO commands. The current hardware build has no actual current/power feedback; those values do not prove energisation or operation.
- `sensorDataFresh` means the DHT22 and primary DS18B20 measurements pass validity and age checks. It does not verify sensor calibration, placement or agreement with the plant environment.
- `recoveryComplete` is only true when firmware declares the boot path complete. Missing configuration/interlocks result in explicit lockout.
- Sensor values may be JSON `null` when invalid or unavailable. Clients must not replace `null` with zero.
- `source` is added by the dashboard: `ESP32` or `SIMULATED`. Preserve it in exports.

## Error handling

Expected HTTP classes: `400` malformed/missing input; `401` missing/wrong API key; `404` unknown endpoint or unavailable log; `409` wrong operating state or commissioning conflict; `422` out-of-range/invalid profile; `423` safety interlock denies a command; `500` internal storage/save error. An error response contains `{"ok":false,"error":"human-readable reason"}`. A dashboard should show failures and must not claim a failed write was saved.

## Network and security notes

The firmware's minimal local API and shared key are designed for a trusted prototype LAN/AP. This is not a security-reviewed public service. Change default AP password and API key; keep device off public port forwarding; do not commit secrets. In a hardened design, add stronger authentication, request replay protection, rate limiting, TLS where viable, signed firmware, secure update and recovery procedures.
