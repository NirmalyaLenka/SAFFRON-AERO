"""SAFFRON-AERO local dashboard. Explicit simulation mode is never labelled as live hardware."""
from __future__ import annotations
import argparse
import csv
import json
import math
import os
import random
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import requests
from flask import Flask, Response, jsonify, render_template, request

ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "data"
DATA_DIR.mkdir(parents=True, exist_ok=True)
TELEMETRY_FILE = DATA_DIR / "telemetry.csv"
EXPERIMENT_FILE = DATA_DIR / "experiment_log.csv"
PHASES = [
    "Corm Dormancy / Preparation", "Floral Induction", "Flowering",
    "Vegetative Growth", "Daughter-Corm Development", "Dormancy / Cycle Transition",
]
DEFAULT_PROFILES = [
    {"airTargetC": 25, "airMinC": 20, "airMaxC": 30, "rhTargetPct": 60, "rhMinPct": 35, "rhMaxPct": 80, "tempHysteresisC": 1.0, "rhHysteresisPct": 5.0, "provisional": True},
    {"airTargetC": 22, "airMinC": 18, "airMaxC": 26, "rhTargetPct": 60, "rhMinPct": 35, "rhMaxPct": 80, "tempHysteresisC": 1.0, "rhHysteresisPct": 5.0, "provisional": True},
    {"airTargetC": 17, "airMinC": 15, "airMaxC": 20, "rhTargetPct": 60, "rhMinPct": 35, "rhMaxPct": 80, "tempHysteresisC": 1.0, "rhHysteresisPct": 5.0, "provisional": True},
    {"airTargetC": 17, "airMinC": 10, "airMaxC": 20, "rhTargetPct": 60, "rhMinPct": 35, "rhMaxPct": 80, "tempHysteresisC": 1.0, "rhHysteresisPct": 5.0, "provisional": True},
    {"airTargetC": 20, "airMinC": 15, "airMaxC": 27, "rhTargetPct": 60, "rhMinPct": 35, "rhMaxPct": 80, "tempHysteresisC": 1.0, "rhHysteresisPct": 5.0, "provisional": True},
    {"airTargetC": 20, "airMinC": 15, "airMaxC": 27, "rhTargetPct": 60, "rhMinPct": 35, "rhMaxPct": 80, "tempHysteresisC": 1.0, "rhHysteresisPct": 5.0, "provisional": True},
]

app = Flask(__name__)
app.config["JSON_SORT_KEYS"] = False
MODE = "device"
DEVICE_BASE_URL = os.getenv("SAFFRON_DEVICE_URL", "http://192.168.4.1").rstrip("/")
API_KEY = os.getenv("SAFFRON_API_KEY", "CHANGE_THIS_TO_A_RANDOM_API_KEY")
TELEMETRY_INTERVAL = max(5, int(os.getenv("SAFFRON_LOG_INTERVAL", "15")))
LOG_LOCK = threading.Lock()
LAST_TELEMETRY_WRITE = 0.0

sim_lock = threading.Lock()
sim_started = time.monotonic()
sim_state: dict[str, Any] = {
    "ok": True, "firmwareVersion": "SIMULATOR-ONLY", "schemaVersion": 1,
    "phaseId": 1, "phase": PHASES[1], "automaticMode": True, "mode": "AUTOMATIC",
    "configLoaded": True, "configRecoveryRequired": False,
    "configurationRevision": 1, "configurationSaveStatus": "SIMULATED SAVE — NOT DEVICE NVS",
    "recoveryStage": "RECOVERY_COMPLETE", "recoveryComplete": True,
    "previousMode": "AUTOMATIC", "lockoutReason": "SIMULATION ONLY; NO HARDWARE OUTPUTS EXIST",
    "bootRelativeMs": 0, "sensorDataFresh": True, "source": "SIMULATED",
    "sensors": {"airTemperatureC": 22.6, "relativeHumidityPct": 61.0,
                "coolantTemperatureC": 23.2, "hotSideTemperatureC": None,
                "dhtValid": True, "ds18b20Valid": True, "hotSideValid": False,
                "airAgeMs": 300, "coolantAgeMs": 650, "lightSensor": "NOT INSTALLED"},
    "outputs": {"peltier1CommandedOn": False, "peltier2CommandedOn": False,
                "coolingDemand": False, "pumpCommandedOn": False, "fanCommandedOn": False,
                "mistCommandedOn": False, "actualElectricalFeedbackAvailable": False},
    "interlocks": {"estopLatched": False, "flowFeedbackInstalled": False, "flowOkay": False,
                    "reservoirFeedbackInstalled": False, "reservoirOkay": False,
                    "peltierDriverVerified": False, "mistDriverVerified": False},
    "activeProfile": DEFAULT_PROFILES[1], "events": [],
}
sim_config: dict[str, Any] = {
    "schema": 1, "revision": 1, "phase": 1, "automaticMode": True,
    "airOffsetC": 0, "rhOffset": 0, "coolantOffsetC": 0, "manualTimeoutMs": 300000,
    "profiles": [dict(p, rhTarget=p["rhTargetPct"], rhMin=p["rhMinPct"], rhMax=p["rhMaxPct"],
                       tempHyst=p["tempHysteresisC"], rhHyst=p["rhHysteresisPct"],
                       mistPulseMs=2000, mistRestMs=60000) for p in DEFAULT_PROFILES],
}

def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")

def _device_headers() -> dict[str, str]:
    return {"X-API-Key": API_KEY, "Content-Type": "application/json"}

def device_get(path: str, timeout: float = 2.5) -> dict[str, Any]:
    if API_KEY.startswith("CHANGE_THIS"):
        raise RuntimeError("Set SAFFRON_API_KEY to match the changed firmware API key before device mode.")
    response = requests.get(DEVICE_BASE_URL + path, headers=_device_headers(), timeout=timeout)
    response.raise_for_status()
    return response.json()

def device_post(path: str, payload: dict[str, Any], timeout: float = 3.0) -> dict[str, Any]:
    if API_KEY.startswith("CHANGE_THIS"):
        raise RuntimeError("Set SAFFRON_API_KEY to match the changed firmware API key before device mode.")
    response = requests.post(DEVICE_BASE_URL + path, headers=_device_headers(), json=payload, timeout=timeout)
    response.raise_for_status()
    result = response.json()
    if not result.get("ok", False):
        raise RuntimeError(result.get("error", "Device rejected the command"))
    return result

def _ensure_csv(path: Path, columns: list[str]) -> None:
    if not path.exists() or path.stat().st_size == 0:
        with path.open("w", newline="", encoding="utf-8") as f:
            csv.DictWriter(f, fieldnames=columns).writeheader()

def _append_telemetry(state: dict[str, Any]) -> None:
    global LAST_TELEMETRY_WRITE
    now = time.monotonic()
    if now - LAST_TELEMETRY_WRITE < TELEMETRY_INTERVAL:
        return
    with LOG_LOCK:
        if now - LAST_TELEMETRY_WRITE < TELEMETRY_INTERVAL:
            return
        cols = ["timestamp_utc", "source", "phase_id", "phase", "mode", "air_temperature_c",
                "relative_humidity_pct", "coolant_temperature_c", "cooling_demand", "cooling_commanded",
                "mist_commanded", "sensor_data_fresh", "recovery_stage", "lockout_reason"]
        _ensure_csv(TELEMETRY_FILE, cols)
        sensors, outputs = state.get("sensors", {}), state.get("outputs", {})
        row = {"timestamp_utc": utc_now(), "source": state.get("source", "ESP32"),
               "phase_id": state.get("phaseId"), "phase": state.get("phase"), "mode": state.get("mode"),
               "air_temperature_c": sensors.get("airTemperatureC"), "relative_humidity_pct": sensors.get("relativeHumidityPct"),
               "coolant_temperature_c": sensors.get("coolantTemperatureC"), "cooling_demand": outputs.get("coolingDemand"),
               "cooling_commanded": outputs.get("peltier1CommandedOn"), "mist_commanded": outputs.get("mistCommandedOn"),
               "sensor_data_fresh": state.get("sensorDataFresh"), "recovery_stage": state.get("recoveryStage"),
               "lockout_reason": state.get("lockoutReason")}
        with TELEMETRY_FILE.open("a", newline="", encoding="utf-8") as f:
            csv.DictWriter(f, fieldnames=cols).writerow(row)
        LAST_TELEMETRY_WRITE = now

def _update_simulation() -> dict[str, Any]:
    with sim_lock:
        elapsed = time.monotonic() - sim_started
        profile = sim_config["profiles"][sim_state["phaseId"]]
        target = float(profile["airTargetC"])
        # A transparent toy model for UI checks, not a thermal simulation or plant prediction.
        air = target + 1.1 * math.sin(elapsed / 31) + 0.12 * math.sin(elapsed / 4.2)
        rh = float(profile["rhTarget"]) + 4.0 * math.sin(elapsed / 38 + 1.1)
        coolant = air + 0.7 + (1.8 if sim_state["outputs"]["peltier1CommandedOn"] else 0.0)
        sim_state["bootRelativeMs"] = int(elapsed * 1000)
        sim_state["sensors"].update({"airTemperatureC": round(air, 2), "relativeHumidityPct": round(rh, 2),
                                      "coolantTemperatureC": round(coolant, 2), "airAgeMs": 300,
                                      "coolantAgeMs": 650, "dhtValid": True, "ds18b20Valid": True})
        sim_state["phase"] = PHASES[sim_state["phaseId"]]
        sim_state["activeProfile"] = {"airTargetC": profile["airTargetC"], "airMinC": profile["airMinC"],
            "airMaxC": profile["airMaxC"], "rhTargetPct": profile["rhTarget"], "rhMinPct": profile["rhMin"],
            "rhMaxPct": profile["rhMax"], "tempHysteresisC": profile["tempHyst"],
            "rhHysteresisPct": profile["rhHyst"], "provisional": True}
        sim_state["source"] = "SIMULATED"
        return json.loads(json.dumps(sim_state))

def get_state() -> dict[str, Any]:
    if MODE == "simulation":
        state = _update_simulation()
    else:
        state = device_get("/api/status")
        state["source"] = "ESP32"
    _append_telemetry(state)
    return state

@app.get("/")
def index():
    return render_template("index.html", mode=MODE, device_url=DEVICE_BASE_URL)

@app.get("/api/state")
def api_state():
    try:
        return jsonify(get_state())
    except (requests.RequestException, ValueError, RuntimeError) as exc:
        return jsonify({"ok": False, "source": "UNAVAILABLE", "error": str(exc),
                        "hint": "Device mode never silently falls back to simulated readings. Check ESP32 Wi-Fi/API settings or start with --simulate."}), 503

@app.get("/api/config")
def api_config():
    try:
        if MODE == "simulation":
            with sim_lock:
                return jsonify({**json.loads(json.dumps(sim_config)), "ok": True, "source": "SIMULATED"})
        result = device_get("/api/config")
        result["source"] = "ESP32"
        return jsonify(result)
    except (requests.RequestException, ValueError, RuntimeError) as exc:
        return jsonify({"ok": False, "error": str(exc)}), 503

@app.post("/api/action")
def api_action():
    body = request.get_json(silent=True) or {}
    action = body.get("action")
    try:
        if MODE == "simulation":
            with sim_lock:
                if action == "phase":
                    phase_id = int(body.get("phaseId", -1))
                    if phase_id not in range(6): return jsonify({"ok": False, "error": "phaseId must be 0..5"}), 422
                    sim_state["phaseId"] = sim_config["phase"] = phase_id
                elif action == "mode":
                    auto = body.get("automaticMode")
                    if not isinstance(auto, bool): return jsonify({"ok": False, "error": "automaticMode must be boolean"}), 422
                    sim_state["automaticMode"] = sim_config["automaticMode"] = auto
                    sim_state["mode"] = "AUTOMATIC" if auto else "MANUAL"
                    if not auto:
                        sim_state["outputs"].update({"peltier1CommandedOn": False, "peltier2CommandedOn": False, "mistCommandedOn": False})
                elif action == "commission":
                    return jsonify({"ok": False, "error": "Simulation is already initialized; commissioning only applies to a fresh physical ESP32."}), 409
                elif action == "factory_reset":
                    return jsonify({"ok": False, "error": "Factory reset is disabled in the simulator."}), 409
                elif action == "estop":
                    sim_state["interlocks"]["estopLatched"] = True
                    sim_state["automaticMode"] = False; sim_state["mode"] = "MANUAL"
                    sim_state["outputs"].update({"peltier1CommandedOn": False, "peltier2CommandedOn": False, "pumpCommandedOn": False, "fanCommandedOn": False, "mistCommandedOn": False})
                    sim_state["lockoutReason"] = "SIMULATED E-STOP — no physical circuit was operated"
                elif action == "profile":
                    phase_id = int(body.get("phaseId", -1)); profile = body.get("profile", {})
                    if phase_id not in range(6): return jsonify({"ok": False, "error": "phaseId must be 0..5"}), 422
                    for key in ("airTargetC", "airMinC", "airMaxC", "rhTarget", "rhMin", "rhMax", "tempHyst", "rhHyst"):
                        if key not in profile or not isinstance(profile[key], (float, int)) or not math.isfinite(profile[key]):
                            return jsonify({"ok": False, "error": f"Missing/invalid profile value: {key}"}), 422
                    if not (-5 <= profile["airTargetC"] <= 45 and -10 <= profile["airMinC"] < profile["airMaxC"] <= 50 and profile["airMinC"] <= profile["airTargetC"] <= profile["airMaxC"] and 10 <= profile["rhTarget"] <= 95 and 10 <= profile["rhMin"] < profile["rhMax"] <= 100 and profile["rhMin"] <= profile["rhTarget"] <= profile["rhMax"]):
                        return jsonify({"ok": False, "error": "Profile bounds are inconsistent or outside safe application ranges"}), 422
                    sim_config["profiles"][phase_id].update(profile)
                else:
                    return jsonify({"ok": False, "error": "Unknown action"}), 422
                sim_config["revision"] += 1; sim_state["configurationRevision"] = sim_config["revision"]
                sim_state["configurationSaveStatus"] = "SIMULATED SAVE — NOT DEVICE NVS"
                sim_state["recoveryStage"] = "SAFETY_LOCKOUT" if sim_state["interlocks"]["estopLatched"] else "RECOVERY_COMPLETE"
                sim_state["recoveryComplete"] = not sim_state["interlocks"]["estopLatched"]
                sim_state["events"].append({"bootMs": sim_state["bootRelativeMs"], "kind": action.upper(), "detail": "Simulated action only; no hardware was controlled"})
            return jsonify({"ok": True, "simulationOnly": True, "message": "State changed in simulator; nothing was sent to hardware"})

        routes = {
            "phase": ("/api/phase", {"phaseId": body.get("phaseId")}),
            "mode": ("/api/mode", {"automaticMode": body.get("automaticMode")}),
            "estop": ("/api/estop", {}),
            "commission": ("/api/commission", {"confirm": body.get("confirm") is True, "commissioningAcknowledged": body.get("commissioningAcknowledged") is True}),
            "factory_reset": ("/api/factory-reset", {"confirm": body.get("confirm") is True, "phrase": body.get("phrase", "")}),
            "clear_fault": ("/api/clear-fault", {"confirm": body.get("confirm") is True}),
            "cooling": ("/api/cooling", {"enabled": body.get("enabled"), "durationMs": body.get("durationMs", 30000)}),
            "mist": ("/api/mist", {"enabled": body.get("enabled"), "durationMs": body.get("durationMs", 30000)}),
        }
        if action not in routes: return jsonify({"ok": False, "error": "Unknown action"}), 422
        path, payload = routes[action]
        if action == "phase" and not isinstance(payload["phaseId"], int): return jsonify({"ok": False, "error": "phaseId integer required"}), 422
        result = device_post(path, payload)
        # Read-after-write: return authoritative state to avoid claiming a local change was saved prematurely.
        state = device_get("/api/status")
        return jsonify({"ok": True, "deviceResult": result, "state": state})
    except (requests.RequestException, ValueError, RuntimeError) as exc:
        return jsonify({"ok": False, "error": str(exc)}), 503

@app.post("/api/profile")
def api_profile():
    body = request.get_json(silent=True) or {}
    phase_id, profile = body.get("phaseId"), body.get("profile")
    if not isinstance(phase_id, int) or phase_id not in range(6) or not isinstance(profile, dict):
        return jsonify({"ok": False, "error": "phaseId 0..5 and profile object required"}), 422
    try:
        if MODE == "simulation":
            # Reuse the same validation and commit path as other simulated actions.
            with sim_lock:
                for key in ("airTargetC", "airMinC", "airMaxC", "rhTarget", "rhMin", "rhMax", "tempHyst", "rhHyst", "mistPulseMs", "mistRestMs"):
                    if key not in profile or not isinstance(profile[key], (int, float)) or not math.isfinite(profile[key]):
                        return jsonify({"ok": False, "error": f"Missing/invalid profile value: {key}"}), 422
                if not (-5 <= profile["airTargetC"] <= 45 and -10 <= profile["airMinC"] < profile["airMaxC"] <= 50 and profile["airMinC"] <= profile["airTargetC"] <= profile["airMaxC"] and 10 <= profile["rhTarget"] <= 95 and 10 <= profile["rhMin"] < profile["rhMax"] <= 100 and profile["rhMin"] <= profile["rhTarget"] <= profile["rhMax"] and 0.2 <= profile["tempHyst"] <= 5 and 1 <= profile["rhHyst"] <= 15 and 0 <= profile["mistPulseMs"] <= 8000 and profile["mistRestMs"] >= 10000):
                    return jsonify({"ok": False, "error": "Profile failed numeric/range validation"}), 422
                sim_config["profiles"][phase_id].update(profile); sim_config["revision"] += 1
                sim_state["configurationRevision"] = sim_config["revision"]
                sim_state["configurationSaveStatus"] = "SIMULATED SAVE — NOT DEVICE NVS"
            return jsonify({"ok": True, "simulationOnly": True})
        response = device_post("/api/config", {"phaseId": phase_id, "profile": profile})
        return jsonify({"ok": True, "deviceResult": response, "state": device_get("/api/status")})
    except (requests.RequestException, ValueError, RuntimeError) as exc:
        return jsonify({"ok": False, "error": str(exc)}), 503

@app.post("/api/experiment")
def api_experiment():
    body = request.get_json(silent=True) or {}
    required = ["batch_id", "observation"]
    if any(not isinstance(body.get(k), str) or not body[k].strip() for k in required):
        return jsonify({"ok": False, "error": "batch_id and observation are required"}), 422
    columns = ["timestamp_utc", "source", "batch_id", "corm_count", "approx_mass_g", "phase", "flower_count", "harvest_mass_g", "daughter_corm_count", "observation"]
    _ensure_csv(EXPERIMENT_FILE, columns)
    row = {"timestamp_utc": utc_now(), "source": MODE, "batch_id": body["batch_id"].strip(),
           "corm_count": body.get("corm_count", ""), "approx_mass_g": body.get("approx_mass_g", ""),
           "phase": body.get("phase", ""), "flower_count": body.get("flower_count", ""),
           "harvest_mass_g": body.get("harvest_mass_g", ""), "daughter_corm_count": body.get("daughter_corm_count", ""),
           "observation": body["observation"].strip()}
    with LOG_LOCK:
        with EXPERIMENT_FILE.open("a", newline="", encoding="utf-8") as f:
            csv.DictWriter(f, fieldnames=columns).writerow(row)
    return jsonify({"ok": True, "message": "Experiment observation appended"})

@app.get("/api/export/telemetry.csv")
def export_telemetry():
    if not TELEMETRY_FILE.exists(): return jsonify({"ok": False, "error": "No telemetry has been logged yet"}), 404
    return Response(TELEMETRY_FILE.read_text(encoding="utf-8"), mimetype="text/csv",
                    headers={"Content-Disposition": "attachment; filename=saffron-aero-telemetry.csv"})

@app.get("/api/export/experiments.csv")
def export_experiments():
    if not EXPERIMENT_FILE.exists(): return jsonify({"ok": False, "error": "No experiment observations have been logged yet"}), 404
    return Response(EXPERIMENT_FILE.read_text(encoding="utf-8"), mimetype="text/csv",
                    headers={"Content-Disposition": "attachment; filename=saffron-aero-experiments.csv"})

def main() -> None:
    global MODE
    parser = argparse.ArgumentParser(description="SAFFRON-AERO offline dashboard")
    parser.add_argument("--simulate", action="store_true", help="Use explicitly marked synthetic readings; sends no commands to hardware")
    parser.add_argument("--host", default=os.getenv("SAFFRON_DASHBOARD_HOST", "127.0.0.1"), help="Bind address; use 0.0.0.0 only on a trusted local network")
    parser.add_argument("--port", type=int, default=int(os.getenv("SAFFRON_DASHBOARD_PORT", "5000")))
    args = parser.parse_args()
    MODE = "simulation" if args.simulate else "device"
    print(f"SAFFRON-AERO dashboard mode: {MODE.upper()}")
    if MODE == "simulation": print("WARNING: synthetic demo values only; no physical outputs exist and no hardware commands are sent.")
    else: print(f"ESP32 base URL: {DEVICE_BASE_URL}; configure SAFFRON_API_KEY. Device mode will not fake a connection.")
    app.run(host=args.host, port=args.port, debug=False, threaded=True)

if __name__ == "__main__":
    main()
