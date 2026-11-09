# Contributing to SAFFRON-AERO

This is an experimental engineering project. Contributions should improve reproducibility and safety rather than make the demo appear more capable than it is.

## Development workflow

1. Create a branch and describe the intended behaviour or research question.
2. Keep simulation and hardware source labels explicit.
3. Run `python -m unittest discover -s tests -v` and `python tools/validate_project.py`.
4. When firmware changes, build the PlatformIO project and include the exact board/core/library versions and build output.
5. For sensor/control changes, describe the failure mode, safety state, acceptance test and how to reproduce the test.
6. Cite sources for numerical cultivation recommendations and identify the cultivation conditions under which each result was measured.

## Safety and evidence rules

- Do not commit Wi-Fi passwords, API keys, tokens, private data or real experiment locations unless intentionally public.
- Do not enable a hardware output merely to make an integration test pass.
- Never describe GPIO command state as verified current, flow, cooling or mist generation.
- Never claim a biological yield result without an auditable measured protocol and data.
- Label synthetic data as `SIMULATED` and keep it separate from actual sensor logs.
- Maintain backward-compatibility or document config-schema migration whenever persisted fields change.

## Reporting issues

Include firmware commit/version, board variant, PlatformIO build output, configuration schema, wiring diagram, sensor model and clear reproduction steps. Avoid posting private credentials. Hardware changes that affect high-current or wet components need a wiring/safety review.
