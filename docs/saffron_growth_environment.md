# Saffron growth environment: evidence notes for SAFFRON-AERO

**Purpose.** This document helps design an experiment; it is not an agronomic prescription. The chamber has a small thermal mass, a glass viewing section, uncertain airflow, two TEC modules, and a misting arrangement whose root-zone function is not yet demonstrated. All initial profiles in the firmware are **provisional software values**, not proven targets for aeroponic saffron in Odisha.

## How to interpret the literature

Saffron (*Crocus sativus* L.) is a fall-flowering geophyte that reproduces vegetatively through corms. Its floral differentiation can occur before an operator sees flowers. Temperature acts at different times in the corm cycle, so a single constant “ideal temperature” is a misleading simplification. Research from the Chinese two-segment system reports a warm/intermediate period followed by cooler conditions for flowering; other experiments examine corm storage temperature, greenhouse planting depth/density or field management. Those systems differ in substrate, ventilation, humidity, day/night profile, corm size and the climatic context. The numbers should be used as study conditions and hypotheses, not pooled into a universal temperature recipe.

### Evidence categories used here

- **Research-derived context:** a number or relationship explicitly reported by a cited publication, under that study's specific conditions.
- **Provisional engineering profile:** a placeholder used to make software and commissioning interfaces functional. It must be checked before it can influence real loads.
- **Experimentally calibrated:** a value supported by the user's own repeatable chamber measurements and crop outcomes. No project setting has this status until those trials occur.
- **Not measured / not installed:** the correct status when the hardware cannot observe a parameter. The software must show no data rather than fabricate a value.

## Phase-by-phase evidence and control implications

| Project phase | Biology / research interpretation | Environmental control and logging | Evidence strength and limitation |
|---|---|---|---|
| Corm dormancy / preparation | The corm can remain visually inactive while floral differentiation or internal physiological changes continue. Study timing, storage environment, corm size and origin all matter. | Log corm batch, mass, external storage history, daily air and corm/loop conditions. Avoid advancing based on an arbitrary timer alone. | Phenology is established; a single setpoint for every corm batch is not. Use provisional limits pending local agronomic advice. |
| Floral induction | Temperature during rest/induction influences subsequent flower formation. Wang et al. (2021) report a sequence involving about 30 °C for 2–3 weeks, then around 25 °C for 1–2 months, followed by about 15–20 °C for flowering in their specific Chinese “two-segment” cultivation model. Jose-Santhi et al. (2023) investigated temperature-associated metabolic regulation of flowering induction. | Record sensor position and actual corm treatment, not only controller setpoint. Do not blindly use the cited sequence if the storage/production model differs. | Direct study evidence for particular corm-treatment systems; transfer to this small aeroponic prototype is unvalidated. The firmware's 22 °C target is not a reproduction of the published protocol. |
| Flowering | Flower emergence is influenced by earlier corm history and temperature. Wang et al. report 15–20 °C in the final flowering period for their described sequence. Other regions and systems differ. | Prioritise stable actual temperature, observation of flower emergence, collection time and corm condition. Keep flower counts and harvest mass as measured values. | Temperature relationship is supported; the numeric band is contextual, not universal. Humidity target for this prototype is not validated by that paper. |
| Vegetative growth | Leaves support photosynthesis and daughter-corm development. Greenhouse research has reported vegetative development under cooler conditions, but lighting, nutrition, watering and substrate are important confounders. | Measure light exposure and chamber gradients if sensors are installed. Record leaf emergence and senescence, irrigation/root mist schedule and diseases. | Greenhouse/soil observations support hypotheses; not a validated aeroponic prescription. |
| Daughter-corm development | Daughter-corm quantity and size influence later cycles. Qaryouti et al. (2025) compared densities and planting depths in a cooled greenhouse; density affected per-area yield and individual plant/corm traits. | At cycle end, count and weigh daughter corms separately; compare survival and total mass with initial corm batch. Avoid claiming increased multiplication from a dashboard alone. | Useful controlled-greenhouse evidence; planting depth results do not translate directly to an aeroponic chamber without substrate. |
| Dormancy / cycle transition | Above-ground senescence and dormant period form part of annual phenology. Corms may be stressed by poor drainage, pathogens or incorrect storage. | Record senescence date, corm health, mass loss, storage condition and cleaning actions. Keep the previous phase saved across power cycles. | Lifecycle is well established; exact local transition criteria require experiment and expert input. |

## Temperature

The controller measures air temperature with a DHT22 and a DS18B20 assigned to a defined loop location. The temperature near the corms may differ from air temperature near a wall, hot-side heatsink, water block or viewing pane. Before comparing a profile with literature, log where each sensor sits and measure spatial gradients with a second calibrated reference thermometer. A DS18B20 attached to a cooling loop cannot be described as corm temperature. Nor does a commanded Peltier output prove that the chamber cooled: hot-side heat rejection, pump flow, contact pressure, insulation leakage, ambient temperature and power supply all influence response.

The papers cited above demonstrate that saffron responds to thermal history and stage. They do not give permission to choose a single arbitrary constant target for every stage. Firmware defaults are deliberately modest demonstration values and are not asserted to reproduce any paper. During commissioning, start with outputs disabled, gather passive environmental readings, and review the chosen schedule with a qualified saffron grower or agricultural researcher. A target, lower/upper bounds and sensor placement should be recorded together as one experimental protocol.

## Humidity, condensation and disease risk

Relative humidity depends on air temperature; it is not a direct measurement of water delivered to a root. High RH can reduce evaporative demand while also increasing condensation on cold panels, wires, sensor housings, corm surfaces and glass. Wet stagnant surfaces and contaminated water can support pathogens. The DHT22 is intended for chamber air monitoring, not immersed measurement, and should not be installed where it is regularly sprayed or condensed upon. Record RH and temperature together, inspect the chamber for visible wetness, and use a separate condensation/leaf-wetness sensor if a research question requires it.

This project does not claim that 60% RH is an experimentally established saffron optimum. It is a placeholder target in the software profile. The allowed lower/upper values are configurable guardrails, not botanical guarantees. Do not respond to a bad DHT22 reading by turning a humidifier fully on; the firmware locks the output when the measurement is stale or invalid.

## Root-zone misting is not the same as humidifying chamber air

An ultrasonic atomizer placed in the chamber can increase airborne moisture; it does not prove that every root receives a controlled droplet flux, a sufficient wet/dry cycle, oxygen, or a defined nutrient solution. A true aeroponic subsystem needs a root enclosure, nozzle/atomizer placement, droplet behaviour, drainage, root coverage, reservoir hygiene, water quality and safe protection against dry running. Until those parts are built and characterised, describe the system as *mist-assisted* or *aeroponic concept under evaluation*, rather than proven aeroponic crop production.

The firmware's mist relay requires a verified driver and reservoir-level switch before activation. The current logic is relative-humidity-driven, which suits chamber humidification more directly than a root-zone schedule. If the hardware is eventually used for root wetting, revise the control law and interlocks around actual root-zone delivery, not ambient RH alone; the actuator function should be renamed in configuration so the UI cannot confuse the two approaches.

## Light and glass section

A glass panel permits observation, but transmitted sunlight can create local heat gain, glare and gradients. A sensor on one wall does not measure photosynthetically active radiation (PAR), UV dose or plant-effective light unless its spectral response and calibration are suitable. The supplied firmware marks the optional light sensor as not installed. It must not display a made-up lux figure. If added, document the model, output type, spectral range, calibration, placement and whether it is shielded from direct condensation. Light exposure should be logged alongside temperature so the role of the viewing section can be evaluated experimentally.

Do not add a UV lamp as a generic saffron treatment. UV decisions need an evidence-backed wavelength, irradiance, exposure duration, shielding, eye/skin safety assessment and independent controls. Unspecified UV exposure is not a safe default.

## Root moisture, aeration, nutrient solution, pH and EC

Conventional saffron research usually discusses soil properties, irrigation, planting depth, mother-corm size and weather. An aeroponic adaptation changes the root environment, so the soil findings cannot provide numeric values for pH or electrical conductivity (EC) of a nutrient solution. This starter project has no pH or EC sensor and makes no claim to regulate a nutrient formulation. Do not invent these data in the dashboard. Future tests should log water source, reservoir volume, pH, EC, temperature, mist/nozzle pattern, pulse/rest time, drainage and root/corm observations. Start small, use clean water and containers, and obtain specialist guidance before trying fertiliser concentrations on valuable corms.

## Corm batch and planting density

Corm source, mass, health, storage history and disease state influence outcomes. The current note says approximately 25 Kashmiri corms at roughly 17–20 g each, but the actual count/mass should be weighed and entered in the experiment log. A corm weight estimate from a supplier is not a measured baseline. Qaryouti et al. (2025) examined 67, 100 and 200 corms per square metre in a cooled greenhouse with conventional planting depths. Those densities cannot be converted directly into a recommendation for this small chamber without the usable plant area, root access and cultivation method. Record the actual spacing and root arrangement rather than reporting only chamber dimensions.

## Research-backed numbers: use with context

| Reported condition | Source / study context | Appropriate use here | Inappropriate claim |
|---|---|---|---|
| About 30 °C for 2–3 weeks, then about 25 °C for 1–2 months, then about 15–20 °C for flowering | Wang et al. (2021), study of a specific Chinese two-segment production approach | Hypothesis for comparative reading, subject to corm storage history and expert review | “The universal saffron growth recipe” or “validated aeroponic settings” |
| Flowering induction is temperature-dependent and high ambient temperature supports induction in the studied dormant-corm context | Jose-Santhi et al. (2023), experimental/molecular study | Evidence that treatment timing matters; motivates phase-specific research | A field-scale commercial yield prediction |
| 67, 100 and 200 corms/m², and 8 cm versus 13 cm planting depth | Qaryouti et al. (2025), cooled greenhouse planting experiment | Example of density/depth trade-offs to design later controlled comparisons | A direct root-spacing recommendation for an aeroponic device without soil |
| Temperature, field age, soil texture/bulk density, soil/water pH and irrigation related to yield across 13 fields | Pirasteh-Anosheh et al. (2023) | Shows that environmental and management variables are coupled | An exact pH/EC setpoint for this prototype's mist reservoir |

## Practical monitoring procedure

1. During initial sensor-only operation, log at least air temperature, RH, DS18B20 position, ambient temperature outside the chamber, time of day, visible condensation and glass-panel conditions.
2. Verify sensor readings against a trusted reference across the actual operating range. Store offsets only after measurement and record the reference instrument.
3. When cooling loads are eventually enabled, log driver command, pump command, flow-switch state, hot-side temperature, ambient temperature and energy consumption. Do not infer actual TEC power from a percentage field.
4. Keep crop observations in a separate experiment log with count and measured mass. A telemetry trace is not proof of flowering or stigma quality.
5. Preserve no-data gaps, restarts and power interruptions. Do not interpolate lost data without labelling it as a model estimate.
6. Compare the prototype with an untreated/reference group if the research goal is to claim an agricultural benefit. Randomise corm allocation where feasible and repeat across more than one corm batch or cycle.

## References

- Wang, Z., Li, X., Xu, J., Yang, Z., & Zhang, Y. (2021). Effects of ambient temperature on flower initiation and flowering in saffron (*Crocus sativus* L.). *Scientia Horticulturae*, 279, 109859. https://doi.org/10.1016/j.scienta.2020.109859
- Jose-Santhi, J., Sheikh, F. R., Kalia, D., & Singh, R. K. (2023). Sugar metabolism mediates temperature-dependent flowering induction in saffron (*Crocus sativus* L.). *Environmental and Experimental Botany*, 206, 105150. https://doi.org/10.1016/j.envexpbot.2022.105150
- Askari-Khorasgani, O., & Pessarakli, M. (2019). Shifting saffron (*Crocus sativus* L.) culture from traditional farmland to controlled environment (greenhouse) condition to avoid the negative impact of climate changes and increase its productivity. *Journal of Plant Nutrition*, 42(19), 2642–2665. https://doi.org/10.1080/01904167.2019.1659348
- Cardone, L., Castronuovo, D., Perniola, M., Cicco, N., & Candido, V. (2019). Evaluation of corm origin and climatic conditions on saffron (*Crocus sativus* L.) yield and quality. *Journal of the Science of Food and Agriculture*, 99(13), 5858–5869. https://doi.org/10.1002/jsfa.9860
- Pirasteh-Anosheh, H., et al. (2023). Climate and management factors influence saffron yield in different environments. *Agrosystems, Geosciences & Environment*, 6(3), e20418. https://doi.org/10.1002/agg2.20418
- Qaryouti, M., Al-Soqeer, A., Abdelaziz, M. E., Gruda, N. S., AlSahly, S., Alrasheed, W., Althobiti, S., Babiker, O., Sharafeldin, M., & Voogt, W. (2025). Cutting-edge greenhouse practices for better stigma yield and corm quality of saffron (*Crocus sativus* L.). *Frontiers in Plant Science*, 16, 1612791. https://doi.org/10.3389/fpls.2025.1612791
- Acharya, B. S., Ghalehgolabbehbahani, A., Hamido, S., Zinati, G., Bozzolo, A., Archer, L., Wendelberger, K., Das, S., Thapa, R., & Panday, D. (2025). Saffron (*Crocus sativus* L.): The golden spice—management, challenges, and opportunities for sustainable production in the United States. *Journal of Agriculture and Food Research*, 21, 101970. https://doi.org/10.1016/j.jafr.2025.101970
