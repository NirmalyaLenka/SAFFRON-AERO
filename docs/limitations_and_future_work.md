# Limitations and future work

## Current limitations

- The chamber's actual internal volume, air circulation, leakage, glass-panel thermal properties and temperature uniformity have not been measured.
- The exact TEC driver topology, current capability, switching behaviour, thermal path, PSU distribution and wiring are not known. Peltier outputs are disabled by default.
- Pump/fan pins, flow switch, hot-side DS18B20 and reservoir-level switch are unconfigured. Firmware intentionally refuses to claim full recovery without configured interlocks.
- Firmware C++ must be built against the pinned PlatformIO environment and then bench-tested on the physical board. Python tests cannot validate microcontroller behaviour.
- DHT22 and DS18B20 locations and calibration are not yet verified. Only air and one loop sensor are in the default code path; no actual electrical output/current feedback exists.
- The misting subsystem's role, atomization pattern and root coverage are not scientifically validated. Air humidity is not root-zone moisture.
- There are no pH, EC, nutrient concentration, reservoir volume, root-zone temperature, root-moisture, PAR, UV, CO₂ or flower/corm-health sensors in the default build.
- The dashboard does not offer a hardened, multi-user identity/security model. Do not expose it to the public internet.
- Wall-clock time is not reliable after long offline power loss without a backup RTC. Missing outage data cannot be reconstructed by the ESP32.
- No real biological yield, water savings, energy savings, climate resilience, commercial return or comparative production result has been established.

## Future priorities

1. Verify the physical driver/supply/wiring and make a signed commissioning record.
2. Add uniquely addressed hot-side/coolant sensors, tested flow and reservoir feedback, a physical stop path and independent thermal cut-off where appropriate.
3. Instrument voltage/current and energy so the cooling cost is measured.
4. Test the empty chamber and thermal gradients before a crop trial; measure glass heat gain and condensation.
5. Define a root-zone misting system if aeroponics is the actual goal; use actual root coverage and moisture delivery criteria.
6. Add an independent RTC/logger if wall-clock timestamps and outage drift matter.
7. Establish controlled crop trials with batch tracking, a reference group and repeat measurements.
8. Only then consider a robust enclosure, redundant sensing, OTA update policy, signed firmware, stronger authentication, more durable time-series storage and any scale-up claim.
