# System architecture

```mermaid
flowchart LR
  subgraph ESP32[ESP32 autonomous controller]
    S[Sensor acquisition + freshness checks] --> C[Phase profile / hysteresis control]
    C --> I[Output-specific interlocks]
    I --> O[Driver command signals]
    P[Dual-slot NVS configuration] <--> R[Boot recovery state machine]
    R --> C
    E[Event logger] --> F[LittleFS rotating CSV]
    A[Local API + fallback page] <--> R
    A <--> P
    S --> E
    I --> E
    D[OLED status] <-- R
    D <-- S
  end
  S --- H[ DHT22 / DS18B20 ]
  O --> HW[Verified external pump/fan/TEC/misting drivers]
  A <--> AP[ESP32 AP or local Wi-Fi]
  AP <--> DASH[Optional Flask dashboard]
  DASH --> CSV[Host-side telemetry/experiment CSV]
```

## Authority and boundaries

- **ESP32 configuration authority:** phase and profile state are read from NVS. The dashboard cannot write defaults merely by opening a page.
- **ESP32 control authority:** the firmware runs regardless of dashboard/browser/laptop presence. HTTP handlers validate a command and return; they do not drive outputs continuously.
- **Dashboard:** requests state and sends explicit commands. In device mode it must not generate a replacement reading if the API is unavailable. Host CSV logging is separate from NVS.
- **Simulation:** the dashboard's `--simulate` mode produces a labelled synthetic signal for UI tests; no claim is made that it is a plant/thermal model.
- **Safety:** hardware protection, fusing, cooling capacity, a physical disconnect and enclosure design are separate from firmware. A GPIO state cannot prove current, flow, thermal contact or fan speed.

## Data flow

Sensor values enter RAM with timestamps and validity flags. A reading is usable only when plausible and fresh. The controller evaluates the active saved profile with hysteresis, then gates each output independently. Configuration changes are range-checked and stored in an alternate NVS slot; the payload must pass schema, CRC, revision and semantic validation before it is considered current. Event messages are appended to LittleFS when available, and status is exposed through `/api/status`.

## Recovery states

`HARDWARE_INIT → LOAD_PERSISTED_STATE → VALIDATE_CONFIGURATION → SENSOR_RECOVERY` is the common recovery path. If a cooling subsystem is configured, `COOLING_LOOP_CHECK` primes the loop and validates flow/hot-side temperature before permitting Peltier commands. Missing/corrupt settings lead to `CONFIG_RECOVERY_REQUIRED`; invalid or stale core measurements lead to `SENSOR_RECOVERY`; a saved manual mode requires a new timed operator request. A physically uncommissioned cooling subsystem keeps its own output locked without automatically preventing core control recovery or an independently verified misting subsystem from operating. A failed live flow/thermal safety check latches `SAFETY_LOCKOUT`. Faults are not cleared merely because power returned.
