# Testing and validation plan

## Software tests

The portable Python reference model in `shared/recovery_model.py` models two-slot persistence with JSON payloads, CRC-32 and revisions. Unit tests exercise interrupted saves, corrupt configuration, phase/profile recovery, automatic-mode recovery, manual command non-replay, sensor freshness and interlock failures. The Flask application has test-client checks for explicit simulation state, action processing, profile bounds and experiment logging. `tools/validate_project.py` verifies required files and the report's actual word count.

These tests validate Python reference behaviour and dashboard request handling. They do not compile C++ firmware, execute on an ESP32, validate NVS atomicity on every silicon/SDK combination, test real sensor characteristics, or validate the power electronics.

## Firmware bench tests required

1. Build with the pinned PlatformIO dependencies and an actual ESP32 board definition.
2. Flash with all loads disconnected. Verify safe GPIO state at reset, boot, brownout, watchdog restart and configuration-recovery state.
3. Save phase/profile, cold power-cycle, and verify readback plus revision/CRC. Interrupt an update if practical and verify recovery uses the previous valid slot.
4. Disconnect each sensor and test invalid/stale lockout; present plausible but incorrect wiring/placement and test physical calibration separately.
5. Verify AP startup and local API while the internet is unavailable; disconnect the dashboard and prove the control loop continues.
6. Verify manual mode restart with a previously active time-limited command: no command is replayed.
7. Test flow-switch startup, flow loss, hot-side overtemperature, stale hot-side readings, reservoir-low and physical E-stop using protected, supervised instrumentation.
8. Verify persistent event log rotation, CSV escaping and data-gap semantics.

## Biological trial design

First gather passive environmental data, then run an empty-chamber cooling test, then a supervised test with no corms and finally a small corm batch. Record a control group or baseline where feasible. Predefine primary outcomes: sensor uptime, time inside a justified experimental band, temperature/RH gradients, energy use, condensation events, flower count, stigma dry mass, corm survival and daughter-corm mass/count. Report the number of corms and batches and show variation, not just the best result. A single flowering event is not evidence of commercial scalability.

## Acceptance criteria (initial suggestions)

These are engineering test gates, not proof of agricultural effectiveness:

- Saved phase/settings return exactly after restart when stored config is valid.
- Corrupt config is detected and never silently replaced with defaults.
- Sensor readings are age-checked and output requests remain off if readings are missing/stale.
- Core automatic mode is restored only after valid core sensors and valid saved configuration; each actuator remains off until its own hardware and live interlocks pass. If the cooling loop is configured but flow/thermal checks fail, a latched safety lockout blocks recovery.
- No stale manual forced-output state is replayed after reboot.
- Flow/thermal fault causes TEC outputs off and a latched fault event.
- Dashboard cannot overwrite device settings merely by opening or refreshing a page.
- Simulation is visibly labelled and device mode does not fall back to simulation.
- Event CSV and exported CSV distinguish actual readings from simulated readings and preserve missing-data gaps.

## Validation status

The build report is authoritative for what was actually executed in the delivery environment. Do not change a status from “not run” to “passed” without evidence. A firmware syntax check or Python test is not a hardware test, and no code-only validation can prove that the chamber maintains a target, produces saffron, or is electrically safe.
