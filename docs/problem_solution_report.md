# SAFFRON-AERO: Offline-First Aeroponic Saffron Cultivation with Persistent Phase-Aware Climate Control

**Problem–solution and engineering research report**  
**Prepared:** 9 November 2026  
**Status:** Engineering prototype specification and research plan; physical validation and saffron production results are outstanding.

> **Scope note.** This report distinguishes published findings, project assumptions, software implementation, proposed experiments and outcomes not yet measured. The project can be useful even if the crop hypothesis fails: a rigorously measured negative result is better than a visually convincing but unsupported success claim.

## Executive summary

Farmers experience rainfall irregularity, heat stress, unseasonal precipitation, drought and other environmental pressures that can disturb crop calendars, field operations and harvest quality. A farmer also needs information that relates to a specific location and decision: whether a field is likely to be too wet for an operation, whether heat risk is rising, or whether irrigation should be reviewed. These needs are not solved by a single sensor enclosure. This project investigates one narrow response: measure and control the microclimate of a small experimental saffron corm chamber and log data so that the usefulness and limits of local climate control can be tested.

SAFFRON-AERO uses an ESP32 as the autonomous controller, a DHT22 for air temperature and relative humidity, a DS18B20 for a defined cooling-loop temperature, a small OLED, a local browser API and an optional Python dashboard. The physical concept is an insulated enclosure of approximately 2 ft × 1.5 ft × 1 ft, with roughly 3 cm of expanded-polystyrene insulation and a glass viewing section. Two TEC1-12715 thermoelectric modules, a liquid cooling loop, fans, a 12 V pump and an ultrasonic atomizer have been described as candidate equipment. Their exact driver circuits and complete sensing/interlock suite are not yet verified; therefore the software defaults keep high-current outputs disabled.

The primary software goal is reliable recovery after a power interruption. A saved cultivation phase must not silently revert to a compiled constant such as `ACTIVE_PHASE`. The firmware stores versioned configuration in alternating non-volatile memory slots, validates the payload with a checksum and semantic checks, restores the latest valid state, rejects stale readings, and checks subsystem interlocks before it allows automatic output control. If the previous mode was manual, the controller restores settings but not an old forced-output command. If the configuration is invalid, the controller enters a visible recovery state rather than inventing a replacement. The controller can recover after electrical power returns; it cannot hold a setpoint while every sensor and actuator is unpowered.

The Python dashboard is optional, not a control dependency. In explicit simulation mode it uses clearly labelled synthetic data. In device mode it displays unavailability rather than substituting synthetic readings. The local ESP32 API and fallback page can operate without internet access; core control does not require a cloud service, laptop, router or NTP. Persistent configuration is stored in NVS; event records use a flash filesystem rather than writing live temperature values to NVS every second.

Scientific caution is essential. Saffron flowering depends on the corm's thermal history and growth stage. Research describes temperature sequences and controlled greenhouse conditions, but those results cannot automatically be transplanted to this small aeroponic/mist-assisted chamber. No peer-reviewed evidence in this starter bibliography validates exact saffron setpoints for this build, and no flowering, stigma yield, energy savings, water savings or commercial benefit is claimed. Initial targets are provisional software values to make the control path testable, not agronomic prescriptions.

---

## 1. Agricultural problem statement

The motivating problem is that farmers face unpredictable weather: irregular rainfall, heat stress and extreme weather events can disrupt sowing and harvesting cycles, while many producers do not have timely, location-specific advice for planning. It is tempting to reduce this to a hardware statement—“install an IoT sensor and climate change is solved”—but that would be an invalid leap. The actual decision problem spans observation, forecast quality, local context, uncertainty, farmer trust, access, and a response that can realistically be carried out. A low-cost sensor can observe some variables at one point; it does not automatically predict conditions elsewhere or recommend the right farm operation.

For saffron, environmental conditions interact with corm size and origin, storage history, planting management, soil or root-zone properties, water availability, disease, harvest handling and season. A protected chamber can deliberately alter a small microclimate, but an enclosure is only a relevant solution where the crop material is suitable, the controllable variables matter at the chosen phase, the energy and labour costs are acceptable, and results are repeatable. A chamber may also introduce new risks: condensation, uneven air temperature, excessive heat on the TEC hot side, and root-zone moisture delivery that looks active but is biologically ineffective.

The engineering statement for this iteration is therefore narrower: **Can a small, instrumented, phase-configurable saffron chamber safely record its conditions and, after appropriate hardware commissioning, regulate selected environmental variables offline while preserving its settings through power interruptions?** This is testable with software invariants and measured physical trials. It is not equivalent to proving climate resilience at field scale. The agricultural benefit is a hypothesis to be evaluated, not a foregone conclusion.

## 2. Weather variability and crop impacts

Weather affects crop processes through the timing, magnitude and duration of exposures, rather than only through a seasonal average. Heat during a sensitive reproductive stage can have a different effect from the same temperature during dormancy; rainfall at sowing can be beneficial in one soil and damaging in a poorly drained one. A short but severe event can disrupt field access or damage harvestable material, and an extended period of moderate stress can change growth rate or disease pressure. This is why an engineering report should record distributions and excursions rather than a daily mean alone.

For saffron, studies have identified temperature and management as important influences on flowering and yield. Wang et al. (2021) describe how a sequence of temperature treatments in a specific Chinese two-segment production model influenced flower initiation and flowering. Pirasteh-Anosheh et al. (2023) examined 13 fields across environments and found multiple factors associated with yield, including temperature, field age, soil texture and water-management variables. Those findings support stage-aware environmental observation, but they also warn that a single climate variable is insufficient to explain crop performance. They do not imply that a miniature chamber can eliminate regional weather risk.

A chamber can isolate an experiment from some outdoor variation while creating a new indoor boundary condition. Its performance will depend on insulation, leakage, glass-panel solar gain, airflow, load capacity and the room temperature around it. If a thermal target cannot be maintained on a hot day, the system must record that fact rather than conceal it behind the intended setpoint. If the chamber is stable only at the cost of high energy use, that cost is part of the result. The device should support trustworthy measurement before any climate-mitigation claim is made.

## 3. Location-specific agricultural weather advisories

A weather advisory and a chamber controller serve different jobs. An advisory uses weather observations, forecast products, crop or soil context, and decision rules to help a farmer choose an action. Its usefulness depends on location resolution, forecast lead time, measured forecast skill, how uncertainty is communicated, and whether the suggested action is practical. A broad regional forecast may miss a local rain cell or a small valley's temperature inversion. A point weather station may be precise for its own location but not representative of a large field.

The SAFFRON-AERO prototype includes no forecast engine and does not currently fetch third-party weather data. Its local sensor data are measurements from inside one small chamber, not a field-scale weather station. A future extension might display an external weather forecast alongside chamber conditions, but forecast values must remain labelled as forecast data, with provider, issue time, validity period, units, location and uncertainty where available. The control loop should not depend on the forecast endpoint: if the network is unavailable, local safety logic and phase control should continue unchanged.

To support actual advisory research later, a separate design would be required. It would identify a target farmer group, a specific decision, the spatial scale and lead time needed, the trusted data source, the fallback action when data are missing and a way to evaluate whether advice changed an outcome. The current enclosure can contribute experimental microclimate observations to that broader ecosystem, but it cannot replace the meteorological, agronomic and human-centred components of an advisory service.

## 4. Why saffron is the experimental crop

Saffron is a high-value crop propagated through corms and valued for its red stigmas. It is a useful experimental subject because its phenology includes visually distinct events, corm size and history matter, and environmental conditions during the dormant/induction period influence subsequent flowering. Protected-cultivation studies already investigate how temperature and greenhouse practice can shape the crop. This provides a literature base for constructing a phase-aware monitoring system and for asking whether controlled conditions can be reproduced in a small device.

The choice is not proof of suitability for the local climate, the chosen chamber or commercial production. Saffron's high value can tempt a project team to extrapolate a positive return from a small flowering demonstration, but labour-intensive flowering/harvest and the need to manage corm multiplication complicate the economics. Corms sourced from a region with a different production calendar may carry a different storage/induction history. A brief successful flower event cannot tell the team whether later vegetative development and daughter-corm formation were healthy or sustainable.

The available batch is described as approximately 25 Kashmiri saffron corms with masses around 17–20 g each. That description should be treated as provisional until the count, individual mass and condition are recorded. Corms should receive unique labels where practical, because pooled measurements can hide uneven initial size. The experiment should record both flowering and follow-on corm outcome, otherwise it risks optimising the visually impressive but incomplete part of the life cycle.

## 5. Saffron biology and growth phases

The corm stores resources and supports seasonal growth. Flower development, leaf growth and daughter-corm formation occur in a sequence whose timing depends on environment and cultivation practice. The biological stage is not always observable directly: floral induction can take place during a period when the corm looks inactive, while visible flowering is a later event. This means that a software phase label should record the intended treatment or operator assessment, not pretend that an algorithm has directly sensed the internal state of the corm.

A useful control interface needs at least six organisational phases: Corm Dormancy/Preparation; Floral Induction; Flowering; Vegetative Growth; Daughter-Corm Development; and Dormancy/Cycle Transition. These labels provide a repeatable way to associate setpoints, observations and references with a protocol. They should not create an automatic biological schedule. An arbitrary timer can announce a phase change even when a corm has not reached the expected stage. The user should advance phases using observations and pre-defined experimental criteria, with the date and reason recorded.

Each phase profile contains temperature target and bounds, RH target and limits, hysteresis, mist pulse/rest values, a provisional-evidence label and notes. Later versions can add environmental schedules and additional measurements, but only where real sensors exist. The phase profiles are stored in persistent configuration. Restoring a phase after reboot restores the intended profile; it does not imply that the crop progressed while power was out or that the environment remained stable during an outage.

## 6. Saffron environmental research

Three evidence streams are especially relevant. Wang et al. (2021) report a warm/intermediate/cool temperature sequence in a specified two-segment cultivation model. Jose-Santhi et al. (2023) study temperature-dependent flowering induction and sugar metabolism. Qaryouti et al. (2025) investigate corm density and planting depth in a cooled greenhouse, documenting trade-offs between yields per unit area and individual corm/plant traits. These studies show that temperature history and cultivation method matter, but their experimental treatments and outcomes cannot simply be copied into the current hardware.

A numerical value is only meaningful with its context: sensor location, day/night cycle, corm source and mass, storage regime, substrate, airflow, watering, light, cultivation environment and measured endpoint. A paper describing 15–20 °C during a flowering transition does not prove that a single air sensor should control a small box to 17 °C at all hours. Similarly, an experimental greenhouse density in corms per square metre does not directly map to a root-suspended system without a clearly defined usable area and planting/root arrangement.

The present firmware's default targets are a software scaffold, not a conclusion from these papers. Profiles must be marked provisional until the target system is calibrated and crop outcomes can be linked to the exposure history. A future research version should document the evidence grade for each profile and allow a supervisor to approve a profile separately from physical-output commissioning. This distinction stops a user-interface value from appearing more scientifically certain than it is.

## 7. Existing cultivation methods

Traditional soil-based saffron production depends on local climate, soil texture and drainage, corm size and density, irrigation/rainfall, planting date/depth, disease management and labour. Greenhouse cultivation changes the boundary conditions and can give the grower more control over temperature, water and exposure. It can also bring expenses related to structures, ventilation, heating/cooling, electricity, monitoring and root-zone management. Controlled conditions reduce some variability, but they do not erase the crop's biological requirements or guarantee a higher return.

The 2025 greenhouse work by Qaryouti and colleagues is informative because it explicitly evaluates planting density and depth in a controlled environment rather than assuming that more plants always means a better crop. Greater density can increase production per unit area while reducing individual-plant performance; corm multiplication is an additional objective, not identical to stigma mass in one harvest. A grower who values future planting material may rationally select a different density from a grower optimising current-year stigma output.

SAFFRON-AERO should therefore describe its arrangement accurately: it is an insulated, instrumented chamber with a mist-assisted root-zone concept. Until the root enclosure, nozzle/atomizer, wet/dry cycle, drainage, coverage and nutrition are validated, the term “aeroponic” describes the investigation goal rather than an established agronomic result. Comparisons with soil and greenhouse methods must state what differs and which outcome is being measured.

## 8. Controlled-environment agriculture

Controlled-environment agriculture (CEA) uses structures and technologies to change the growing environment. The term covers a wide range, from passive shelters and greenhouses to highly instrumented indoor systems. A control loop may regulate air temperature, RH, light, CO₂, irrigation or nutrient solution, but the correct combination depends on crop and production goal. More instrumentation is not automatically better: each sensor adds calibration, maintenance, failure modes, and a responsibility not to misinterpret data.

In this prototype, control authority is bounded by the hardware that has been verified. A thermostat can request cooling, but cannot create cooling capacity that a TEC assembly lacks. A dashboard can plot a target against sensor data, but cannot prove that all corms experience that condition. A local fan may improve mixing but can also deliver hot radiator exhaust into the chamber if placed incorrectly. A small glass panel can provide observation and indirect daylight while raising thermal gain in direct sun. The experiment must determine which of these effects dominates.

The practical objective is a controlled and measurable test environment, not a decorative automation display. Start with passive measurements; determine thermal gradients and leak paths; then test the active cooling arrangement without corms. Only after the control response is characterised should a crop trial begin. This sequence separates engineering failure from biological response and avoids sacrificing valuable corms to an untested controller.

## 9. Aeroponics and its uncertainties

Aeroponics generally means that roots are suspended in air and receive water/nutrients via a mist or spray. The root-zone design must provide appropriate coverage, oxygenation, water quality, drainage and a controllable moisture cycle. The atomizer's droplet distribution, root-chamber geometry, nozzle location, pressure/flow, duty cycle and failure behaviour all affect delivery. A humid atmosphere can exist while roots remain dry; conversely, excess condensation can keep surfaces wet without delivering a balanced nutrient solution.

The current ultrasonic mist maker may function as a chamber humidifier. Unless its actual installation and root coverage have been demonstrated, the device should not claim successful aeroponic delivery. There is no pH, EC, nutrient dosing, reservoir volume or root-zone moisture measurement in the default sensor set, so software must not invent those values. If the build evolves into a genuine aeroponic arrangement, the project should add a dedicated root enclosure and measurable delivery criteria, then research a suitable saffron-specific moisture/nutrient approach with an agronomist.

The first valid experiment could compare two clearly defined moisture-delivery conditions or simply measure the pattern and volume delivered before planting. Record reservoir water source, cleanliness, volume, temperature, pulse duration/rest period, root coverage, drainage and evidence of disease. A pulse setting copied from another crop is not a validated saffron setpoint. The controller should guard against a stale RH sensor and low reservoir regardless of whether the operator expects the mist maker to be active.

## 10. Project objectives and research questions

The project objectives are: preserve the active phase and committed profile across restarts; avoid unsafe output commands when the sensor or hardware state is uncertain; continue local control without internet or dashboard; expose real controller state through a local API; record events and observations; and establish a reproducible path from simulation to hardware test. The agricultural objective is separate: measure whether the protected microclimate is controllable and whether a well-designed crop experiment supports any biological benefit.

Research questions should be answerable from data. For the empty chamber: how does internal temperature vary with ambient temperature and glass exposure? For active cooling: what is the measured cooling response, electrical energy per unit time, hot-side temperature and condensation burden? For persistence: do saved settings survive reset and interrupted writes, and do sensor faults leave loads off? For crop production: how do flowering, dry stigma mass, corm survival and daughter-corm outcomes compare with a stated reference group?

The first three questions can be addressed through engineering measurements and software/bench tests. The fourth requires a biological trial with corm-level records, an appropriate comparison, pre-defined outcomes and repeatability. The software interface is infrastructure for that work, not evidence that the hypotheses are true. The project's release checklist should keep objectives and outcomes distinct so an unfinished agricultural experiment is not hidden by finished code.

## 11. System requirements

Functional requirements include six named phases; separately configurable environmental profiles; sensor validity and age reporting; persistent configuration; dual-slot recovery; an explicit first-time commissioning route; timed manual overrides; an emergency-stop latch; local Wi-Fi AP operation; a local API; an optional dashboard; event logging and CSV export; and simulation for interface testing. The ESP32 must remain the autonomous control authority when the dashboard closes or the internet disappears. Opening the dashboard must never push its default values into the controller.

Safety requirements override convenience requirements. Every output starts in a defined safe state. The phase can be restored even when a sensor is invalid, but that restored phase does not grant power permission. TEC output requires an inspected driver, pump and fan control, a verified flow switch, a correctly identified hot-side temperature probe, fresh readings and a safe thermal condition. Misting requires a verified driver, a valid RH reading and reservoir-level feedback. Missing safety instrumentation keeps the affected output disabled. Software should never label a GPIO request as verified electrical output.

Non-functional requirements include non-blocking scheduling, plain error explanations, explicit source labels, documentation for unknown components, pinned dependencies, automated tests and a clear build report. Good human factors are important: an error should tell an operator what failed, rather than just displaying a red indicator. But interface quality does not compensate for missing measurement, physical protection or scientific uncertainty.

## 12. Chamber construction and insulation

The stated chamber size is approximately 2 ft × 1.5 ft × 1 ft, with about 3 cm of thermocol/EPS insulation. Those are approximate external dimensions, not a verified internal growth volume. Panel thickness, seams, lid fit, cable penetrations, tubing, mounting brackets and the glass area can create thermal bridges and leakage. Before performing a thermal analysis, measure the internal geometry and record materials and panel thickness. Avoid converting the outside dimensions into a claimed effective root-zone volume without checking what equipment occupies it.

Insulation reduces heat transfer but does not remove the heat load from TEC electronics, radiation through glass, conduction through frames or warm air entering through gaps. The viewing section provides observation and possible indirect daylight, yet it can admit solar heat. A single air sensor can miss a hot pocket at the glass or near the cooling block. Place sensors where they represent the measurement goal and add a temporary reference logger at several positions during chamber characterisation.

Thermocol can be damaged by heat, compression or moisture, and it is not a substitute for proper electrical enclosure design. Support heavy modules and water blocks mechanically rather than relying on foam alone. Manage condensate so it does not saturate the insulation or reach electronics. Include a drain/inspection method, and document whether the chamber can be cleaned without damaging the sensor/TEC mount. A stable chamber requires both thermal design and maintainable construction.

## 13. Glass observation and light exposure

The glass section is a useful experimental variable. It can simplify visual inspection without opening the chamber, but its heat transfer differs from EPS. Direct or diffuse sunlight can warm the panel and nearby air; the effect depends on orientation, glazing, shading, time and outside weather. A “daylight” label does not establish a beneficial light dose. In addition, a glass surface may be colder than the internal air in active cooling mode and become a condensation site.

Record whether the panel is exposed to sun, shaded, or illuminated by room light, plus measured temperature and RH. If a light sensor is installed later, document its response range and calibration. A cheap lux sensor generally measures a photometric quantity; that is not interchangeable with photosynthetically active radiation or UV irradiance. The firmware defaults to “NOT INSTALLED” for light data and must not show fabricated lux readings. If light exposure is not part of the experiment, keep it as controlled as possible rather than allowing an undocumented change.

The chamber's viewing panel should not be used to justify unspecified UV treatments. UV wavelength, irradiance, exposure time, shielding and eye/skin safety all matter. No UV device is assumed in the current design. The first experiment can quantify the glass panel's temperature effect with and without shading, then decide whether its light/thermal trade-off is useful.

## 14. ESP32 architecture

The ESP32 performs sensor acquisition, profile lookup, control decisions, safety interlocks, recovery-state updates, local networking and OLED updates. A small web server handles local API commands, but does not run the control loop on behalf of a browser. The implementation uses a repeated non-blocking loop and short `delay(1)` yielding; it avoids waiting for an external request before making a safety decision. The laptop dashboard is an observer/operator client and may be stopped without disabling control.

Configuration is held in a schema-versioned object: current phase, mode, safety latch, calibration offsets and six profiles. The storage subsystem serialises it into JSON and writes to alternating slots in ESP32 NVS. A checksum helps detect corruption, while a revision number helps select the newest valid slot. Semantic validation rejects inconsistent min/target/max values or values outside the allowed numeric ranges. The code reads back a written slot before confirming save success. It does not store every temperature in NVS.

The current firmware is a first engineering version and needs a real PlatformIO build on the intended board before deployment. The implementation also needs bench validation against the actual version of the Arduino-ESP32 core and the declared libraries. Firmware source review alone cannot prove timing, boot-state polarity, NVS power-failure behaviour or watchdog operation on the actual hardware. `BUILD_AND_VALIDATION_REPORT.md` records the checks executed and those not executed.

## 15. Sensor selection and calibration

A DHT22 is used for chamber air temperature and RH because it is a common inexpensive digital sensor and matches the existing plan. Its limitations include sample interval, response time, accuracy, humidity drift and vulnerability to condensation. It should be mounted in representative chamber air, not directly in atomizer spray, on a cold plate or in hot fan exhaust. The software checks for finite values and plausible ranges, but that catches obvious failures rather than proving calibration. Every reading has a timestamp and a validity flag; stale values are not treated as current.

The DS18B20 provides a temperature at its installed location. In this project, index 0 is assigned to the cooling-loop probe; it must not be reported as the saffron root temperature unless it actually is placed and validated there. A distinct hot-side probe is needed for the proposed Peltier interlock, and its ROM address/location must be mapped before configuring its index. A probe that reads a plausible value while physically detached is an undetected hazard unless its attachment is inspected or another diagnostic is used.

Calibration consists of comparison against a trusted reference across representative conditions, recording raw readings, uncertainty, correction and calibration date. A software offset should not be adjusted just until the displayed value matches expectation. Where a second logger is available, use it to measure gradients between the glass, centre, root zone, coolant loop and ambient. These records help determine whether one sensor placement is adequate or multiple measurements are required.

## 16. Peltier cooling principles

A thermoelectric cooler moves heat from its cold side to its hot side when supplied with electrical power. The hot side must reject the transferred heat plus the electrical energy that becomes heat in the assembly. A module labelled TEC1-12715 does not by itself define the capacity of the overall cooling system; actual performance depends on current, voltage, temperature difference, interface pressure, thermal compound, heat sink, radiator, water flow, air movement and ambient conditions. A second TEC adds a substantial load and may exceed the capability of the selected hot-side system.

The project includes two TEC1-12715 units and a water loop with pump, reservoir and radiator, but the exact topology and load current are not provided. The controller must not assume the drivers are independently controllable or that direct PWM is appropriate. Both module enable pins are commanded together as a single cooling request. The design uses guarded on/off operation, minimum timing and interlocks rather than a 0–100% slider that implies a linear response. If the actual driver supports a different strategy, it must be verified and documented before the control scheme changes.

The required acceptance test should measure actual supply voltage and current, coolant temperatures, hot-side temperature, chamber temperature, flow switch and electrical energy over time. The command log alone cannot tell whether the TEC received power or whether it cooled the chamber. The physical assembly must demonstrate stable behaviour under the expected ambient conditions before a crop is exposed. If the Peltier system cannot reach its intended band, the correct result is to record that capacity limitation and redesign—not to bypass the interlock.

## 17. Hot-side heat rejection and energy limitations

A common failure in small TEC projects is treating the cold side as the whole system. Thermoelectric modules are inefficient compared with many compressor cooling solutions for substantial temperature lifts, and the hot side must dissipate heat continuously. A small radiator or weak airflow can cause hot-side temperature to rise, reducing cooling capacity and risking damage. Two modules mounted to one thermal assembly also need good contact and balanced heat transfer. Insulation around the chamber cannot compensate for inadequate heat rejection.

The power supply's advertised total wattage is not proof that an individual rail, connector or wire can deliver the required current safely. A possible 650 W computer PSU must be evaluated by its output rails, current limits, protections, connector quality and distribution. Branch fuses, wire gauge, switched-driver current rating, cable routing and grounding must be matched to the actual load. Water and electrical components need separation, drip paths and a safe disconnect. These decisions require component data sheets and actual electrical inspection.

Energy cost should be measured, not guessed from controller demand. Use an appropriate power meter to calculate watt-hours or kWh over a defined period and record ambient temperature, target, runtime and cooling response. Compare energy against time in a justified band and against the biological outcome. A chamber that produces flowers but consumes impractical energy may be an interesting lab system yet still fail as a farm solution. No economic or energy-saving result is claimed before these measurements exist.

## 18. Humidity and misting

Humidity control and root-zone water delivery should be modelled separately. The DHT22 measures ambient air RH, which changes with temperature; it does not measure how wet a root is or whether an atomizer's droplets reach all root surfaces. A humidifier can raise RH in the chamber while leaving root coverage poor. A fine mist can also deposit water on cold walls and electronic components, create condensation and increase disease risk. A successful misting trial requires an actual assessment of distribution and drainage.

The provisional controller uses RH hysteresis to create a mist demand, a configurable pulse and rest period, maximum continuous runtime and a timeout for manual requests. It only allows the relay command when the RH value is valid and fresh, the driver is explicitly verified, and a reservoir-level signal confirms a safe level. If the reservoir switch is absent or faulty, it cannot infer reservoir safety from the fact that a pump/atomizer output was requested. When a sensor is invalid, affected output remains off.

The control law must be reconsidered if the mist maker becomes a dedicated aeroponic root delivery system. An air-RH threshold is not necessarily the right trigger for root hydration. A revised subsystem may need pulsed root delivery, a validated nozzle layout, a moisture/water presence check and drainage monitoring, while chamber RH remains an additional variable. pH, EC, nutrient concentration and root-zone moisture are not measured in the default build and must be displayed as unavailable rather than inferred.

## 19. Phase-aware automatic control

A phase-aware controller selects a profile based on the saved phase, then evaluates measurements against that profile. Temperature and RH hysteresis create separate activation and deactivation thresholds to reduce rapid relay toggling around a target. The target is a desired condition, the lower and upper bounds indicate allowed experimental limits, and the output request indicates only the controller's decision. These concepts should not be collapsed into a single dashboard number.

Phase-specific profiles support different experimental schedules without editing firmware every time. However, the profiles do not prove the biological phase. Manual phase changes are supported and persist only after the device confirms the save. Automatic phase advancement is intentionally absent; an arbitrary elapsed time is inadequate evidence that flowering, vegetative development or daughter-corm development has occurred. Observations, corm history and references should support stage transitions.

The control loop remains local when Wi-Fi, internet or the dashboard fails. It should not wait for an HTTP request, Python process, or browser polling cycle. If sensors go stale, outputs are forced to safe states. The controller also enforces subsystem-specific interlocks: cooling checks the loop/flow/hot side; misting checks the reservoir and RH sensor. The system must not turn on both outputs as a generic reaction to a restart or a missing reading. Actual output feedback, current sensing and redundant safety hardware remain future additions.

## 20. Persistent configuration and power-failure recovery

Persistence is the central feature. A common beginner implementation restores an `ACTIVE_PHASE` compile-time constant at startup or rewrites preferences every loop. Both approaches can produce silent phase regression or excessive flash writes. SAFFRON-AERO instead stores all committed profiles and critical settings in versioned data, writes only when a user commits a change or a rare safety latch changes, and keeps high-frequency sensor samples in RAM or a separate log. The storage routine writes an alternate slot and validates the saved content before accepting the new revision.

On boot, the controller turns load outputs off, loads persisted state, checks schema and ranges, identifies the newest valid slot and proceeds to fresh sensor acquisition. It refuses to silently replace corrupt state with a default phase. If an automatic mode was saved, the controller must not immediately switch on TECs: it waits for fresh plausible readings and runs configured cooling-loop and reservoir interlocks. If the previous mode was manual, it restores the user configuration but clears the command that could have held a load on. A persisted E-stop/fault latch remains until an intentional, validated clearance path is followed.

A full outage still has a physical consequence. Without power, the ESP32 cannot read the chamber, run the pump, operate fans or regulate temperature. It cannot know exactly when the power failed if no independent powered logger records it, and it cannot reconstruct the temperature drift during the gap. The reboot event can be logged after power returns, using boot-relative time; wall-clock time after a long outage requires a backup RTC or another trusted time source. Missing history must be shown as a gap, not filled with invented measurements.

## 21. Fully offline local operation

“Offline-first” means that sensing, local control decisions, safety checks and event generation happen on the ESP32 without a remote service. The device can create a local Wi-Fi access point so a laptop or phone can connect directly without a router or internet. If a station network is configured, the controller may also join it, but the access point and closed-loop logic should not be conditional on a successful Internet connection. Router failure must not reset a phase or setpoint.

Local networking introduces an access-control boundary. The prototype uses a shared API key on mutating requests and an AP password, both of which must be changed from sample values. These are appropriate only for a small trusted local prototype. Do not port-forward the ESP32 to the public Internet, commit credentials, or claim enterprise security. A hardened deployment would require a stronger identity model, secure update process, threat review and careful network segmentation.

The dashboard reads current state from the device, displays freshness and recovery status, then sends specific user actions. It does not own the control loop. Closing the dashboard should not stop regulation. Reconnecting the dashboard must refresh device state, not replay stale local values. In simulation mode, fake readings are explicitly marked and no physical hardware command is sent. Device mode must report an API/network error when the ESP32 is unavailable.

## 22. Interactive dashboard and API

The principal dashboard is a local Flask application with HTML, CSS and JavaScript and no external CDN dependency. It presents air temperature, RH, coolant temperature, phase, mode, configuration status, recovery state, current interlocks, command status, recent charts, event rows and experiment observations. Charts show the readings collected while the page is open; longer-term data are exported as CSV. The application is usable on a laptop connected to the device's local network, but it is not required for automatic control.

The API provides status and configuration retrieval, phase changes, mode changes, profile updates, timed manual cooling/mist requests, emergency-stop and fault-reset paths, diagnostics, event access and a first-time commissioning route. It validates inputs on both the dashboard and firmware. A settings write should be reported as saved only after the device confirms the commit. If a command fails, the dashboard must display the reason without claiming success. The device remains the source of truth for live state and committed configuration.

The interface explicitly distinguishes a commanded GPIO state from verified electrical state. It cannot know whether a Peltier draws current or whether a pump moves water unless appropriate feedback hardware is installed. It also shows unavailable optional metrics as “NOT INSTALLED” or “NO DATA” rather than a zero. This is especially important for light, pH, EC, nutrient concentration and root-zone delivery. A visible button is not a completed function unless it calls a real route and handles the result.

## 23. Optional weather-data integration

An external weather service could add local forecast information and help explore the relationship between outdoor conditions and chamber performance. It should be treated as a future feature, not a dependency. Weather forecast endpoints may require a network, API key, quota, and data attribution. A forecast can be stale, uncertain or unavailable; local control must continue with local measurements in those cases. The control firmware does not currently call any external API and does not require cloud credentials.

If added, the dashboard should show the provider, location, retrieval time, forecast issue time, validity interval, units and uncertainty metadata where available. It must distinguish external forecast values from measured chamber values, and log API failures as missing data. An advisory feature needs a specific farm decision, calibrated decision rule and evaluation plan; adding a weather icon alone is not an agricultural advisory system.

This extension should sit in the optional dashboard/service layer. The ESP32 can continue using saved conservative local profiles and safety interlocks when the weather source disappears. If the design later includes a cloud recommendation, the firmware should validate the received target against preconfigured safe bounds and require explicit operator approval unless an agronomically validated protocol clearly authorises an automatic adjustment. Internet connectivity should never be able to override the hardware emergency stop.

## 24. Data logging and experiment reproducibility

Reproducible research requires more than a live dashboard. Each trial should preserve firmware version, configuration revision, phase history, profile values, calibration records, sensor locations, corm batch, mass/count, external conditions, output events, electrical energy and crop observations. It should also preserve the exact changes made during the run and the reason for each intervention. Data should have explicit units, timestamps and source labels.

The firmware uses NVS for low-frequency committed configuration and LittleFS for event records. Live sensor values are not written to NVS every second because frequent flash writes are a poor logging strategy for this use. Host-side CSV logging is separate and runs only when the dashboard is active; it therefore cannot be relied on for autonomous data capture while the laptop is off. For longer unattended time series, add a bounded firmware logger or independent logger and define retention/capacity. The current event log records boot-relative milliseconds because an accurate wall clock may not be available after a long outage.

Gaps matter. If the chamber lost power overnight, the controller cannot report the in-chamber temperature during that interval. The dataset should represent the interval as unknown. Interpolated or model-derived values may be analysed separately only when clearly labelled; they must not be inserted into the raw dataset as if they were measured. A good report should preserve raw measurements, data-cleaning scripts and a separate analysis copy with every transformation documented.

## 25. Hardware and software safety

Safety is layered. A controller's interlocks are useful but cannot substitute for correct physical design, overcurrent protection, thermal cutoffs, wire ratings, insulation, grounding, enclosure integrity, an accessible disconnect and an appropriately designed emergency stop. The ESP32 must not drive a TEC directly. External drivers must match the load current, switching method, logic level and protection requirements. A displayed “OFF” can mean that the firmware command is low; it cannot prove that a failed MOSFET or stuck relay has physically disconnected power.

Firmware startup drives outputs to safe states before configuration restoration. The output paths are disabled by default until a set of explicit commissioning constants and safety inputs are verified. Missing flow and hot-side feedback lock the TEC path; a missing reservoir switch locks the mist path. Sensor failure, stale data, implausible readings and a persistent fault latch must prevent unsafe actuation. The emergency-stop endpoint latches software state and drives command pins low, but a separate physical power disconnect is needed for safety.

The current code's interlocks are conservative software gates, but they still require review and bench testing. Optional sensor inputs use expected polarities which must be verified; a miswired normally-closed switch may invert the safe/fault condition. Pin numbering, boot strap pins, active-high/active-low drivers, pull-downs and reset states also matter. Every real output should be tested with the load disconnected before actual load current is switched. Safety sign-off should be recorded in hardware documentation rather than implied by a checkbox in the UI.

## 26. Electrical protection

A possible 650 W computer power supply cannot be judged safe based on its total wattage alone. Different rails, connectors, current limits and protective behaviour matter. For each device, record the exact voltage, typical/maximum/stall current, fuse, wire gauge, connector, driver part number and protection requirement. Test the supply at the expected load and confirm temperature rise in connectors and conductors. Verify the grounding/reference arrangement for logic and power domains based on the actual driver design; do not casually tie isolated supplies together.

TEC current is large compared with sensor and ESP32 loads. Use a suitable switching driver and protective components, and check thermal interface and pressure. The cooling loop must be designed to reject the hot-side heat. Fans and pumps have start/stall currents; inductive loads may need flyback or surge suppression. Relay contacts must be rated for the actual DC load—AC ratings do not automatically translate into safe DC switching capacity. Keep wet lines and reservoirs away from exposed conductors and provide drainage and splash protection.

Any hardware diagram included in this package is conceptual until the component models are known. The pin map documents candidate GPIOs; it does not establish an as-built wiring topology. An accurate drawing should be prepared from the actual modules after inspection. The commissioning process should include power-off continuity checks, logic-level verification, output testing with loads disconnected, flow/thermal tests, and a record of measured current and temperature. No code change should weaken these checks for the sake of demonstrating an “ON” state.

## 27. Commissioning and calibration

Commissioning progresses in stages. First, validate the software build and run the dashboard simulator. Second, flash the ESP32 with high-current loads disconnected and verify startup outputs, sensor readings, API and phase persistence. Third, map sensor addresses, check the DHT22 against a reference, confirm the DS18B20 probe's physical placement and validate each optional switch. Fourth, use a meter or logic analyser to test driver inputs with load power disabled. Fifth, power each subsystem separately behind the required protections and inspect response to fault injection.

The first active thermal test should use no corms. Record ambient temperature, chamber centre/glass/loop measurements, hot-side temperature, flow state, fan/pump operation, voltage/current and energy. Check that the system can start, stop and recover from power loss without false sensor values or unexpected actuator activation. Observe condensation and the temperature rise in the hottest components. If the chamber cannot reach its target safely, stop and change the thermal design; do not bypass an interlock.

Calibration is not a one-time button. Sensor mounting can move, DHT22 response can drift, a DS18B20 can become detached, and a flow switch can change behaviour as tubing/reservoir arrangement changes. Maintain a dated calibration/inspection record and establish a check interval based on use. At each startup, the software should report freshness and validity; routine physical checks must confirm that those sensor readings still represent the intended measurement location.

## 28. Experimental methodology and controls

An experiment must distinguish intervention from observation. If the goal is to see whether active cooling changes the chamber environment, compare ambient and internal measurements under a specified target, window orientation and load. If the goal is a crop result, describe the treatment environment, corm batch, handling, moisture strategy, planting/root layout and other variables. Use a suitable control condition when possible. Allocate corms carefully so that initial mass and health do not differ systematically between groups. Small sample sizes can reveal practical problems but do not support broad commercial claims.

Define outcomes before the trial. Engineering outcomes may include time to reach a temperature band, duration within band, variation across positions, total kWh, peak hot-side temperature, condensation events, sensor uptime and recovery success. Crop outcomes may include flowering count, stigma dry mass, days to visible flowering, corm survival, leaf growth, disease and daughter-corm mass/count. Use consistent weighing and drying procedures, and record uncertainty. An outcome omitted from the dataset should not later be described as successful.

Keep a versioned profile and intervention log. If a target is changed after a sensor anomaly, note when and why. Do not discard a failed run merely because it looks unattractive; failures reveal limits. Keep raw CSV files read-only, transform a copy for analysis, and preserve the script. The experiment should not rely on the dashboard being open to retain the full history. For an unattended trial, firmware or independent logging must be sized and tested for the required duration.

## 29. Performance metrics and acceptance criteria

Software acceptance is concrete: the phase saved before reboot is the phase shown after reboot; profile values match; an invalid or corrupt slot is rejected; the older valid slot is used when possible; outputs remain off during sensor warm-up; stale measurements cause lockout; automatic mode is only declared recovered when configured interlocks pass; manual forced output does not resume after reboot; and the dashboard does not overwrite device state when it starts. These can be tested with a reference model and then confirmed on real hardware.

Hardware acceptance is separate. The verified flow switch must respond when the loop is not flowing and when it is flowing. The hot-side probe must be correctly located and the configured limit must be justified by the actual hardware. The driver must turn off under sensor faults, loss of flow, hot-side limit, reservoir low and physical stop. The power supply must operate within ratings and protective devices must be effective. Actual TEC current and cooling performance must be measured. A “recovery complete” message from a real controller can only mean that the firmware’s configured checks passed, not that every unmeasured mechanical condition is proven.

Agricultural acceptance requires a different evidence bar: a corm batch, reference group, known treatment, repeatable observations, measured output and sufficient duration to observe later corm development. Time in temperature band does not equal crop success. A single harvest does not establish daughter-corm health or future-cycle productivity. No project-specific numeric agricultural acceptance threshold is established here because validated aeroponic saffron outcomes for this chamber are not yet available.

## 30. Bill of materials and cost analysis

A bill of materials should list the ESP32 development board, DHT22, DS18B20 probes, I²C OLED, TEC modules, verified TEC driver, pump, tubing, radiator, hot-side heat sink/fan, mist unit, rated mist driver, reservoir-level/flow sensors, wiring, connectors, fuses, regulators, enclosure material, glass panel, thermal interface material and measurement instruments. The current CSV distinguishes known/planned parts from components whose model or rating is not confirmed. Do not invent unit prices for unknown components or imply that the existing computer PSU is a validated supply for this load.

A meaningful cost estimate includes consumables, failed prototypes, measuring equipment, electrical protection, thermal parts, replacement corms, enclosure cleaning and labour. A low-cost controller is not equivalent to a low-cost production system. The cooling subsystem can dominate cost and energy use; the physical protection and safety instrumentation can be more expensive than the microcontroller. If a required safety sensor is missing, the safe response is to budget and add it, not to treat the uninstrumented output as a free feature.

For any future scale-up, calculate annual energy use from measured kWh, local tariff, expected operating hours, maintenance, yield and crop loss. Avoid comparing a one-off chamber demonstration with field yield per hectare without normalising area, labour, planting material, crop cycle, output quality and capital costs. No return-on-investment estimate is made in this report because those inputs are not measured.

## 31. Energy measurement and running cost

The controller logs requested cooling, but it does not measure electrical power in the current build. A user interface may show a Peltier GPIO ON while the driver is disabled, a fuse is blown or a supply is current-limited. Energy use needs a suitable meter on the actual supply branch or a properly engineered current/voltage measurement circuit. Log average and peak current, voltage, power factor where relevant, and elapsed operation under a defined ambient load. Do not infer wattage as “percent slider × rated wattage” unless the exact driver/metering design justifies that calculation.

A fair test should capture the full thermal cycle: startup, steady period, shutdown, pump/fan cooldown and any restart. Record outside temperature, target, chamber temperature at multiple positions, coolant/hot-side values, flow state and visible condensation. Calculate kWh by integration or meter reading and report the method. Compare runs at similar ambient conditions or explicitly model the difference; otherwise a cooler day can make the device appear more efficient without any design change.

Energy data is relevant to the agricultural problem because climate-control costs can outweigh a small crop's value. A commercially useful system must keep the target safely while using acceptable electricity and labour. If the result shows poor coefficient of performance or large heat gain through the viewing panel, reducing the load through shading, better insulation or improved airflow may be more effective than adding larger TECs. Such changes should be tested and recorded rather than assumed.

## 32. Scaling and potential farmer applications

A prototype that controls a small enclosure is not automatically scalable to a 10 ft × 10 ft × 10 ft chamber or a farm. Heat transfer, air volume, leakage, load power, flow distribution, thermal gradients and control response all change with scale. TECs are especially constrained by heat rejection and current. A larger system may need a different cooling technology, multiple independently monitored zones, commercial environmental controls and professionally engineered electrical protection. Scaling by copying the same two TEC modules is not a valid plan.

Potential use cases include research on corm treatment, protected nursery work, educational demonstrations, and collecting environmental data in a small controlled space. A grower-facing deployment would require dependable hardware, serviceability, calibration, remote diagnostics where appropriate, long-term support, safe operation during outages, proven agronomic guidance and evidence of cost-effectiveness. The project could later connect to weather advisories, but that remains a different system component with its own data quality and user requirements.

The correct scale-up milestone is not “it fits a bigger box.” It is a set of measured relationships: thermal load versus ambient temperature, energy per period, control stability, root-zone delivery effectiveness, disease risk, corm outcomes and cost. Only after these are measured can the team select a different cooling plant, enclosure and instrumentation budget. Commercial claims should follow trials, not precede them.

## 33. Climate-resilience applications

Climate resilience includes the capacity to anticipate, absorb, respond to and recover from environmental stress. Controlled cultivation can reduce selected exposures at the production unit, but it does not remove water availability, energy access, market, disease, supply-chain or extreme-event risks. A climate-controlled chamber dependent on a fragile power supply can create a new vulnerability. Its contribution therefore needs to be stated narrowly: it may protect a limited crop batch during powered operation if the target is agronomically useful and the hardware is effective.

The offline-first design supports resilience at the controller/network level. Lack of internet should not stop local control, and persisted state should allow recovery after a restart. However, a total electrical outage can stop cooling and misting; temperature drift during the outage remains unknown unless an independent powered logger or backup system is fitted. A future UPS must be sized for measured load and runtime, with battery safety and transfer behaviour tested. A backup that powers only the ESP32 but not the TECs does not maintain temperature.

If climate-resilience claims are evaluated, define the stress event, exposure window, control condition and crop outcome. For example, compare chamber and reference temperature histories during an external heat episode, then track the crop response. A lower sensor reading is an engineering result, while reduced crop damage is a biological result. A farmer benefit also requires a cost and adoption assessment. The report must not collapse these different evidence levels into one statement that the system “solves climate change.”

## 34. Potential social, economic and environmental benefits

Potential benefits include making controlled agriculture more accessible as an educational prototype, enabling repeatable small-batch observations, reducing dependence on cloud systems for basic control, and exposing how environmental variables relate to plant outcomes. The local API, CSV logs and research documentation can lower the barrier for students to test hypotheses and share reproducible methods. The safe recovery approach is valuable beyond saffron because many embedded control devices need to preserve a configuration without automatically re-energising unsafe outputs.

Economic benefit is not yet demonstrated. Saffron has high unit value but small crop scale, manual labour, corm acquisition costs, energy cost, quality requirements and uncertain yield. A chamber with high electricity consumption or only occasional flowering may not be viable for a grower. A realistic cost-benefit assessment must include capital replacement, measured kWh, labour, corm multiplication, disease losses and the value of quality-assessed spice—not just the retail price per gram.

Environmental impact also needs measurement. Water use, power consumption, plastic and electronic waste, cleaning chemicals, battery disposal, coolant leakage and material durability are relevant. Aeroponics may reduce some water use in some systems, but no water-savings claim is justified until total input and losses are measured against a defined comparison. The goal is to make such comparisons possible, not to assert them before the experiment.

## 35. Technical and scientific limitations

Technical limits include the unknown TEC-driver topology, current supply, flow and hot-side instrumentation, actual sensor locations, temperature uniformity, glass thermal properties and chamber leakage. The default code reports GPIO commands but has no current feedback. It cannot know whether a pump impeller is moving water unless a valid flow signal is present. A disconnected hot-side sensor or wrong DS18B20 index can create a plausible-looking but incorrect result. These are not minor implementation details: they directly determine whether the control state is safe.

Scientific limits include the transfer of soil-based and greenhouse saffron research to an aeroponic arrangement, the unknown corm batch history, the absence of pH/EC/root-zone moisture measurement, and the lack of a suitable reference treatment. A setpoint that is comfortable for a dashboard plot may be biologically ineffective. Temperature and RH data alone cannot explain disease, resource allocation or chemical quality. Flower count and appearance do not establish the concentration of saffron's key compounds.

Software limits include a local prototype API and shared-key authentication, no RTC-backed wall-clock guarantee, a bounded event log, a simple simulation rather than a calibrated thermal model, and no production-grade update/security architecture. Firmware compile and hardware checks remain to be completed. The correct response to these limitations is to label them and prioritise the highest-risk measurements before crop claims.

## 36. Commercial-scale risks

Commercial deployment adds requirements for safe electrical and thermal design, robustness, maintenance, calibration, data retention, service access, operator training, replaceable parts, compliance with relevant rules and evidence under variable environmental conditions. A prototype can be locally useful without meeting these requirements, but it should not be sold or presented as a finished farm appliance. The two TEC modules could fail to meet cooling requirements in high ambient temperatures; a more suitable compressor or other heat-pumping system may be needed at larger scale.

An economic model must also account for the fact that saffron corms have multiple years of biological value. Optimising for one harvest may reduce corm health or daughter-corm production. A high planting density may increase stigma output per area yet reduce individual corm growth; the 2025 controlled-greenhouse study demonstrates that trade-off under its own conditions. Labour for flower picking, stigma separation and drying remains significant and cannot be removed by climate monitoring alone.

Reliability risk spans electronics, water, firmware and biological material. A sensor fault could disable cooling; a stuck driver could remain on despite a low GPIO command; a misting failure could either dry the root zone or cause excess moisture; an outage can interrupt the climate. Future commercial design should include independent thermal cutoffs, tested fault handling, a physical disconnect, a suitable backup strategy, service logs, component availability and a documented agronomic protocol. None of these should be assumed from a successful demo.

## 37. Future development

The near-term priority is commissioning and measurement. Identify exact driver parts, map sensor ROM addresses, connect and test verified flow and reservoir switches, and determine whether the cooling system can safely reject heat. Add independent hot-side protection if supported by the physical design, then verify all output states under reset and injected faults. Instrument electrical power. Collect empty-chamber data before adding corms. This addresses the largest risks before investing in a larger enclosure or complicated dashboard features.

The second tier is research instrumentation: a backup RTC or independent logger, additional air probes for gradients, a calibrated light sensor if light matters, root-zone delivery checks, and pH/EC instruments only if a nutrient-solution experiment is designed. A true aeroponic implementation would need a separately designed root enclosure, verified mist coverage, drainage and sanitation. Biological logging should include corm-level initial mass and later daughter-corm outcomes.

The third tier is scale and accessibility: improve dashboard authentication, refine event-log retention, introduce an OTA update process with safe rollback, add exportable complete configuration, present uncertainty/evidence classifications in the profile editor, and test operation on phones and low-bandwidth local networks. Weather/advisory integration can be an optional module after selecting a trusted provider and farmer decision. These features should not take priority over thermal safety or valid crop measurement.

## 38. Implementation roadmap

**Stage 1 — Software simulation.** Run Python tests and the simulator, confirm dashboard actions and exports, and review the phase/profile model. No hardware claims are made at this stage.

**Stage 2 — Sensor-only firmware.** Build and flash firmware with all load outputs disconnected. Validate the DHT22 and DS18B20, test no-data and stale-data paths, verify NVS save/reboot behaviour, inspect AP access, and check event log persistence. This stage validates software on the intended board, but not high-current circuits.

**Stage 3 — Electrical commissioning.** Confirm the actual TEC driver, pump/fan drivers, supply rails, fuses and wiring; install and test flow/hot-side/reservoir feedback; validate the physical stop. Run driver inputs with load power disconnected, then test loads one at a time under supervision. All high-current outputs remain locked until this stage passes.

**Stage 4 — Empty-chamber engineering validation.** Measure internal gradients, heat gain through glass, cooling capacity, response time, power and condensation with no corms present. Inject sensor faults and restart events. Do not accept “recovery complete” as proof of thermal capability.

**Stage 5 — Small biological study.** Record corm batch, treatment and reference conditions; define outcomes before starting; maintain measured temperature/RH and crop logs; capture anomalies and losses. Use a repeatable harvest/drying protocol and follow the corms through the vegetative and daughter-corm phase.

**Stage 6 — Review and scale decision.** Compare energy, labour, safety, crop results and cost against an actual reference. Continue only if the evidence supports a clear next experiment. A failed hypothesis is a valid result; a larger chamber should not be built only because the current interface looks polished.

## 39. Conclusion

SAFFRON-AERO is a project for engineering and studying a small protected saffron environment. Its defensible contribution is the combination of a local autonomous controller, persistent phase-aware configuration, safe recovery logic, visible sensor validity, optional local dashboard and experiment-oriented documentation. By design, its saved configuration is independent of the cloud and laptop. It can restore the phase after power returns, but it cannot keep the chamber cool while unpowered.

The project's strongest risk-control decision is to separate “what the operator saved” from “what the hardware may safely energise.” A valid phase is restored even if a sensor is invalid, but the outputs remain disabled. A manual command is not replayed after reboot. An absent flow or reservoir signal blocks the related output. A corrupted configuration enters recovery rather than silently selecting a default. These behaviours can be tested and provide reusable lessons for other embedded systems.

The crop hypothesis remains open. Published saffron studies justify examining thermal history, controlled environment and corm trade-offs, but they do not validate the exact aeroponic chamber, current targets, water delivery or expected commercial yield. To make an agricultural claim, the team must verify the real hardware, measure the chamber, conduct a controlled crop trial and report the result honestly. A functioning dashboard is an engineering milestone, not proof of successful saffron production.

## 40. Bibliography and appendices

The bibliography and detailed phase notes are maintained in `research_and_references.md`, `saffron_growth_environment.md` and `references.bib`. The following entries are the core sources used in this report:

1. Wang, Z., Li, X., Xu, J., Yang, Z., & Zhang, Y. (2021). Effects of ambient temperature on flower initiation and flowering in saffron (*Crocus sativus* L.). *Scientia Horticulturae*, 279, 109859. https://doi.org/10.1016/j.scienta.2020.109859
2. Jose-Santhi, J., Sheikh, F. R., Kalia, D., & Singh, R. K. (2023). Sugar metabolism mediates temperature-dependent flowering induction in saffron (*Crocus sativus* L.). *Environmental and Experimental Botany*, 206, 105150. https://doi.org/10.1016/j.envexpbot.2022.105150
3. Askari-Khorasgani, O., & Pessarakli, M. (2019). Shifting saffron culture from traditional farmland to controlled environment (greenhouse) condition. *Journal of Plant Nutrition*, 42(19), 2642–2665. https://doi.org/10.1080/01904167.2019.1659348
4. Cardone, L., Castronuovo, D., Perniola, M., Cicco, N., & Candido, V. (2019). Evaluation of corm origin and climatic conditions on saffron yield and quality. *Journal of the Science of Food and Agriculture*, 99(13), 5858–5869. https://doi.org/10.1002/jsfa.9860
5. Pirasteh-Anosheh, H., et al. (2023). Climate and management factors influence saffron yield in different environments. *Agrosystems, Geosciences & Environment*, 6(3), e20418. https://doi.org/10.1002/agg2.20418
6. Qaryouti, M., Al-Soqeer, A., Abdelaziz, M. E., Gruda, N. S., AlSahly, S., Alrasheed, W., Althobiti, S., Babiker, O., Sharafeldin, M., & Voogt, W. (2025). Cutting-edge greenhouse practices for better stigma yield and corm quality of saffron. *Frontiers in Plant Science*, 16, 1612791. https://doi.org/10.3389/fpls.2025.1612791
7. Acharya, B. S., Ghalehgolabbehbahani, A., Hamido, S., Zinati, G., Bozzolo, A., Archer, L., Wendelberger, K., Das, S., Thapa, R., & Panday, D. (2025). Saffron—the golden spice: management, challenges and opportunities for sustainable production in the United States. *Journal of Agriculture and Food Research*, 21, 101970. https://doi.org/10.1016/j.jafr.2025.101970
8. Espressif Systems. Preferences — Arduino ESP32 documentation. https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html
9. Pallets Projects. Flask installation documentation. https://flask.palletsprojects.com/en/stable/installation/

### Appendix A — Example commissioning checklist

| Check | Evidence to retain | Pass condition |
|---|---|---|
| GPIO boot safe | Scope/meter/logic capture across reset and power cycle | Every load-driver input stays in verified OFF state before firmware permits actuation |
| DHT22 validity | Raw serial readings and reference comparison | Plausible fresh temperature/RH and known measurement location |
| DS18B20 identity | ROM-address map and attachment photo | Coolant-loop and hot-side probes uniquely identified; readings physically plausible |
| NVS recovery | Saved phase/profile file or serial/API revision before/after reboot | Latest valid revision restores exactly; no default overwrites it |
| Interrupted write | Fault-injection record or simulated slot-write failure | Previous valid slot remains readable and selected |
| Flow interlock | Bench recording with pump stopped/running and switch state | TEC command cannot energise without confirmed flow |
| Hot-side interlock | Sensor response and threshold test | TEC command is removed at sensor fault or configured limit |
| Reservoir interlock | Level switch state test | Mist command stays off when reservoir safety signal is absent/unsafe |
| E-stop | Physical circuit and firmware response | Hazardous output becomes de-energised and latch survives reboot |
| Power outage semantics | Independent logger trace, if fitted | Data gap is shown honestly; no invented readings fill it |

### Appendix B — Example data dictionary

| Field | Unit / type | Interpretation |
|---|---|---|
| `timestamp_utc` | ISO-8601 string or unavailable | Host timestamp for dashboard log; not equivalent to the controller's boot-relative time |
| `boot_relative_ms` | ms since boot | Useful for ordering events in one boot; resets after restart |
| `source` | `ESP32` or `SIMULATED` | Must be preserved through export and analysis |
| `phase_id` / `phase` | integer / text | Saved treatment label, not independent proof of plant biology |
| `air_temperature_c` | °C | DHT22 air measurement when valid |
| `relative_humidity_pct` | % RH | DHT22 measurement when valid |
| `coolant_temperature_c` | °C | DS18B20 at the verified cooling-loop probe location |
| `cooling_demand` | Boolean | Logical controller request, not actual power |
| `cooling_commanded` | Boolean | Reported GPIO/driver command, not measured current |
| `sensor_data_fresh` | Boolean | Freshness status under software timeout rules |
| `recovery_stage` | text | Named boot/recovery state at the sample time |
| `lockout_reason` | text | Human-readable reason for denied/blocked control |
| `flower_count` | count | Counted flowers during a defined observation interval |
| `harvest_mass_g` | g | Actual measured harvest mass, with drying protocol documented separately |
| `daughter_corm_count` | count | Count of recovered daughter corms at the specified harvest/cycle endpoint |

### Appendix C — Example test cases

**Test 1: saved phase recovery.** Save phase Floral Induction with unique profile values and revision R. Restart a fresh controller instance. Confirm that phase and all profile values match R, that the new instance does not use a compile-time default, and that output states remain OFF until fresh sensors and safety checks pass.

**Test 2: stale temperature.** Feed a valid temperature, advance monotonic time past the freshness window without a new sample, and request cooling. Expected result: the temperature remains labelled stale; the requested output is blocked; status contains a specific lockout reason; no stale value is displayed as current.

**Test 3: interrupted persistence.** Save revision R in one slot, begin revision R+1 in the alternate slot, then simulate a power cut between payload, checksum and revision writes. On restart, R must still be accepted if its slot is intact. The corrupt/incomplete slot must not be selected solely because it has a larger stale revision value.

**Test 4: manual command non-replay.** Enter manual mode, issue a short forced-on request in the live test harness, then create a fresh controller after a simulated restart. Configuration and phase return, but the manual actuator demand is false and the output remains OFF until a new timed command passes interlocks.

**Test 5: driver missing.** Leave `PELTIER_DRIVER_VERIFIED` false or the flow pin at `-1`. Enable automatic mode and set a cooling demand. Expected result: the demand may be recorded, but the TEC driver command remains OFF and the lockout explains which hardware prerequisite is missing.

**Test 6: dashboard disconnect.** Run the firmware control loop, close the Flask app or disconnect the client, and observe sensor updates and the output policy independently. After reconnect, the dashboard must fetch controller state rather than restoring browser defaults. Repeat with no internet access.

### Appendix D — Decision record

- **Local controller:** selected so core control persists without a remote service.
- **NVS persistence:** selected for small, low-frequency configuration rather than live time-series writes.
- **Dual-slot + CRC + revision:** selected to detect accidental corruption and tolerate some interrupted writes. It is not a secure/cryptographic store.
- **LittleFS event log:** selected for modest event history; an external logger is preferred for long, high-frequency measurement datasets.
- **AP fallback:** selected to allow direct local access when no router is available. Credentials must be changed and the device must not be exposed to public networks.
- **No automatic crop-phase advancement:** selected because elapsed time alone does not prove biological development.
- **No automatic replay of manual loads:** selected because a manual ON condition can be stale/unsafe after restart.
- **Physical outputs disabled by default:** selected because driver topology and critical feedback inputs are not confirmed.

### Appendix E — Open questions for the next engineering review

1. What exact ESP32 board/variant and boot behaviour are in the physical build?
2. What exact TEC driver is installed, and what interface/polarity/frequency does it support?
3. Are the two TECs electrically independent or one shared driver path?
4. What are the measured startup and steady currents for TEC, pump, fan and atomizer?
5. What model of PSU, which rail(s), and what fuses/connectors/wire gauges are used?
6. Where is each DS18B20 physically attached, and what ROM address identifies it?
7. Is there a real flow switch and reservoir-low switch? What are their safe and fault signal states?
8. What hardware will independently stop the TEC on overtemperature or unsafe current?
9. Is the ultrasonic mist maker intended to humidify chamber air or deliver controlled mist to roots?
10. What is the actual root-zone geometry, drainage path and cleaning procedure?
11. What external agronomic protocol is suitable for the batch's storage and flowering history?
12. Which corm observations will determine stage changes, and what reference group will be used?
13. How will electrical energy and outage-period temperature be measured independently?
14. What evidence would justify scale-up, and what result would terminate the current hypothesis?

**Report integrity note:** this report is a research and engineering design document. It contains no claim that the physical device has been commissioned, that the TECs have been run safely, or that saffron yield has been measured. Actual word count is recorded in `BUILD_AND_VALIDATION_REPORT.md` after packaging validation.

### Appendix F — Detailed experiment protocol and evidence capture

The first cultivation run should be treated as a feasibility study, not as a production trial. Its primary outputs are an auditable record of chamber performance, corm survival and observed phenology, not a claimed improvement in commercial yield. Define the question before powering the plant: for example, “Can the existing insulated chamber hold a measured air-temperature band during a specified ambient-temperature window without unsafe hot-side conditions?” That question can be answered independently of whether any corm flowers. A second question might be, “What visible developmental events occur in this corm batch under the recorded storage and flowering conditions?” The two questions need separate acceptance criteria.

Before the trial, give the batch a stable identifier such as `BATCH-2026-A`. Record supplier or source if known, receipt date, corm count, individual mass or a clearly described representative sample, visible damage, firmness, signs of mould, and storage history provided by the supplier. Do not invent a cultivar purity statement. Crocus sativus is sterile and is multiplied vegetatively, but commercial corm material may still differ in size, health, prior storage conditions and physiological state. These are potential confounders. A photograph scale and date card can improve comparison, provided the pictures are stored with the corresponding observation record.

Write down the enclosure configuration before placing corms: approximate external dimensions, insulation thickness and condition, panel dimensions, seal gaps, sensor locations, fan direction, radiator location, water-loop configuration, mist-maker model and mounting, and all driver/supply identifiers. Draw a simple top-view and side-view layout. Label which dimensions are measured and which are estimated. Mark electrical devices close to the water path. If an optional sensor is not installed, record `NOT INSTALLED`; do not enter zero. If a probe is relocated, create a new configuration note rather than making the historical series appear spatially continuous.

A run should begin with a dry commissioning interval with no corms. Use a reference thermometer and an independent logging instrument if available. Observe chamber pull-down and recovery, air gradients at multiple positions, coolant temperatures, hot-side temperatures, and humidity response under the actual expected ambient conditions. Run only the load combinations that have passed electrical inspection. A high-current output should be tested with the crop absent and with a technician able to disconnect power. This interval reveals whether the enclosure and cooling loop are capable of the intended environmental band before plant material is risked.

For the biological run, specify what “misting” actually means. The present prototype has an ultrasonic atomizer proposal; it does not automatically implement a validated commercial aeroponic nozzle system. If roots are inside a container, document whether droplets directly contact them, whether water pools, how the root chamber drains, and how long each wetted surface remains wet. Record reservoir water source and any additives. Unless the solution composition, pH and EC are measured by suitable instruments, report them as unmeasured. Do not claim an agronomic nutrient recipe on the basis of air temperature and relative humidity alone.

Use a daily observation sheet at a consistent time where practical. Record corm appearance, visible sprout length, first flower-bud observation, first open flower, daily flower count, stigma harvest if actually performed, visible mould, root condition if inspected, and interventions. Record the observation time and whether the chamber was opened. Opening the chamber changes temperature and humidity and may change light exposure; the action is part of the experimental record. The unit for flower count is count per batch per observation interval, not “yield” unless the study explicitly defines and measures yield. If stigma mass is measured, record balance resolution, tare procedure, moisture state and whether the mass is fresh or dried. Post-harvest drying and storage can affect measured mass and quality, so preserve those details.

If comparing configurations, change one planned factor at a time or use a design that can separate effects. For example, a glass shading test can compare shaded and unshaded periods, but season, ambient temperature and day length can confound a simple before-and-after comparison. A more credible comparison repeats periods and records outside conditions. A two-chamber comparison needs similar starting corm material, random allocation where practical, equivalent sensor calibration and independent measurement. One chamber and one corm batch cannot establish a general treatment effect. Report the number of units, not merely the number of sensor samples; thousands of measurements from one chamber are not thousands of independent biological replicates.

Set stopping rules in advance. Examples include a manufacturer-specified hot-side limit, loss of essential coolant flow, electrical leakage, persistent condensation on electrical items, repeated sensor-invalid status, or visible serious deterioration of the corms. Setpoint deviations should have defined response actions rather than prompting improvised changes while the trial is underway. A stop condition should not depend on having internet or the dashboard available. The local controller must place outputs in its designed safe state and keep diagnostics available when possible.

At the end, archive the exact firmware version, configuration revision, profile values, calibration offsets, wiring map, sensor serial/address map, ambient conditions, data exports and observation sheet. Compute simple summaries only from data with declared quality rules. Show missing intervals on plots. If some hours are missing during a power cut, do not interpolate them for a control-performance claim unless a clearly labelled analysis method is justified; the primary record should preserve the gap. Report unsuccessful runs as well as successful ones. Failed cooling, absent flowers or corm losses are informative outcomes and should not be omitted because they weaken a demonstration.

### Appendix G — Preliminary failure-mode and effects review

The following table is a design review aid. It is not a completed formal hazard analysis and not proof that any mitigation works in the physical installation. Each line needs a bench test and an owner before the system is operated unattended.

| Failure mode | Possible consequence | Detection or evidence | Required initial response | Residual concern |
|---|---|---|---|---|
| DHT22 absent, stale or implausible | Incorrect cooling/humidity decisions | Sensor validity flag, sample age, plausibility rules | Keep affected loads off; expose sensor fault | A plausible but biased sensor may evade simple checks |
| DS18B20 disconnected | Coolant estimate unavailable | DallasTemperature disconnected sentinel and stale-time check | Block Peltier start; report location-specific fault | Probe may be connected electrically but physically misplaced |
| Hot-side probe absent or at wrong location | Overheating remains undetected | Device ROM/address mapping and commissioning record | Do not commission TEC output | Software cannot infer an unmeasured hot-side temperature |
| Pump commanded but no flow | TEC heat rejection may fail | Verified flow switch or calibrated independent proof | Shut TEC off; keep pump/fan sequencing according to reviewed design | A switch can fail or be installed where it misses a blockage |
| Radiator fan failure | Reduced heat rejection | Tachometer or independent inspection; thermal trend if measured | Stop TEC and latch a fault | Without fan feedback, an early failure may be detected late |
| Mist reservoir empty | Atomizer damage or no root moisture delivery | Actual level sensor, if fitted | Lock out misting; label missing hardware feedback | Level alone does not prove mist generation or root coverage |
| Driver stuck on after ESP32 command goes low | Continued high-current output | Independent current/voltage feedback or physical observation | Use appropriately rated hardware disconnect and fuse; do not rely solely on software | GPIO state cannot stop a shorted power switch |
| Brownout or abrupt power cut | Environmental drift and restart uncertainty | Restart event; RTC/logger if present | Restore configuration but validate sensors/interlocks before resume | The unpowered interval is not observed by an unpowered ESP32 |
| Corrupted persisted configuration | Wrong phase or unsafe setpoints | Schema, checksum, range validation, redundant slots | Configuration-recovery state; outputs locked | Dual slots do not protect against every storage or software defect |
| Interrupted NVS write | Partial new configuration | Read-back and revision/checksum validity | Retain prior validated slot | Must be tested on intended firmware/core and physical NVS |
| Dashboard sends stale settings | Phase/setpoints revert unexpectedly | ESP32-authoritative read, revision on committed response | Reject invalid writes; do not push defaults on page load | A future client could still race edits unless version checks are added |
| Wi-Fi disconnect | Remote viewing/control unavailable | Network status and client errors | Continue local loop; fallback AP if configured | Local clients cannot interact until network access returns |
| API key is left at example value | Unauthorised local commands possible | Startup warning and installation checklist | Replace key before commissioning | Basic shared-key HTTP is not robust defense on an untrusted network |
| Condensation reaches electronics | Corrosion, short circuit or shock hazard | Visual inspection, humidity logs, leakage protection | De-energise affected equipment and correct drainage/separation | Foam and glass joints can trap moisture invisibly |
| Corm rot or infection | Lost plant material and misleading results | Visual observations, batch records, sanitation review | Isolate affected material and stop/adjust experiment appropriately | Sensor data alone cannot diagnose plant disease |

The safety hierarchy should prefer physical prevention, properly rated electrical protection, independent interlocks, firmware lockout, operator procedures and finally dashboard warnings—in that order. A warning in a browser is not a safety circuit. A software flag saying “flow okay” is meaningful only if it is grounded in a fitted, tested sensor. The firmware's default uncommissioned state is intentionally restrictive because several essential feedback devices are currently optional or absent.

### Appendix H — Control logic, calculations and commissioning acceptance

A two-position controller with hysteresis is a reasonable transparent starting point for this small prototype, provided the physical cooling system and sensor placement have been validated. For an illustrative air target \(T_s\) and hysteresis half-width \(h\), a simple cooling controller may request cooling when the measured temperature rises above \(T_s+h\), then stop requesting it after the measurement falls below \(T_s-h\). It must also enforce a minimum off-time and minimum on-time where those rules are compatible with the emergency stop and safety interlocks. Safety faults override the normal minimum-on timer; they must not keep a heater/cooler powered just to satisfy a timer. This example describes logic, not a universal agronomic optimum.

For a humidity control loop, choose the action based on what physical actuator is actually fitted. If an atomizer wets the air, that may change chamber RH without reliably wetting the root system. If it is deployed as a root-zone mist generator, its duty cycle should be evaluated through direct observation of coverage and drainage, not inferred only from a DHT22. A controller must not use one measurement to claim two distinct controlled variables are both satisfactory. RH and root moisture delivery are separate experimental quantities.

Sensor freshness must be an explicit condition. A plausible numerical reading is not necessarily current. Each measurement should carry a value, valid flag and sample time; the controller compares the current monotonic time to that timestamp. If the DHT22 hasn't refreshed within the configured stale interval, it should become invalid for output decisions even when the last value looks reasonable. Since the DS18B20 can be read asynchronously, the code must request a conversion, wait for conversion completion using elapsed time, and only then accept a new result. It should not block the web server or watchdog while a sensor converts.

Configuration persistence should occur when an operator commits a valid profile, changes phase, or changes a safety-relevant setting—not on every loop cycle. The intended transaction is: receive command, validate types and ranges, make a candidate copy, serialise it, write to the inactive slot, verify payload/checksum/revision by reading back, and only then expose it as committed. If verification fails, keep the former valid configuration authoritative and return an error. Any schema migration must specify how missing fields are filled and which migrations are backwards-compatible. A migration should not silently convert a missing phase to a default active phase.

Example energy arithmetic should be transparent. If a plug-in power meter reports an average measured draw of \(P\) watts for \(t\) hours, estimated energy is \(E = P \times t / 1000\) kWh. At an electricity tariff \(C\) currency units per kWh, the energy charge is \(E \times C\). Average power needs actual measurement across representative cycling; the sum of rated module powers is not the same as measured average consumption, and the power supply's maximum wattage is not actual draw. Include pumps, fans, controller and any losses inside the measured boundary. State exactly what the meter included.

A candidate commissioning acceptance suite is:

1. With loads disconnected, boot repeatedly and verify every output pin remains at the documented safe electrical state before and during configuration loading.
2. Save a non-default phase and distinctive setpoint values, remove power, restore it, and confirm the values and revision are recovered exactly.
3. Interrupt configuration update attempts in a controlled software test and verify that the prior valid slot remains loadable.
4. Disconnect each sensor in turn and demonstrate that stale or invalid data prevents the affected automatic output from starting.
5. Force the configured emergency stop and demonstrate that restart alone does not clear the fault latch.
6. Deny the flow proof and confirm that TEC control remains disabled; do not emulate a successful flow switch by software alone for the final acceptance test.
7. Stop the router, then stop the laptop dashboard, and independently verify the local control loop continues.
8. Measure driver input and load current using suitable instruments during controlled tests; compare commanded state with actual electrical behaviour.
9. Operate at representative ambient conditions with no corms, log the entire thermal response, and inspect hot-side temperature and condensation.
10. Only after these tests pass, run a small supervised biological trial and preserve its protocol and raw observations.

For each test, record identifier, requirement, preconditions, setup, procedure, expected behaviour, observed behaviour, evidence file, pass/fail result, tester and date. Use “not run” rather than “pass” where a tool or physical setup was unavailable. A simulation pass means only that the tested software model exhibited the expected behaviour under test conditions. It does not demonstrate that a relay contact, MOSFET, pump, sensor, seal, power supply or corm behaves as expected in real conditions.

### Appendix I — Evidence grades and claims register

A useful project record separates four evidence classes. **Published evidence** is a result stated in a research paper or authoritative technical documentation. The record must identify its context and link. **Implemented software behaviour** is behaviour visible in source code and verified by automated tests. **Bench-verified hardware behaviour** requires actual equipment, a reproducible procedure and recorded observations. **Biological outcome** requires a declared cultivation protocol and observations of actual plant material. These categories are not interchangeable. A high-quality paper does not validate an untested chamber; a good software unit test does not show that saffron will flower; and one successful flower does not validate a commercial yield estimate.

For every claim in a pitch deck or report, maintain a short claim entry: exact claim, evidence class, supporting record, uncertainty, scope and whether the statement can be made publicly. “The prototype is designed to recover the saved phase after reboot” is a design claim. “The host-side model passed the saved-phase test” is a software test claim. “The ESP32 physically resumed automatic cooling after power loss” is a hardware claim and must wait for board-level test evidence. “The system increased saffron yield” is a biological comparative claim requiring proper controls and repeated evidence.

A practical evidence scale for project management is: E0, idea or assumption; E1, supported by source inspection or calculation; E2, covered by automated test; E3, demonstrated on a bench; E4, demonstrated in a documented biological experiment; E5, replicated in a broader independent or production-relevant setting. This scale is an internal management tool, not a published scientific standard. The project must not label an item E3 or higher without a dated artifact describing the real setup and results. When the project is judged by a hackathon or startup panel, this distinction is an advantage: it makes the remaining work visible and credible rather than masking it with broad “AI-powered” or “smart agriculture” claims.

### Appendix J — Practical scale-up gates

Scaling this system from a small chamber to a room or farm structure requires a new thermal and agronomic design, not simply larger Peltiers or more ESP32 boards. Heat load changes with wall area, insulation, infiltration, solar gains, equipment, crop transpiration, desired temperature difference and ambient extremes. A small chamber can be dominated by leakage and a single glass panel; a room can have vertical stratification, multiple local microclimates, higher door-opening load and maintenance access requirements. The cooling equipment should be sized from a documented heat-load estimate and verified operating envelope. Thermoelectric modules are not automatically a suitable technology for a 10 ft × 10 ft × 10 ft chamber; compressor-based refrigeration may be more practical, but that choice requires engineering and cost evaluation.

The distribution problem also changes at scale. One DHT22 in a corner cannot characterise a large plant room. Multiple calibrated air sensors, root-zone measurements, flow/pressure monitoring, leakage detection, redundancy, local alarms and a maintenance plan may become necessary. A local controller can coordinate devices without cloud access, but a multi-zone system needs a carefully defined loss-of-communications and authority model. A single central dashboard must not be allowed to be a single point of failure for safety. Electrical panels, protective earth, fusing, RCD/ELCB requirements, wet-environment enclosures and installation standards need review by qualified personnel and by the local authority as applicable.

Before any farmer-facing claim, compare the controlled-environment approach against the farmer's actual problem and alternatives. If the key pain point is knowing when a heatwave, frost or rainfall event may occur, a location-specific advisory service might be cheaper and more scalable than indoor cultivation. If the problem is producing a high-value crop in a location with unsuitable or variable climate, controlled cultivation may be one candidate, but the cost per gram, energy use, corm replacement, disease incidence, labour, reliability and market quality must be measured. The system might serve as a research unit or a specialised nursery before being considered a crop-production appliance.

A scale-up gate should therefore demand: (a) documented crop survival and flowering across repeated batches, (b) measured energy and water use per defined output, (c) comparison with a credible local baseline, (d) maintenance and failure-rate logs, (e) food/agricultural handling and electrical compliance review, and (f) sensitivity analysis for input costs and sale price. If these requirements are not met, the honest conclusion is that SAFFRON-AERO is a prototype for learning and experimentation—not a validated commercial farm solution.


### Appendix H — Interpreting control traces, energy use and interrupted trials

A temperature plot is not a performance result until its measurement locations, freshness rules and acceptance band are stated. The chamber has at least three potential thermal locations: the air volume near the corms, the coolant loop, and the hot side of the thermoelectric assembly. Those measurements answer different questions. A coolant reading is not a substitute for air temperature; an air reading does not show the temperature of the TEC hot side; and a GPIO command does not prove the corresponding hardware state. Each data column therefore needs a physical-location label and a statement of whether it is directly measured, derived, imported or simulated.

For each trial, define the analysis interval before examining the results. Exclude startup only according to a predefined rule, not because the warm-up trace looks poor. Calculate time inside band as the proportion of valid samples within the experimental temperature interval, while reporting the total duration and valid-data fraction alongside it. Report sample count and wall-clock coverage separately. If the controller has a long data gap, the count of available samples can look large while the fraction of the trial observed is low. Show gaps visibly and avoid treating samples from one chamber as independent biological replicates.

Cooling energy should be measured with a suitable meter at the supply boundary that matches the question. The nameplate power of the power supply is not actual consumption. The sum of TEC current, pump power, fan power, mist-maker power and controller consumption may vary over time; use a time series or a properly integrated energy reading. Record supply voltage and current, duration, ambient conditions and any changes in fan or pump configuration. If only total energy for a whole trial is measured, identify that aggregate and do not attribute it to individual loads without separate measurements. Calculate cost only from measured kilowatt-hours and an explicitly stated tariff. Do not compare unlike trials if ambient temperature, enclosure configuration, starting corm condition or operating duration differ materially.

Power interruptions create both a control interruption and an evidence interruption. An ESP32 that is fully unpowered cannot sample temperature, save event records or timestamp the beginning of the outage. On the next boot it can log the time since boot and the fact that a restart occurred, but it cannot reconstruct the missing chamber trajectory from its own memory. A battery-backed clock supplies a wall-clock reference, not temperature measurements. An independent temperature logger can preserve the outage trace only if it has its own power and storage. If neither is present, the interval should be labelled “unknown due to power interruption” and excluded from any claim that the chamber held a target band continuously.

When comparing the SAFFRON-AERO prototype with a conventional growing setup, compare the boundary of each system honestly. A small insulated box may allow tighter control at a small scale, while it may require a high energy input per unit of crop and may not represent soil, field microclimate or a commercial greenhouse. Report both crop outcomes and resource inputs: corm survival, flower emergence, harvested stigma mass if actually measured, daughter-corm observations, water used, electricity consumed, labour, maintenance and unplanned downtime. Where results are uncertain, list the observations that would resolve the uncertainty in a future experiment rather than assigning a numerical benefit unsupported by data.

The stop/go decision should depend on predefined evidence. A successful software restart test means that the tested software restored the expected logical configuration under that test. It does not establish that the cooling loop starts, the Peltier hot side stays within limit, the reservoir sensor detects an empty tank or the crop responds favourably. The next stage is justified only after its own acceptance criteria are met. This staged evidence prevents a functioning interface from being mistaken for an effective agricultural intervention.
