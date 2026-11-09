# Data directory

This directory starts with clearly labelled synthetic demonstration data. When the dashboard runs, it creates `telemetry.csv` and `experiment_log.csv` here. Do not treat the sample rows or simulator output as physical observations.

- `sample_data.csv`: synthetic example rows used to demonstrate the schema.
- `telemetry.csv`: locally logged dashboard telemetry; rows include a `source` column (`ESP32` or `SIMULATED`).
- `experiment_log.csv`: operator-entered corm-batch observations.

Keep real experimental data backed up separately. This folder is not designed as an unlimited high-frequency data logger, and the ESP32's NVS is reserved for small persistent configuration rather than time-series data.
