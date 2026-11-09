# Dashboard user guide

## Modes

- **Device mode** (`python app.py`) connects to the ESP32 API. A failed connection remains an error; the dashboard must not fabricate live measurements.
- **Simulation mode** (`python app.py --simulate`) uses a simple synthetic signal to test the UI and CSV/export workflow. A warning banner remains visible. No hardware command is sent.

## Overview panel

Air temperature and RH come from the DHT22. The DS18B20 value is explicitly labelled as a cooling-loop measurement, but the exact physical location must still be confirmed by the operator. A sensor marked invalid or stale must not be treated as a reading. The active phase is a saved configuration label; it does not independently prove biological stage.

## Phase change

Choose a phase and use “Commit phase to controller.” The dashboard prompts for confirmation and asks the API for the result. In device mode, the ESP32 commits the change to NVS. A browser refresh must read the current device state rather than pushing its local form defaults into the controller. Phase advancement is manual because a timer alone cannot confirm saffron phenology.

## Profile edit

Select a phase, edit target/min/max, hysteresis and mist timing, then save. Targets must sit inside bounds. The dashboard validates the form and the firmware repeats validation on the device. In device mode, a write is displayed as saved only after the controller returns a successful result; a failure message means that success was not confirmed. The settings are still provisional and should not be copied into an agronomic report without evidence.

## Mode and emergency stop

“Automatic mode” requests stored automatic control, but it does not bypass any safety interlocks. “Manual mode / loads off” stops automatic demands and turns load commands off. The software emergency-stop action forces configured GPIO outputs LOW and persists a latch; it does not replace a physical disconnect. Clearing a latch requires an intentional command and the hardware stop circuit verification flag.

## Charts and CSV

The two charts display the most recent valid readings sampled while this page is open. They are not the authoritative record and reset when the page is reloaded. Use the CSV export endpoint for host-side telemetry history. Rows include an explicit source field so simulated and physical observations can be separated. Missing samples are not reconstructed.

## Experiment log

Use a batch ID and record only measured values. Corm count and mass should be weighed or counted. Flower count, harvest mass and daughter-corm count should remain blank until observed; never enter a desired value. Explain treatment changes, anomalies, condensation, sensor issues and outages in the observation field. Export experiment CSV separately from telemetry.

## Recovery and interlocks panel

The panel shows controller recovery stage, previous mode, saved phase, sensor validation, flow/reservoir feedback and whether the firmware actually reports recovery complete. A saved phase can be restored while the physical outputs remain locked. Do not interpret GPIO “ON” as proof of current draw, pump flow, fan rotation or TEC cooling.
