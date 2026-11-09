# Operating procedure

## Routine startup

1. Check the chamber, glass panel, wiring, reservoir, tubing, drains and physical disconnect visually.
2. Power the logic controller and watch the OLED/Serial boot sequence. Confirm the saved phase, configuration revision, recovery stage and sensor health.
3. Do not rely on saved setpoints alone. Read the active interlocks and lockout reason in the API/dashboard. If the configuration is missing, a sensor is stale, or the cooling-loop check fails, investigate before loading corms or energising high-current loads.
4. Observe actual chamber temperature/RH and the cooling-loop reading. The DS18B20 measures only its verified physical location.
5. Inspect for condensation, unusual sound, leaks, odour, warm connectors, or unexpected load behaviour. If any occur, use the physical disconnect and investigate.
6. Record the corm batch, stage, observations and any interventions.

## Phase changes

Choose phases based on an actual observation/research protocol, not an arbitrary timer. Commit the phase using the interface and confirm that the ESP32 returned a save status and incremented revision. Reboot only as part of a planned recovery test and verify the same phase returns. Do not claim the phase name itself proves that the plant has reached that biological stage.

## Manual output commands

Manual outputs require the controller to be in manual mode, sensors to be fresh and that subsystem's hardware interlocks to pass. Requests have a timeout. They are not persisted as forced-on commands and must not be replayed after reboot. Use manual commands only while physically supervising the hardware; do not use them to bypass a failed flow switch, hot-side sensor or reservoir-level switch.

## Shutdown

Switch to manual / loads off, verify the output commands read OFF, then isolate load power where appropriate. A UI reading is not a replacement for a physical disconnect. If maintenance involves water, the TEC stack, pump or wiring, disconnect all relevant supplies and verify they are de-energised before touching them.

## Power interruption recovery

The saved phase and configuration should survive because they are stored in NVS. When power returns, the controller must reacquire valid measurements and test configured safety interlocks. If it was previously in manual mode, the controller keeps actuator commands off and asks for a fresh operator action. If the controller was in automatic mode but a sensor/interlock is invalid, outputs remain locked. The system cannot record temperature drift during a total power outage unless an independent powered logger/RTC exists.
