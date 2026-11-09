# Troubleshooting

| Symptom | Likely causes | Safe checks / response |
|---|---|---|
| `CONFIG_RECOVERY_REQUIRED` after first flash | No configuration exists in this firmware's NVS schema; expected on first use | Use the explicit commissioning action with loads disabled. Do not silently assume an old firmware's compile-time phase was migrated. |
| Configuration is invalid or cannot be saved | NVS error, incomplete/corrupt slot, schema mismatch, storage write failure | Keep outputs off. Read save status and Serial logs. Inspect both slots; do not erase NVS before exporting/reviewing data. |
| Phase changes back after reboot | Change was not committed, save failed, user flashed different partition layout, or storage was reset | Confirm API save response/revision, reboot and inspect actual `/api/config`; do not infer persistence from a local form value. |
| Sensor is `NO DATA` | Unplugged sensor, invalid library read, poor wiring/pull-up, condensation, too-short startup, faulty sensor | Keep affected loads off. Check wiring with power disconnected, then monitor Serial with loads disabled. |
| DS18B20 reads near 85 °C or −127 °C | First conversion sentinel, wrong bus wiring, absent probe, or probe issue | Treat as invalid until conversion and wiring are verified. Do not use it as a real temperature. |
| Recovery stuck at `SENSOR_RECOVERY` | DHT22 or DS18B20 never becomes fresh/plausible | Verify the required measurements and actual probe placement. Do not bypass the interlock. |
| Recovery says cooling not commissioned | One or more driver/pump/fan/flow/hot-side configuration flags/pins are not verified | This is expected for the supplied safe defaults. Complete the written driver and safety commissioning checklist. |
| Pump runs but TEC remains off | Flow not proven, hot-side reading absent/stale/over limit, prime period incomplete, or minimum-off time not elapsed | Stop and inspect flow, sensors, driver config and thermal assembly. The pump command is not proof of flow. |
| Mist remains off | Driver unverified, reservoir switch absent/low, DHT invalid, manual timeout expired, or RH demand not met | Confirm atomizer role and reservoir sensor; never bypass interlocks to demonstrate an ON state. |
| API returns 401 | Firmware API key still the placeholder or dashboard key differs | Change key in firmware configuration and set matching `SAFFRON_API_KEY` in the dashboard environment. |
| Dashboard reports unavailable controller | Wrong IP, AP not joined, API key mismatch, device reboot or HTTP failure | Check Serial IP, laptop route and firmware status. Device mode intentionally does not fake readings. |
| Simulation is visible | Dashboard launched with `--simulate` | Stop and relaunch `python app.py` for device mode. Simulation is not real sensor data. |
| OLED absent but Serial/API work | I²C address/wiring/logic-level issue or absent display | Verify SDA/SCL and address; control loop must remain active if OLED fails. |
| Real chamber fails to cool | Insufficient hot-side heat rejection, bad contact/thermal paste, inadequate supply/driver, poor insulation, pump/flow fault | Disconnect and inspect hardware. Firmware percentages/commands do not measure thermal capacity or electrical power. |

Never troubleshoot high-current TEC circuits with exposed live conductors. Use the physical disconnect and competent electrical review.
