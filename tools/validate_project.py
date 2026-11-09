"""Lightweight project hygiene checks; does not replace PlatformIO or hardware validation."""
from __future__ import annotations
import ast, csv, hashlib, json, re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "README.md", "firmware/esp32_controller/platformio.ini", "firmware/esp32_controller/src/main.cpp",
    "firmware/esp32_controller/include/config.h", "dashboard/app.py", "dashboard/templates/index.html",
    "docs/saffron_growth_environment.md", "docs/problem_solution_report.md", "docs/research_and_references.md",
    "docs/electrical_safety.md", "docs/hardware_and_wiring.md", "shared/protocol.md",
    "shared/recovery_model.py", "tests/test_recovery_model.py", "tests/test_dashboard.py",
    "hardware/bill_of_materials.csv", "hardware/pin_map.csv", "LICENSE", "CONTRIBUTING.md",
    "CHANGELOG.md", ".gitignore", "BUILD_AND_VALIDATION_REPORT.md",
]

def main() -> int:
    errors, warnings = [], []
    for rel in REQUIRED:
        path = ROOT / rel
        if not path.is_file() or path.stat().st_size == 0:
            errors.append(f"Missing/empty required file: {rel}")
    py_files = list(ROOT.rglob("*.py"))
    for path in py_files:
        try: ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
        except (SyntaxError, UnicodeDecodeError) as exc: errors.append(f"Python syntax: {path.relative_to(ROOT)}: {exc}")
    for rel in ("hardware/bill_of_materials.csv", "hardware/pin_map.csv", "data/sample_data.csv"):
        try:
            with (ROOT / rel).open(newline="", encoding="utf-8") as f:
                rows = list(csv.reader(f))
            if len(rows) < 2: warnings.append(f"CSV has header but no data rows: {rel}")
            elif any(len(row) != len(rows[0]) for row in rows[1:]): errors.append(f"CSV row width mismatch: {rel}")
        except Exception as exc: errors.append(f"CSV read failed: {rel}: {exc}")
    report = ROOT / "docs/problem_solution_report.md"
    word_count = len(report.read_text(encoding="utf-8").split()) if report.exists() else 0
    if word_count < 13000: warnings.append(f"Long report word count {word_count}; target is approximately 15,000")
    firmware_path = ROOT / "firmware/esp32_controller/src/main.cpp"
    config_path = ROOT / "firmware/esp32_controller/include/config.h"
    if firmware_path.exists() and config_path.exists():
        firmware = firmware_path.read_text(encoding="utf-8")
        config = config_path.read_text(encoding="utf-8")
        setup_start = firmware.find("void setup() {")
        setup_end = firmware.find("\nvoid loop()", setup_start)
        setup_body = firmware[setup_start:setup_end] if setup_start >= 0 and setup_end > setup_start else ""
        if not setup_body or setup_body.find("setOutputsSafe();") < 0 or setup_body.find("loadConfig();") < 0 or setup_body.find("setOutputsSafe();") > setup_body.find("loadConfig();"):
            errors.append("Firmware source invariant failed: outputs must be forced safe before persisted configuration is loaded")
        if "recoveryFinished && recoveryStage == RECOVERY_COMPLETE" not in firmware:
            errors.append("Firmware source invariant failed: completed boot recovery must not restart the recovery sequence every loop")
        for field in ("manualCoolingOverrideActive", "manualMistOverrideActive", "commissioningRecordAcknowledged", "hotSideLimitC"):
            if f'doc["{field}"]' not in firmware:
                errors.append(f"Firmware persistence invariant failed: {field} must be serialized")
        if "MANUAL_OVERRIDE_EXPIRED_AT_BOOT" not in firmware or "runtimeSafetyLockout" not in firmware:
            errors.append("Firmware recovery invariant failed: manual overrides must not replay and runtime safety faults must stay locked")
        if 'html.replace("CHANGE_THIS_TO_A_RANDOM_API_KEY", NetworkConfig::API_KEY)' in firmware:
            errors.append("Firmware UI invariant failed: API key must not be embedded in fallback HTML")
        for flag in ("PELTIER_DRIVER_VERIFIED", "MIST_DRIVER_VERIFIED", "COOLANT_PUMP_VERIFIED", "HOT_SIDE_FAN_VERIFIED", "FLOW_SENSOR_VERIFIED", "RESERVOIR_SENSOR_VERIFIED", "HOT_SIDE_SENSOR_VERIFIED", "HARDWARE_ESTOP_CIRCUIT_VERIFIED"):
            if not re.search(r"constexpr\s+bool\s+" + flag + r"\s*=\s*false\s*;", config):
                errors.append(f"Firmware commissioning flag is not explicitly disabled by default: {flag}")
        for pin in ("PELTIER_1_ENABLE", "PELTIER_2_ENABLE", "MIST_ENABLE", "COOLANT_PUMP_ENABLE", "HOT_SIDE_FAN_ENABLE", "FLOW_SWITCH_INPUT", "RESERVOIR_OK_INPUT", "HOT_SIDE_SENSOR_INDEX", "LIGHT_SENSOR_INPUT"):
            if not re.search(r"constexpr\s+int\s+" + pin + r"\s*=\s*-1\s*;", config):
                errors.append(f"Unsafe/unreviewed optional output or feedback pin must be disabled by default: {pin}")
    readme = (ROOT / "README.md").read_text(encoding="utf-8") if (ROOT / "README.md").exists() else ""
    for match in re.findall(r"\]\(([^)]+\.md)(?:#[^)]*)?\)", readme):
        if not match.startswith(("http://", "https://")) and not (ROOT / match).exists():
            warnings.append(f"README relative link target not found: {match}")
    out = {"root": str(ROOT), "required_files": len(REQUIRED), "python_files_syntax_checked": len(py_files),
           "report_word_count": word_count, "errors": errors, "warnings": warnings}
    print(json.dumps(out, indent=2))
    return 1 if errors else 0

if __name__ == "__main__": raise SystemExit(main())
