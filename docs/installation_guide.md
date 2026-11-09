# Installation guide

## A. Dashboard simulation first

Use Python 3.11+ and run from `dashboard/`:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python app.py --simulate
```

Open `http://127.0.0.1:5000`. On macOS/Linux, activate with `source .venv/bin/activate`. Simulation is explicit and labelled. It is useful for checking the dashboard, exports and form validation, not for estimating chamber performance.

## B. Prepare firmware

1. Install PlatformIO Core or the VS Code PlatformIO extension.
2. Open `firmware/esp32_controller/` as the project.
3. Read `hardware_and_wiring.md` and `electrical_safety.md` before wiring loads.
4. Change the sample AP password and API key in `include/config.h`. Never commit a real secret to a public repo.
5. Keep all `Commissioning::*_VERIFIED` flags false until actual hardware is inspected. Keep optional safety pins at `-1` if the sensor/load is absent.
6. Build and upload. Select the actual serial port; do not hardcode a workstation-specific COM number.

```bash
pio run
pio device list
pio run -t upload --upload-port COM14
pio device monitor --port COM14 --baud 115200
```

Replace `COM14` with the port detected on your computer. PlatformIO will install the versions pinned by `platformio.ini` on first build.

## C. First boot and commissioning

- All load outputs must be disconnected or physically disabled for the first firmware boot.
- Open Serial Monitor at 115200 baud. Confirm all output pins are set safe, note the AP IP, and inspect sensor/API status.
- With no validated NVS state, the controller must show `CONFIG_RECOVERY_REQUIRED`. This is intentional. It will not silently claim an old `ACTIVE_PHASE` value from a different sketch was migrated.
- Change the firmware AP password and API key before use. Connect your laptop to the `SAFFRON-AERO` AP. Open the local fallback page at the IP printed in Serial, normally `http://192.168.4.1` if AP startup succeeds.
- In the fallback page, enter the random API key from your private firmware `config.h`; it stays in that tab's session storage rather than being embedded in the page source. Run the explicit commissioning action to save provisional phase profiles. This action does not verify or unlock high-current hardware.
- If commissioning flags or the hot-side threshold are changed later in `config.h`, only do so after documenting the physical inspection, rebuild and flash, then re-run the commissioning action to refresh the persisted snapshot. The action preserves the existing phase and profiles; it is not evidence that the circuit is safe.
- Check that phase changes survive a complete ESP32 restart before adding hardware outputs.

## D. Optional Python dashboard in device mode

On the same local network as the ESP32:

```powershell
$env:SAFFRON_DEVICE_URL = "http://192.168.4.1"
$env:SAFFRON_API_KEY = "your-same-random-api-key"
python app.py
```

The environment variable must match the API key in firmware `config.h`. The dashboard's default mode is device mode and does not simulate if the device is unavailable. You can use another station IP if the ESP32 joins a router. Avoid exposing the API to the public internet; the simple shared-key mechanism is intended for a small trusted local network, not hostile or multi-user environments.

## E. Before connecting high-current loads

Follow the signed commissioning record in `electrical_safety.md`, verify the component data sheets, use correctly sized wiring and fuses, validate the physical stop path, test flow/thermal/reservoir interlocks, and measure the real operating current. A working web UI or successful software test is not an electrical-safety test.
