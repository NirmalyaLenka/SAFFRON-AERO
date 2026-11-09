import unittest
import json
import zlib
from shared.recovery_model import Config, DualSlotStorage, RecoveryController, Profile

SAFE_SENSORS = {"air_valid": True, "coolant_valid": True, "air_fresh": True, "coolant_fresh": True}
SAFE_INTERLOCKS = {"driver_verified": True, "estop_clear": True, "cooling_proof": True, "mist_proof": True}

class RecoveryTests(unittest.TestCase):
    def make_saved(self):
        storage = DualSlotStorage()
        cfg = Config()
        cfg.phase_id = 1  # Floral Induction
        cfg.profiles[1] = Profile(air_target_c=21.5, air_min_c=18, air_max_c=26,
                                  rh_target_pct=62, rh_min_pct=35, rh_max_pct=80,
                                  temp_hysteresis_c=0.8, rh_hysteresis_pct=4, mist_pulse_ms=2500, mist_rest_ms=45000)
        self.assertTrue(storage.save(cfg))
        return storage

    def test_phase_and_exact_profile_persist_across_restart(self):
        storage = self.make_saved()
        rebooted = DualSlotStorage.__new__(DualSlotStorage)
        rebooted.slots = storage.slots.copy(); rebooted.active_slot = storage.active_slot
        cfg = rebooted.load()
        self.assertEqual(cfg.phase_id, 1)
        self.assertEqual(cfg.profiles[1].air_target_c, 21.5)
        self.assertEqual(cfg.profiles[1].mist_rest_ms, 45000)

    def test_auto_restores_only_after_valid_sensor_and_interlocks(self):
        result = RecoveryController().boot(self.make_saved(), SAFE_SENSORS, SAFE_INTERLOCKS)
        self.assertEqual(result.phase, "Floral Induction")
        self.assertTrue(result.automatic_resumed)
        self.assertFalse(result.outputs_enabled, "Recovery does not by itself authorize actuator outputs")

    def test_invalid_sensor_blocks_resume(self):
        result = RecoveryController().boot(self.make_saved(), {"air_valid": False, "coolant_valid": True, "air_fresh": True, "coolant_fresh": True}, SAFE_INTERLOCKS)
        self.assertEqual(result.stage, "SENSOR_RECOVERY")
        self.assertFalse(result.automatic_resumed)

    def test_failed_interlock_blocks_resume(self):
        locks = dict(SAFE_INTERLOCKS); locks["cooling_proof"] = False
        result = RecoveryController().boot(self.make_saved(), SAFE_SENSORS, locks)
        self.assertEqual(result.stage, "SAFETY_LOCKOUT")
        self.assertIn("cooling_proof", result.lockout_reason)

    def test_missing_interlock_evidence_blocks_resume(self):
        result = RecoveryController().boot(self.make_saved(), SAFE_SENSORS, {"driver_verified": True})
        self.assertEqual(result.stage, "SAFETY_LOCKOUT")
        self.assertIn("cooling_proof", result.lockout_reason)

    def test_stale_reading_blocks_resume_even_if_numeric_value_exists(self):
        sensors = dict(SAFE_SENSORS); sensors["coolant_fresh"] = False
        result = RecoveryController().boot(self.make_saved(), sensors, SAFE_INTERLOCKS)
        self.assertEqual(result.stage, "SENSOR_RECOVERY")
        self.assertFalse(result.automatic_resumed)

    def test_corrupt_latest_slot_falls_back_to_previous_valid_slot(self):
        storage = self.make_saved()
        cfg = storage.load(); cfg.phase_id = 2
        storage.save(cfg)
        storage.corrupt_slot(storage.active_slot)
        recovered = storage.load()
        self.assertIsNotNone(recovered)
        self.assertEqual(recovered.phase_id, 1)

    def test_interrupted_write_does_not_replace_previous_valid_slot(self):
        storage = self.make_saved()
        old = storage.load()
        candidate = Config(); candidate.phase_id = 4
        self.assertFalse(storage.save(candidate, interrupt_at="after_crc"))
        self.assertEqual(storage.load().phase_id, old.phase_id)

    def test_manual_mode_does_not_restore_forced_output(self):
        storage = DualSlotStorage(); cfg = Config(); cfg.automatic_mode = False; cfg.phase_id = 1; storage.save(cfg)
        result = RecoveryController().boot(storage, SAFE_SENSORS, SAFE_INTERLOCKS)
        self.assertEqual(result.stage, "MANUAL_INTERVENTION_REQUIRED")
        self.assertFalse(result.automatic_resumed)
        self.assertFalse(result.outputs_enabled)

    def test_persisted_fault_latch_survives_reboot(self):
        storage = DualSlotStorage(); cfg = Config(); cfg.estop_latched = True; storage.save(cfg)
        result = RecoveryController().boot(storage, SAFE_SENSORS, SAFE_INTERLOCKS)
        self.assertEqual(result.stage, "SAFETY_LOCKOUT")


    def test_unsupported_schema_fails_closed_instead_of_using_defaults(self):
        storage = DualSlotStorage()
        payload = Config().to_dict(); payload["schema"] = 99
        raw = json.dumps(payload, sort_keys=True, separators=(",", ":"), allow_nan=False)
        storage.slots["a"] = {"payload": raw, "crc32": zlib.crc32(raw.encode()) & 0xFFFFFFFF, "revision": 0}
        storage.active_slot = "a"
        result = RecoveryController().boot(storage, SAFE_SENSORS, SAFE_INTERLOCKS)
        self.assertEqual(result.stage, "CONFIG_RECOVERY_REQUIRED")
        self.assertIsNone(result.phase_id)

    def test_interrupted_update_after_corrupt_latest_slot_preserves_older_good_slot(self):
        storage = self.make_saved()
        cfg = storage.load(); cfg.phase_id = 2
        storage.save(cfg)  # second slot is newest
        newest = storage.load_with_slot()[1]
        storage.corrupt_slot(newest)
        old = storage.load()
        update = Config(); update.phase_id = 4
        self.assertFalse(storage.save(update, interrupt_at="after_payload"))
        self.assertEqual(storage.load().phase_id, old.phase_id)

    def test_stale_manual_override_is_not_replayed(self):
        storage = DualSlotStorage(); cfg = Config(); cfg.phase_id = 1
        cfg.automatic_mode = True; cfg.manual_override_active = True
        storage.save(cfg)
        result = RecoveryController().boot(storage, SAFE_SENSORS, SAFE_INTERLOCKS, manual_command_expired=False)
        self.assertEqual(result.stage, "MANUAL_INTERVENTION_REQUIRED")
        self.assertFalse(result.outputs_enabled)

    def test_invalid_config_rejected(self):
        with self.assertRaises(ValueError):
            Profile(air_target_c=70).validate()

if __name__ == "__main__": unittest.main()
