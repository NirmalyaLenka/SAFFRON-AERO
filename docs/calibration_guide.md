# Sensor calibration and placement

## Air temperature and RH (DHT22/AM2302)

Mount in representative chamber air, away from direct atomizer spray, wet glass, direct sunlight, TEC cold plates, radiator exhaust, and the ESP32's own heat. Note the height and distance from corms. A compact chamber can contain gradients; one air sensor does not represent every root/flower location.

Compare the sensor against a trusted reference thermometer and a suitable RH reference where available. Record serial/model, reference instrument, test conditions, correction value, date and uncertainty. Apply a software offset only when supported by the comparison. Never “calibrate” by changing the offset until the dashboard looks plausible. DHT22 is not suitable for submerged readings and can be affected by condensation.

A sample should be marked valid only if the library returns a finite, plausible value and its timestamp is recent. The firmware reads approximately every 2.5 seconds and treats values older than 10 seconds as stale. That is a software freshness policy, not a guarantee of sensor accuracy.

## DS18B20 probes

Record each 1-Wire ROM address, cable, waterproofing, physical location and role. The code treats index 0 as the primary coolant-loop probe and only considers a separate hot-side probe when `Pins::HOT_SIDE_SENSOR_INDEX` is set and commissioning is completed. Do not use an index value without first enumerating and verifying the probe addresses: an index alone is not a physical identity guarantee.

The code rejects missing/implausible library values and the common power-up sentinel near 85 °C, but this simple plausibility check is not a calibrated metrology procedure. A reading may be numerically plausible while the sensor is detached from the surface. Attach probes securely and compare them against a reference under steady conditions.

## Optional flow, reservoir and light inputs

The firmware expects the flow and reservoir inputs to read HIGH when safe. Verify the actual switch's normally-open/normally-closed behaviour, signal voltage, pull-up/down, and fault state under unplugged wires. A disconnected sensor must fail safe where practical. If the physical switch requires a different polarity, adjust code and test it before setting the related `VERIFIED` flag.

The light sensor is not installed in the default build. If added, identify sensor model, measurement unit, spectral response, range, calibration and location. Lux is not identical to PAR. Do not report UV dose without a wavelength/irradiance instrument and defined measurement method.

## Calibration record template

| Field | Record |
|---|---|
| Calibration ID / date | |
| Sensor manufacturer, model, serial/address | |
| Physical position and function | |
| Reference instrument and calibration status | |
| Conditions and stabilisation period | |
| Raw readings / reference readings | |
| Offset or correction method | |
| Repeatability / uncertainty note | |
| Performed by / reviewer | |
| Next check or replacement trigger | |

Do not enter derived data as directly measured data. Preserve the raw reading alongside any corrected value in formal experiments.


The saved configuration has separate offsets for DHT22 air temperature/RH, the coolant-loop DS18B20 and the optional hot-side DS18B20. Calibrate each probe at its actual installed location and document its reference instrument, observed error and date. Do not apply a coolant-loop calibration offset to the hot-side probe by assumption. The hot-side probe is not available unless its DallasTemperature device index/address is verified and `HOT_SIDE_SENSOR_VERIFIED` is enabled after inspection.
