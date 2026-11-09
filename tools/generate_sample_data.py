"""Generate a small synthetic CSV fixture; never represents actual chamber measurements."""
from __future__ import annotations
import csv, math
from datetime import datetime, timedelta, timezone
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "data" / "sample_data.csv"
FIELDS = ["timestamp_utc", "source", "phase_id", "phase", "mode", "air_temperature_c",
          "relative_humidity_pct", "coolant_temperature_c", "cooling_demand", "cooling_commanded",
          "mist_commanded", "sensor_data_fresh", "recovery_stage", "lockout_reason"]

def main() -> None:
    start = datetime(2026, 1, 1, tzinfo=timezone.utc)
    with OUT.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS); writer.writeheader()
        for i in range(96):
            writer.writerow({
                "timestamp_utc": (start + timedelta(minutes=15*i)).isoformat().replace("+00:00", "Z"),
                "source": "SIMULATED", "phase_id": 1, "phase": "Floral Induction", "mode": "AUTOMATIC",
                "air_temperature_c": round(22.0 + 0.8*math.sin(i/6), 2),
                "relative_humidity_pct": round(60 + 3*math.sin(i/9), 2),
                "coolant_temperature_c": round(23 + 0.7*math.sin(i/7), 2),
                "cooling_demand": "false", "cooling_commanded": "false", "mist_commanded": "false",
                "sensor_data_fresh": "true", "recovery_stage": "RECOVERY_COMPLETE",
                "lockout_reason": "SYNTHETIC EXAMPLE ONLY"})
    print(f"Wrote {OUT} with synthetic rows. Not real sensor data.")

if __name__ == "__main__": main()
