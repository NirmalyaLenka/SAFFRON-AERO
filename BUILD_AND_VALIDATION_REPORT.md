# Build and validation report

**Project:** SAFFRON-AERO 0.1.0  
**Validation date:** 9 November 2026  
**Status:** Source and host-side reference tests checked; firmware compilation, dashboard runtime and physical hardware validation remain blocked/unrun in the delivery environment.

## Deliverable contents

This archive contains the ESP32 Arduino/PlatformIO source, local Flask dashboard, ESP32-hosted fallback UI, portable recovery reference model, automated tests, wiring assumptions, electrical safety and commissioning guides, research bibliography, experiment protocol, sample CSV, and problem-solution report. The problem-solution report contains **15,186 whitespace-delimited words** in this build (`wc -w docs/problem_solution_report.md`). This is approximately the requested 15,000-word target; it is not claimed to be exactly 15,000 words.

## Checks actually run

| Check | Result | What the result means |
|---|---|---|
| `python -m unittest discover -s tests -v` | **14 passed, 5 skipped (19 discovered)** | The portable recovery/persistence model tests passed. Dashboard endpoint tests skipped because Flask is not installed in this environment. |
| `python -m compileall -q dashboard tools tests shared` | **Passed** | Python files compile to bytecode syntax-wise; imports/runtime behaviour of the dashboard is not established by this check. |
| `python tools/validate_project.py` | **Passed; no errors or warnings** | Required files exist, Python AST checks pass, CSV column widths are consistent, README markdown targets checked by the script resolve, the source-level boot-safety invariants pass, and the report count is checked. This is not a C++ compile. |
| `node --check dashboard/static/app.js` | **Passed** | Dashboard JavaScript syntax check only, not browser/device integration. |
| Fallback HTML inline JavaScript via `node --check` | **Passed** | Syntax only; no browser/device integration test. |
| Firmware source lexical delimiter check | **Passed (limited)** | Checks balanced delimiters and closed strings/comments only; this is **not** a C++ compilation. |
| `python tools/generate_sample_data.py` | **Passed** | Generated sample CSV contains synthetic data expressly marked `SIMULATED`; not chamber data. |
| ZIP integrity and manifest hash verification | **Passed after final packaging** | Archive was reopened, `ZipFile.testzip()` returned no corrupt member, and every manifest-listed file matched its SHA-256 and byte count. `MANIFEST.csv` describes all other project files; it does not hash itself. |

## Checks not run and why

- **PlatformIO firmware build:** not run. `pio`/`platformio` is not installed in the provided runtime. Attempting to install Python dependencies failed because the runtime could not resolve its package index (no network/DNS connectivity). Therefore the C++ firmware is **not compile-verified** here. Build it on a machine with PlatformIO and network access, review every compiler warning, and fix any compile issues before flashing.
- **Flask dashboard API tests:** five test-client tests skipped because Flask is missing (and the environment could not install packages). Install the pinned dependencies in `dashboard/requirements.txt`, then rerun the complete test command; the dashboard has not been runtime-verified in this environment.
- **Physical ESP32/NVS recovery:** not run; no board is connected. Host-side tests simulate a fresh controller and dual-slot persistence, not the silicon's NVS, brownout behaviour, watchdog, GPIO power-on state, or actual firmware timing.
- **Electrical, thermal and biological validation:** not run. No verified driver schematic, flow switch, reservoir-level input, hot-side probe installation, physical emergency-stop circuit, Peltier thermal test or saffron crop result was available to test.

## Software test coverage

The portable model tests cover Floral Induction and exact setpoint restoration; fresh sensor data; stale/invalid sensor lockout; missing/failed interlocks; manual mode and expired manual-override non-replay; persistent safety latch; corrupt latest-slot fallback; interrupted writes; unsupported schema fail-closed behaviour; profile range validation; and the key distinction between recovered configuration and permission to energise outputs. The model deliberately leaves output permission false even when a recovery result permits automatic control: the real runtime must still calculate guarded demand and validate load-specific conditions.

The dashboard test suite includes explicit simulation labelling, phase mutation, invalid phase rejection, manual output shutdown, and no silent simulation fallback when the device API is unavailable. These tests are present but were skipped because dependencies were unavailable.

## Firmware and hardware limitations

1. The source uses schema version 1. Unsupported or corrupt configuration fails closed into `CONFIG_RECOVERY_REQUIRED`; no migration from a previous unrelated sketch or prior SAFFRON-AERO schema is claimed. A future schema change must add a version-specific migration, validate the candidate, write/read back the inactive slot, and retain the previous valid slot if any step fails.
2. Cooling and misting are separate subsystems. Peltier operation remains locked unless the actual driver, pump, fan, flow proof and distinct hot-side temperature sensor are configured and verified. Misting remains separately locked until its driver, reservoir-level feedback and humidity-sensor conditions pass. If cooling hardware is simply uncommissioned, core automatic recovery can complete while the cooling output stays locked; if a configured flow/thermal safety check fails, the firmware latches a safety lockout.
3. All hardware commissioning flags are **false** in the delivered `config.h`. Peltier/mister command pins and optional pump, fan, flow switch, reservoir switch, hot-side temperature and light-sensor pins are set to `-1` by default. Proposed Peltier/mister GPIOs are recorded in `hardware/pin_map.csv` but are not configured as output GPIOs. Real load operation is not enabled by the stock configuration.
4. Two TEC1-12715 modules may exceed the real system's hot-side heat-rejection capacity. Confirm load current, power supply rails, driver type/polarity, wiring, protective devices, thermal interfaces, radiator, coolant flow and fan before changing any commissioning flag.
5. A GPIO `commanded` state is not measurement of electrical current, coolant flow, air movement, mist generation, root coverage, temperature stability or biological success. Independent feedback and physical tests are needed.
6. The current profile numbers are provisional engineering defaults. Published temperature sequences depend on the cultivation model; for example, Wang et al. studied a staged warm/intermediate/cool sequence in a Chinese two-segment system, not this exact aeroponic enclosure. See `docs/saffron_growth_environment.md` and `docs/research_and_references.md`.
7. A complete power cut disables the ESP32 and cooling system. The firmware can restore saved settings after power returns, but cannot maintain climate during the outage or reconstruct the unmeasured interruption interval.

## Exact local run instructions

### Dashboard simulation

```powershell
cd SAFFRON-AERO\dashboard
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python app.py --simulate
```

Open `http://127.0.0.1:5000`. On macOS/Linux, activate with `source .venv/bin/activate`. Simulation is clearly marked and does not control hardware.

### Dashboard connected to the ESP32

Change both `NetworkConfig::AP_PASSWORD` and `NetworkConfig::API_KEY` in `firmware/esp32_controller/include/config.h` before use; do not publish real credentials. On Windows PowerShell, from `dashboard/` after dependency installation:

```powershell
$env:SAFFRON_DEVICE_URL = "http://192.168.4.1"
$env:SAFFRON_API_KEY = "the-same-random-api-key-as-firmware"
python app.py
```

The URL may differ if using a station network. Device mode reports an unavailable device as an error rather than displaying simulated measurements.

### Firmware build and flashing

Install PlatformIO Core or the VS Code PlatformIO extension, open `firmware/esp32_controller/`, confirm the target is a compatible classic ESP32 development board, then run:

```bash
pio run
pio device list
pio run -t upload --upload-port COM14
pio device monitor --port COM14 --baud 115200
```

Replace `COM14` with the detected port. **Do not flash to control real loads until the build succeeds and the commissioning/safety checks have been completed.**

## Research metadata

The cited saffron articles are linked directly from `docs/research_and_references.md` and collected in BibTeX form in `docs/references.bib`. Metadata checked in the source record includes the Wang et al. temperature study, Jose-Santhi et al. floral-induction study, Askari-Khorasgani and Pessarakli controlled-environment review, Cardone et al. corm-origin study, Pirasteh-Anosheh et al. multi-field study, Qaryouti et al. greenhouse study and Acharya et al. review. The bibliography is a focused starter set, not a systematic review. The exact chamber's aeroponic profiles remain unvalidated.

## Gate before enabling real Peltier control

Do not change `PELTIER_DRIVER_VERIFIED`, `COOLANT_PUMP_VERIFIED`, `HOT_SIDE_FAN_VERIFIED`, `FLOW_SENSOR_VERIFIED` or `HOT_SIDE_SENSOR_VERIFIED` merely to bypass the lockout. Identify the physical hardware, establish rated drivers and fuses, fit and test pump/fan/flow/hot-side feedback, map each DS18B20 physical location/address, verify a physical emergency-stop/disconnect and external pull-down-safe driver state, measure current and thermal response with no corms, test flow/thermal faults, and retain dated evidence. The default hardware configuration intentionally keeps output permission blocked.
