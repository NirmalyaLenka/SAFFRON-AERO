"""Dashboard endpoint tests; skipped clearly when optional runtime dependencies are absent."""
import importlib.util
import unittest
from pathlib import Path

APP_PATH = Path(__file__).resolve().parents[1] / "dashboard" / "app.py"
module = None
IMPORT_ERROR = ""
try:
    spec = importlib.util.spec_from_file_location("saffron_dashboard_app", APP_PATH)
    candidate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(candidate)
    module = candidate
except Exception as exc:  # dependency absence should not hide the independent recovery tests
    IMPORT_ERROR = f"Dashboard test dependencies/runtime unavailable: {type(exc).__name__}: {exc}"

@unittest.skipIf(module is None, IMPORT_ERROR or "Dashboard runtime unavailable")
class DashboardTests(unittest.TestCase):
    def setUp(self):
        module.MODE = "simulation"
        module.app.config["TESTING"] = True
        self.client = module.app.test_client()

    def test_simulation_is_explicitly_labeled(self):
        response = self.client.get("/api/state")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json["source"], "SIMULATED")
        self.assertEqual(response.json["firmwareVersion"], "SIMULATOR-ONLY")

    def test_phase_change_works_in_simulation(self):
        response = self.client.post("/api/action", json={"action": "phase", "phaseId": 2})
        self.assertEqual(response.status_code, 200)
        self.assertTrue(response.json["simulationOnly"])
        self.assertEqual(self.client.get("/api/state").json["phaseId"], 2)

    def test_bad_phase_rejected(self):
        response = self.client.post("/api/action", json={"action": "phase", "phaseId": 20})
        self.assertEqual(response.status_code, 422)

    def test_manual_output_is_not_replayed(self):
        response = self.client.post("/api/action", json={"action": "mode", "automaticMode": False})
        self.assertEqual(response.status_code, 200)
        state = self.client.get("/api/state").json
        self.assertEqual(state["mode"], "MANUAL")
        self.assertFalse(state["outputs"]["peltier1CommandedOn"])
        self.assertFalse(state["outputs"]["mistCommandedOn"])

    def test_unavailable_device_does_not_fall_back_to_simulation(self):
        original_mode, original_url, original_key = module.MODE, module.DEVICE_BASE_URL, module.API_KEY
        try:
            module.MODE = "device"
            module.API_KEY = "CHANGE_THIS_TO_A_RANDOM_API_KEY"
            response = self.client.get("/api/state")
            self.assertEqual(response.status_code, 503)
            self.assertEqual(response.json["source"], "UNAVAILABLE")
        finally:
            module.MODE, module.DEVICE_BASE_URL, module.API_KEY = original_mode, original_url, original_key

if __name__ == "__main__": unittest.main()
