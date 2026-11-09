# Electrical and thermal safety

## Stop conditions

Do not enable TEC drivers if any one of these is unknown: driver topology/current rating, current limiting, supply ratings, fuse arrangement, heatsink/hot-side capacity, pump and fan control, coolant-flow proof, hot-side temperature sensor identity, safe response to a sensor fault, or physical disconnect. Do not enable the mist load without verifying the load voltage/current and a functioning reservoir-level interlock. If water can reach live wiring, redesign the enclosure before operation.

## Never power a Peltier from an ESP32 pin

ESP32 GPIO pins are logic-level signals. TEC1-12715 modules are high-current devices and need a suitable power stage, appropriate wiring, a compatible supply, protection and a thermal assembly. The pins in this repository are only enable-signal proposals for an external driver. The actual driver type is unknown, and the supplied firmware does not assume PWM capability.

## Main hazards

- **Overcurrent / wire heating:** module and pump current, especially at startup or stall, can exceed a connector or wire rating. Size each branch from measured load and datasheet values, and install appropriate overcurrent protection.
- **Thermal runaway on hot side:** without adequate hot-side heat rejection, a TEC may overheat even when the cold side is intended to cool the chamber. Use a verified hot-side sensor and a hardware-appropriate cutoff.
- **Loss of coolant flow:** a pump command does not prove circulation. A flow switch must be tested and must lock out TECs when flow is absent.
- **Condensation and water:** cold blocks, glass and surfaces can form condensate. Use drip management, appropriate enclosure separation, protected connectors and a safe disconnect.
- **Relay/inductive switching:** select a driver suited to the load; include required flyback/surge suppression for inductive loads. Verify polarity and active state so a reset or floating pin does not turn a load on.
- **Incorrect boot state:** test every output during power-up, reset, brownout and watchdog restart with loads disconnected first. Use hardware pull-downs or driver-enable defaults that keep loads off even if the ESP32 pin floats.
- **Unexpected re-energisation:** automatic recovery must not replay a manual forced-on request. Physical emergency stop and overtemperature/overcurrent protections must not depend solely on software.

## Commissioning order

1. Inspect wiring with all supplies disconnected; check shorts and continuity.
2. Confirm each supply output and polarity without connecting the ESP32 or high-current load.
3. Power the ESP32 and sensor bus only. Confirm pins remain at safe levels through boot, reset and a simulated sensor fault.
4. Verify each driver input using a meter or logic analyzer with the load disconnected.
5. Test pump, fan, flow switch and reservoir switch individually using protected supply branches. Verify actual flow and real sensor changes rather than commands alone.
6. Map every DS18B20 ROM address to its physical sensor. Confirm hot-side probe identity and check readings against a suitable reference.
7. Test loss of flow, disconnected sensor, stale measurement, overtemperature, reservoir-low and emergency-stop response. Confirm that TEC output is forced off.
8. Only after sign-off, update the constants in `config.h` and repeat the sequence under supervision with no corms inside.
9. Conduct a supervised thermal test and record current, voltage, temperatures and power. Do not start the biological experiment until the chamber can reach a stable safe condition without excess condensation.

## Power loss

The ESP32 cannot control the chamber while the ESP32 and loads have no power. On restoration, the controller reads saved state, validates sensors and applicable interlocks, and may resume automatic mode. It must not drive the TEC at 100% merely because a restart occurred. A UPS or separate logger is a separate future design and should be selected based on load current, runtime, isolation, battery safety and measured energy consumption.

## Physical emergency stop

Use an accessible hardware disconnect or safety mechanism suited to the current and voltage involved. A software `E-STOP` endpoint forces configured GPIO commands LOW and latches a software fault; it is not a substitute for a physical stop circuit and cannot disconnect an arbitrary power stage unless the circuit is designed to do that. Verify that turning off the ESP32, disconnecting sensor wires or rebooting cannot bypass hardware safety.
