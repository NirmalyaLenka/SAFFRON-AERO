# Project overview

SAFFRON-AERO is a small controlled-environment agriculture prototype with a phase-aware ESP32 controller. It couples temperature/humidity observation to a software state machine, optional cooling and misting drivers, power-failure configuration recovery, local browser status and an optional laptop dashboard for CSV/experiment records.

The current chamber is approximately 2 ft × 1.5 ft × 1 ft, built with roughly 3 cm expanded-polystyrene insulation and a glass viewing section. The figures describe the approximate enclosure, not verified internal cultivation volume. Around 25 corms have been described as available, but batch count and mass should be confirmed before trials. The glass section is both an observation feature and a possible solar heat-gain path.

The first engineering deliverable is safe deterministic recovery. The firmware starts load outputs LOW, validates stored configuration, waits for fresh measurements and performs configured interlocks before restoring automatic output authority. A previous manual actuator command is deliberately not replayed. If the saved configuration is missing or invalid, the interface exposes a recovery/commissioning state rather than silently selecting a new phase.

The software defaults are not equivalent to commissioning. The proposed GPIO values for Peltier, mist maker and sensors must be checked against the actual ESP32 board and driver hardware. Unknown components remain unknown in the BOM. The firmware defaults leave verified-driver and safety-input constants false or unconfigured, so real loads remain blocked.

The dashboard is optional. The local ESP32 runs its own sensor updates, control decisions, alarms, phase management and API independent of a laptop. Simulation runs only when explicitly requested and is labelled clearly. Device mode never masks a communication failure with simulated values.

Success for the first iteration means reproducible configuration recovery, accurate timestamps/freshness reporting, safe no-data behaviour, documented interlock limitations and repeatable experiments. Successful saffron flowering, usable stigma yield, better daughter-corm production, lower energy consumption or commercial viability are research outcomes that have not yet been established.
