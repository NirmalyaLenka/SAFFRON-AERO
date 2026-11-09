# Experimental protocol

## Questions

1. How much does the empty insulated chamber's temperature vary from ambient under different glass-panel exposure conditions?
2. After electrical commissioning, can the verified cooling assembly reach a chosen test band without violating a measured hot-side limit or flow condition?
3. What are the actual energy use, cooling response and condensation burden?
4. In a controlled, documented small-corm trial, are flowering, stigma mass, survival and daughter-corm outcomes different from a suitable reference condition?

Do not assume the answer is positive. The device should be measured as an engineering system before crop conclusions are attempted.

## Experimental units and baseline

Record each corm's unique identifier if practical, batch/source, actual mass, health grade, pretrial storage, initial phase and condition. If corms are split into treatment and reference groups, allocate them in a way that avoids giving all heavier corms to the treatment arm. A reference group should be exposed to a clearly described environment; “normal” is not a reproducible control description. Keep sample size and exclusions visible.

## Sensor layout

Record the sensor model, serial/ROM address, location, height, airflow exposure, reading interval and calibration information. If possible use a second reference logger to quantify gradients between the centre, glass section, root zone and coolant loop. The supplied default wiring has only one air-temperature/RH sensor and a DS18B20 assumed to be the coolant loop; this cannot resolve all microclimates.

## Treatment protocol

Before crops are present, define target/min/max, hysteresis, allowed output conditions and stop limits based on reviewed literature and actual system capacity. Record profile version and configuration revision. Do not change setpoints after seeing an inconvenient outcome without logging the intervention. Each phase change should record who made it, the observation that motivated it and the time known to the operator. The phase controller does not use a timer to declare flowering.

## Data collection

Collect temperature, RH, coolant/hot-side temperature where instrumented, pump/fan/flow states, output command, supply voltage/current, energy, illumination if installed, external ambient conditions and anomalies. The ESP32 event log uses boot-relative time; an independent clock/logger is needed for trustworthy outage-period timestamps. Mark unpowered intervals as unknown.

Crop observations should include flower count, harvest date, number and quality of stigmas if assessed, fresh/dry mass and drying protocol, visible mould/disease, leaf-emergence and senescence dates, daughter-corm number and individual/total mass. Use consistent weighing instruments and units. Do not infer pigment quality or chemical composition from appearance alone; that requires an appropriate assay.

## Analysis

Predefine summary statistics and compare distributions between experimental units. Plot temperature and RH with data gaps preserved. Calculate time-in-band only after justifying the band and use the recorded sensor sampling intervals. Energy use should be measured with a suitable meter; commanded duty or a GUI slider is not a wattmeter. Separate engineering response from crop results. Describe confounders such as corm size, source, temperature history, room ambient, glass exposure, airflow, mist distribution, water quality, density and sanitation.

## Reproducibility record

Preserve firmware version, dashboard version, configuration revision, profile export, wiring revision, component identities, calibration records, experiment CSV, raw sensor logs, analysis code and photos of the actual arrangement. Do not edit raw records; make corrected/analysed copies with a record of the transformation.
