"""Portable, deterministic model of SAFFRON-AERO's persistence and boot recovery.

This is a host-side test model, not the ESP32 firmware and not hardware validation.
"""
from __future__ import annotations
from dataclasses import asdict, dataclass, field
import json
import zlib
from typing import Any

SCHEMA_VERSION = 1
PHASES = (
    "Corm Dormancy / Preparation", "Floral Induction", "Flowering",
    "Vegetative Growth", "Daughter-Corm Development", "Dormancy / Cycle Transition",
)

@dataclass
class Profile:
    air_target_c: float = 22.0
    air_min_c: float = 18.0
    air_max_c: float = 26.0
    rh_target_pct: float = 60.0
    rh_min_pct: float = 35.0
    rh_max_pct: float = 80.0
    temp_hysteresis_c: float = 1.0
    rh_hysteresis_pct: float = 5.0
    mist_pulse_ms: int = 2000
    mist_rest_ms: int = 60000

    def validate(self) -> None:
        nums = [self.air_target_c, self.air_min_c, self.air_max_c, self.rh_target_pct,
                self.rh_min_pct, self.rh_max_pct, self.temp_hysteresis_c, self.rh_hysteresis_pct]
        import math
        if not all(isinstance(n, (int, float)) and math.isfinite(n) for n in nums):
            raise ValueError("Profile contains a non-finite or non-numeric value")
        if not (-5 <= self.air_min_c < self.air_max_c <= 50 and self.air_min_c <= self.air_target_c <= self.air_max_c):
            raise ValueError("Air temperature bounds are inconsistent")
        if not (10 <= self.rh_min_pct < self.rh_max_pct <= 100 and self.rh_min_pct <= self.rh_target_pct <= self.rh_max_pct):
            raise ValueError("Relative humidity bounds are inconsistent")
        if not (0 <= self.temp_hysteresis_c <= 10 and 0 <= self.rh_hysteresis_pct <= 40):
            raise ValueError("Hysteresis outside allowed limits")
        if not (100 <= self.mist_pulse_ms <= 120000 and 1000 <= self.mist_rest_ms <= 3600000):
            raise ValueError("Misting pulse/rest outside allowed limits")

@dataclass
class Config:
    schema: int = SCHEMA_VERSION
    revision: int = 0
    phase_id: int = 1
    automatic_mode: bool = True
    manual_override_active: bool = False
    manual_override_until_ms: int = 0
    air_offset_c: float = 0.0
    rh_offset_pct: float = 0.0
    coolant_offset_c: float = 0.0
    peltier_enabled: bool = False
    mist_enabled: bool = False
    estop_latched: bool = False
    profiles: list[Profile] = field(default_factory=lambda: [Profile() for _ in PHASES])

    def validate(self) -> None:
        if self.schema != SCHEMA_VERSION:
            raise ValueError(f"Unsupported schema {self.schema}")
        if not 0 <= self.phase_id < len(PHASES):
            raise ValueError("Invalid cultivation phase")
        if not isinstance(self.automatic_mode, bool):
            raise ValueError("automatic_mode must be boolean")
        if len(self.profiles) != len(PHASES):
            raise ValueError("Exactly six phase profiles are required")
        import math
        for val in (self.air_offset_c, self.rh_offset_pct, self.coolant_offset_c):
            if not math.isfinite(val) or abs(val) > 20:
                raise ValueError("Calibration offset outside allowed limits")
        for profile in self.profiles:
            profile.validate()

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Config":
        profiles = [Profile(**p) for p in data.get("profiles", [])]
        data = dict(data)
        data["profiles"] = profiles
        return cls(**data)


def _canonical(config: Config) -> bytes:
    return json.dumps(config.to_dict(), sort_keys=True, separators=(",", ":"), allow_nan=False).encode()

def _crc(config: Config) -> int:
    return zlib.crc32(_canonical(config)) & 0xFFFFFFFF

class DualSlotStorage:
    """NVS-like model with two independent slots; used only for desktop unit tests."""
    def __init__(self) -> None:
        self.slots: dict[str, dict[str, Any] | None] = {"a": None, "b": None}
        self.active_slot: str | None = None

    def save(self, config: Config, interrupt_at: str | None = None) -> bool:
        candidate = Config.from_dict(config.to_dict())
        candidate.validate()
        current, current_slot = self.load_with_slot()
        candidate.revision = (current.revision if current else 0) + 1
        # Always write to the slot opposite the newest VALID slot, not merely the
        # last slot we attempted to write. This preserves a previous good slot if
        # the newest one was corrupted or an earlier write was interrupted.
        slot = ("b" if current_slot == "a" else "a") if current_slot else "a"
        payload = candidate.to_dict()
        raw = json.dumps(payload, sort_keys=True, separators=(",", ":"), allow_nan=False)
        self.slots[slot] = {"payload": raw}
        if interrupt_at == "after_payload":
            return False
        self.slots[slot]["crc32"] = zlib.crc32(raw.encode()) & 0xFFFFFFFF
        if interrupt_at == "after_crc":
            return False
        self.slots[slot]["revision"] = candidate.revision
        if interrupt_at == "before_commit":
            return False
        self.active_slot = slot
        return True

    def _read_slot(self, name: str) -> Config | None:
        slot = self.slots.get(name)
        if not isinstance(slot, dict) or not all(k in slot for k in ("payload", "crc32", "revision")):
            return None
        raw = slot["payload"]
        if not isinstance(raw, str) or zlib.crc32(raw.encode()) & 0xFFFFFFFF != slot["crc32"]:
            return None
        try:
            config = Config.from_dict(json.loads(raw))
            config.validate()
            if config.revision != slot["revision"]:
                return None
            return config
        except (ValueError, TypeError, KeyError, json.JSONDecodeError):
            return None

    def load_with_slot(self) -> tuple[Config | None, str | None]:
        valid = [(name, self._read_slot(name)) for name in ("a", "b")]
        valid = [(name, config) for name, config in valid if config is not None]
        if not valid:
            return None, None
        name, config = max(valid, key=lambda pair: pair[1].revision)
        return config, name

    def load(self) -> Config | None:
        config, _slot = self.load_with_slot()
        return config

    def corrupt_slot(self, name: str, field: str = "payload") -> None:
        if self.slots.get(name):
            self.slots[name][field] = "CORRUPTED"

@dataclass
class RecoveryResult:
    stage: str
    phase_id: int | None
    phase: str | None
    mode: str
    automatic_resumed: bool
    outputs_enabled: bool = False
    lockout_reason: str = ""
    config_revision: int | None = None
    missing_history: bool = True

class RecoveryController:
    """Deterministic policy model: saved intent is restored separately from output permission."""
    def boot(self, storage: DualSlotStorage, sensors: dict[str, bool], interlocks: dict[str, bool],
             manual_command_expired: bool = True) -> RecoveryResult:
        config = storage.load()
        if config is None:
            return RecoveryResult("CONFIG_RECOVERY_REQUIRED", None, None, "LOCKED", False,
                                  lockout_reason="No valid persistent configuration slot")
        phase = PHASES[config.phase_id]
        if config.estop_latched:
            return RecoveryResult("SAFETY_LOCKOUT", config.phase_id, phase, "LOCKED", False,
                                  lockout_reason="Persisted emergency-stop/fault latch is active", config_revision=config.revision)
        if not config.automatic_mode:
            reason = "Manual mode is not replayed after a restart; operator action is required"
            return RecoveryResult("MANUAL_INTERVENTION_REQUIRED", config.phase_id, phase, "MANUAL", False,
                                  lockout_reason=reason, config_revision=config.revision)
        if config.manual_override_active and not manual_command_expired:
            return RecoveryResult("MANUAL_INTERVENTION_REQUIRED", config.phase_id, phase, "AUTOMATIC", False,
                                  lockout_reason="Manual override cannot be replayed after reboot", config_revision=config.revision)
        required_sensor_flags = ("air_valid", "coolant_valid", "air_fresh", "coolant_fresh")
        invalid_sensor_flags = [name for name in required_sensor_flags if not sensors.get(name, False)]
        if invalid_sensor_flags:
            return RecoveryResult("SENSOR_RECOVERY", config.phase_id, phase, "AUTOMATIC_PENDING", False,
                                  lockout_reason="Fresh valid sensor measurements required: " + ", ".join(invalid_sensor_flags), config_revision=config.revision)
        required_interlocks = ("driver_verified", "estop_clear", "cooling_proof", "mist_proof")
        failed = [name for name in required_interlocks if not interlocks.get(name, False)]
        if failed:
            return RecoveryResult("SAFETY_LOCKOUT", config.phase_id, phase, "AUTOMATIC_PENDING", False,
                                  lockout_reason="Required interlocks failed or were not reported: " + ", ".join(failed), config_revision=config.revision)
        return RecoveryResult("RECOVERY_COMPLETE", config.phase_id, phase, "AUTOMATIC", True,
                              outputs_enabled=False, lockout_reason="Automatic control may run; actuator demand remains independently guarded",
                              config_revision=config.revision)
