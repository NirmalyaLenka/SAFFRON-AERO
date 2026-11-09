#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>
#include "config.h"
#include <math.h>

// SAFFRON-AERO firmware. Outputs are command signals to verified external drivers,
// never direct loads. All outputs are forced LOW before configuration is read.
static constexpr char FW_VERSION[] = "0.1.0";
static constexpr int OLED_W = 128, OLED_H = 64;
static constexpr uint8_t DHT_TYPE = DHT22;
static constexpr size_t PROFILE_COUNT = 6;
static constexpr size_t EVENT_CAPACITY = 24;

enum Phase : uint8_t { CORM_PREPARATION=0, FLORAL_INDUCTION=1, FLOWERING=2,
                        VEGETATIVE_GROWTH=3, DAUGHTER_CORM=4, DORMANCY_TRANSITION=5 };
enum RecoveryStage : uint8_t { BOOT_INIT, LOAD_PERSISTED_STATE, CONFIG_RECOVERY_REQUIRED,
                               SENSOR_RECOVERY, COOLING_LOOP_CHECK, SAFETY_LOCKOUT,
                               RESTORE_OPERATING_MODE, CONTROL_READY, RECOVERY_COMPLETE,
                               MANUAL_INTERVENTION_REQUIRED };

struct Profile {
  float airTargetC;
  float airMinC;
  float airMaxC;
  float rhTarget;
  float rhMin;
  float rhMax;
  float tempHyst;
  float rhHyst;
  uint16_t mistPulseMs;
  uint32_t mistRestMs;
  bool provisional;
};
struct Config {
  uint16_t schema = CONFIG_SCHEMA_VERSION;
  uint32_t revision = 0;
  Phase phase = FLORAL_INDUCTION;
  bool automaticMode = false;
  bool estopLatched = false;
  // These fields record the commissioning configuration that was reviewed at save time.
  // Saved TRUE never overrides a FALSE compile-time hardware-verification constant.
  bool commissioningRecordAcknowledged = false;
  bool peltierDriverVerified = false;
  bool mistDriverVerified = false;
  bool coolantPumpVerified = false;
  bool hotSideFanVerified = false;
  bool flowSensorVerified = false;
  bool reservoirSensorVerified = false;
  bool hotSideSensorVerified = false;
  bool hardwareEStopCircuitVerified = false;
  // Timed manual requests are persisted only as active metadata, never as an output to replay.
  bool manualCoolingOverrideActive = false;
  bool manualMistOverrideActive = false;
  float hotSideLimitC = 55.0f;
  float airOffsetC = 0.0f;
  float rhOffset = 0.0f;
  float coolantOffsetC = 0.0f;
  float hotSideOffsetC = 0.0f;
  uint32_t manualTimeoutMs = MANUAL_OVERRIDE_MAX_MS;
  Profile profiles[PROFILE_COUNT];
};
struct Measurement {
  float airC = NAN;
  float rh = NAN;
  float coolantC = NAN;
  float hotSideC = NAN;
  uint32_t dhtAt = 0;
  uint32_t coolantAt = 0;
  uint32_t hotSideAt = 0;
  bool dhtValid = false;
  bool coolantValid = false;
  bool hotSideValid = false;
};
struct Event { uint32_t ms; String kind; String detail; };

WebServer server(80);
Preferences nvs;
DHT dht(Pins::DHT22_DATA, DHT_TYPE);
OneWire oneWire(Pins::DS18B20_DATA);
DallasTemperature ds18(&oneWire);
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);
Config cfg;
Measurement meas;
Event events[EVENT_CAPACITY];
size_t eventHead = 0, eventCount = 0;
uint32_t bootAt = 0, lastSampleAt = 0, lastDsRequestAt = 0, lastOledAt = 0;
uint32_t lastMistStart = 0, lastMistStop = 0, coolingSince = 0, lastCoolingStop = 0;
uint32_t manualCoolingUntil = 0, manualMistUntil = 0;
bool configLoaded = false, configRecoveryRequired = true, storageOkay = false, fsOkay = false, oledOkay = false;
bool coolingCommand = false, mistCommand = false, pumpCommand = false, fanCommand = false;
bool coolingDemand = false, mistDemand = false, flowOkay = false, reservoirOkay = false;
bool bootSensorsPassed = false, recoveryFinished = false, recoveryPumpStarted = false;
// Runtime-only lockout also holds if the NVS write for a newly detected fault fails.
// Persistent protection across a total restart still requires a working saved latch and hardware protection.
bool runtimeSafetyLockout = false;
uint32_t recoveryCheckStartedAt = 0;
RecoveryStage recoveryStage = BOOT_INIT;
String lockoutReason = "Configuration has not been commissioned";
String lastSaveStatus = "NO SAVED CONFIGURATION";
String previousModeLabel = "UNKNOWN";
uint32_t rebootCount = 0;

static const char* phaseName(Phase p) {
  switch (p) {
    case CORM_PREPARATION: return "Corm Dormancy / Preparation";
    case FLORAL_INDUCTION: return "Floral Induction";
    case FLOWERING: return "Flowering";
    case VEGETATIVE_GROWTH: return "Vegetative Growth";
    case DAUGHTER_CORM: return "Daughter-Corm Development";
    default: return "Dormancy / Cycle Transition";
  }
}
static const char* recoveryName(RecoveryStage s) {
  switch (s) {
    case BOOT_INIT: return "HARDWARE_INIT";
    case LOAD_PERSISTED_STATE: return "LOAD_PERSISTED_STATE";
    case CONFIG_RECOVERY_REQUIRED: return "CONFIG_RECOVERY_REQUIRED";
    case SENSOR_RECOVERY: return "SENSOR_RECOVERY";
    case COOLING_LOOP_CHECK: return "COOLING_LOOP_CHECK";
    case SAFETY_LOCKOUT: return "SAFETY_LOCKOUT";
    case RESTORE_OPERATING_MODE: return "RESTORE_OPERATING_MODE";
    case CONTROL_READY: return "CONTROL_READY";
    case RECOVERY_COMPLETE: return "RECOVERY_COMPLETE";
    default: return "MANUAL_INTERVENTION_REQUIRED";
  }
}
static uint32_t crc32Of(const String &s) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < s.length(); ++i) {
    crc ^= static_cast<uint8_t>(s[i]);
    for (uint8_t j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}
static bool finiteBetween(float v, float lo, float hi) { return isfinite(v) && v >= lo && v <= hi; }
static bool validateProfile(const Profile &p) {
  return finiteBetween(p.airTargetC, -5, 45) && finiteBetween(p.airMinC, -10, 40) &&
         finiteBetween(p.airMaxC, -5, 50) && p.airMinC < p.airMaxC &&
         p.airTargetC >= p.airMinC && p.airTargetC <= p.airMaxC &&
         finiteBetween(p.rhTarget, 10, 95) && finiteBetween(p.rhMin, 10, 90) &&
         finiteBetween(p.rhMax, 15, 100) && p.rhMin < p.rhMax &&
         p.rhTarget >= p.rhMin && p.rhTarget <= p.rhMax &&
         finiteBetween(p.tempHyst, 0.2f, 5.0f) && finiteBetween(p.rhHyst, 1.0f, 15.0f) &&
         p.mistPulseMs <= MIST_MAX_CONTINUOUS_MS && p.mistRestMs >= 10000UL && p.mistRestMs <= 86400000UL;
}
static void addEvent(const String &kind, const String &detail) {
  events[eventHead] = { millis() - bootAt, kind, detail };
  eventHead = (eventHead + 1) % EVENT_CAPACITY;
  if (eventCount < EVENT_CAPACITY) ++eventCount;
  Serial.printf("[%lu ms] %s: %s\n", (unsigned long)(millis() - bootAt), kind.c_str(), detail.c_str());
  if (fsOkay) {
    if (LittleFS.exists("/events.csv")) {
      File oldFile = LittleFS.open("/events.csv", "r");
      size_t oldSize = oldFile ? oldFile.size() : 0;
      if (oldFile) oldFile.close();
      if (oldSize > 24000) {
        LittleFS.remove("/events.previous.csv");
        LittleFS.rename("/events.csv", "/events.previous.csv");
      }
    }
    bool needHeader = !LittleFS.exists("/events.csv");
    File f = LittleFS.open("/events.csv", "a");
    if (f) {
      if (needHeader) f.println("boot_relative_ms,event,detail");
      String safe = detail; safe.replace("\"", "'"); safe.replace("\r", " "); safe.replace("\n", " ");
      f.printf("%lu,\"%s\",\"%s\"\n", (unsigned long)(millis()-bootAt), kind.c_str(), safe.c_str());
      f.close();
    }
  }
}
static void setPeltierPins(uint8_t level) {
  if (Pins::PELTIER_1_ENABLE >= 0) digitalWrite(Pins::PELTIER_1_ENABLE, level);
  if (Pins::PELTIER_2_ENABLE >= 0) digitalWrite(Pins::PELTIER_2_ENABLE, level);
}
static void setMistPin(uint8_t level) {
  if (Pins::MIST_ENABLE >= 0) digitalWrite(Pins::MIST_ENABLE, level);
}
static void setOutputsSafe() {
  setPeltierPins(LOW); setMistPin(LOW);
  if (Pins::COOLANT_PUMP_ENABLE >= 0) digitalWrite(Pins::COOLANT_PUMP_ENABLE, LOW);
  if (Pins::HOT_SIDE_FAN_ENABLE >= 0) digitalWrite(Pins::HOT_SIDE_FAN_ENABLE, LOW);
  coolingCommand = mistCommand = pumpCommand = fanCommand = false;
}
static Profile defaultProfile(float target, float minC, float maxC, float rhTarget, bool provisional=true) {
  Profile p{target, minC, maxC, rhTarget, 35.0f, 80.0f, 1.0f, 5.0f, 2000, 60000UL, provisional};
  return p;
}
static Config freshCommissioningConfig() {
  Config c;
  c.schema = CONFIG_SCHEMA_VERSION;
  c.revision = 0;
  c.phase = FLORAL_INDUCTION;
  c.automaticMode = true;
  c.hotSideLimitC = Commissioning::MAX_HOT_SIDE_C;
  c.peltierDriverVerified = Commissioning::PELTIER_DRIVER_VERIFIED;
  c.mistDriverVerified = Commissioning::MIST_DRIVER_VERIFIED;
  c.coolantPumpVerified = Commissioning::COOLANT_PUMP_VERIFIED;
  c.hotSideFanVerified = Commissioning::HOT_SIDE_FAN_VERIFIED;
  c.flowSensorVerified = Commissioning::FLOW_SENSOR_VERIFIED;
  c.reservoirSensorVerified = Commissioning::RESERVOIR_SENSOR_VERIFIED;
  c.hotSideSensorVerified = Commissioning::HOT_SIDE_SENSOR_VERIFIED;
  c.hardwareEStopCircuitVerified = Commissioning::HARDWARE_ESTOP_CIRCUIT_VERIFIED;
  c.profiles[CORM_PREPARATION] = defaultProfile(25, 20, 30, 60);
  c.profiles[FLORAL_INDUCTION] = defaultProfile(22, 18, 26, 60);
  c.profiles[FLOWERING] = defaultProfile(17, 15, 20, 60);
  c.profiles[VEGETATIVE_GROWTH] = defaultProfile(17, 10, 20, 60);
  c.profiles[DAUGHTER_CORM] = defaultProfile(20, 15, 27, 60);
  c.profiles[DORMANCY_TRANSITION] = defaultProfile(20, 15, 27, 60);
  return c;
}
static bool validateConfig(const Config &c) {
  if (c.schema != CONFIG_SCHEMA_VERSION || c.phase >= PROFILE_COUNT ||
      !finiteBetween(c.airOffsetC, -5, 5) || !finiteBetween(c.rhOffset, -15, 15) ||
      !finiteBetween(c.coolantOffsetC, -5, 5) || !finiteBetween(c.hotSideOffsetC, -5, 5) ||
      !finiteBetween(c.hotSideLimitC, 25.0f, 100.0f) ||
      c.hotSideLimitC > Commissioning::MAX_HOT_SIDE_C || c.manualTimeoutMs < 1000UL ||
      c.manualTimeoutMs > 600000UL) return false;
  for (size_t i=0;i<PROFILE_COUNT;i++) if (!validateProfile(c.profiles[i])) return false;
  return true;
}
static void configToJson(const Config &c, JsonDocument &doc) {
  doc.clear();
  doc["schema"] = c.schema; doc["revision"] = c.revision;
  doc["phase"] = static_cast<uint8_t>(c.phase); doc["automaticMode"] = c.automaticMode;
  doc["estopLatched"] = c.estopLatched;
  doc["commissioningRecordAcknowledged"] = c.commissioningRecordAcknowledged;
  doc["peltierDriverVerified"] = c.peltierDriverVerified;
  doc["mistDriverVerified"] = c.mistDriverVerified;
  doc["coolantPumpVerified"] = c.coolantPumpVerified;
  doc["hotSideFanVerified"] = c.hotSideFanVerified;
  doc["flowSensorVerified"] = c.flowSensorVerified;
  doc["reservoirSensorVerified"] = c.reservoirSensorVerified;
  doc["hotSideSensorVerified"] = c.hotSideSensorVerified;
  doc["hardwareEStopCircuitVerified"] = c.hardwareEStopCircuitVerified;
  doc["manualCoolingOverrideActive"] = c.manualCoolingOverrideActive;
  doc["manualMistOverrideActive"] = c.manualMistOverrideActive;
  doc["hotSideLimitC"] = c.hotSideLimitC;
  doc["airOffsetC"] = c.airOffsetC;
  doc["rhOffset"] = c.rhOffset; doc["coolantOffsetC"] = c.coolantOffsetC;
  doc["hotSideOffsetC"] = c.hotSideOffsetC;
  doc["manualTimeoutMs"] = c.manualTimeoutMs;
  JsonArray arr = doc["profiles"].to<JsonArray>();
  for (size_t i=0;i<PROFILE_COUNT;i++) {
    JsonObject p = arr.add<JsonObject>(); const Profile &v = c.profiles[i];
    p["airTargetC"] = v.airTargetC; p["airMinC"] = v.airMinC; p["airMaxC"] = v.airMaxC;
    p["rhTarget"] = v.rhTarget; p["rhMin"] = v.rhMin; p["rhMax"] = v.rhMax;
    p["tempHyst"] = v.tempHyst; p["rhHyst"] = v.rhHyst; p["mistPulseMs"] = v.mistPulseMs;
    p["mistRestMs"] = v.mistRestMs; p["provisional"] = v.provisional;
  }
}
static bool jsonToConfig(JsonDocument &doc, Config &out) {
  if (!doc["schema"].is<uint16_t>() || !doc["phase"].is<uint8_t>() || !doc["profiles"].is<JsonArray>()) return false;
  out.schema = doc["schema"].as<uint16_t>(); out.revision = doc["revision"] | 0UL;
  out.phase = static_cast<Phase>(doc["phase"].as<uint8_t>());
  out.automaticMode = doc["automaticMode"] | false; out.estopLatched = doc["estopLatched"] | false;
  // Optional field reads are fail-safe for older schema-1 snapshots: absent safety flags
  // become FALSE and cannot unlock a high-current pathway.
  out.commissioningRecordAcknowledged = doc["commissioningRecordAcknowledged"] | false;
  out.peltierDriverVerified = doc["peltierDriverVerified"] | false;
  out.mistDriverVerified = doc["mistDriverVerified"] | false;
  out.coolantPumpVerified = doc["coolantPumpVerified"] | false;
  out.hotSideFanVerified = doc["hotSideFanVerified"] | false;
  out.flowSensorVerified = doc["flowSensorVerified"] | false;
  out.reservoirSensorVerified = doc["reservoirSensorVerified"] | false;
  out.hotSideSensorVerified = doc["hotSideSensorVerified"] | false;
  out.hardwareEStopCircuitVerified = doc["hardwareEStopCircuitVerified"] | false;
  out.manualCoolingOverrideActive = doc["manualCoolingOverrideActive"] | false;
  out.manualMistOverrideActive = doc["manualMistOverrideActive"] | false;
  out.hotSideLimitC = doc["hotSideLimitC"] | Commissioning::MAX_HOT_SIDE_C;
  out.airOffsetC = doc["airOffsetC"] | 0.0f; out.rhOffset = doc["rhOffset"] | 0.0f;
  out.coolantOffsetC = doc["coolantOffsetC"] | 0.0f; out.hotSideOffsetC = doc["hotSideOffsetC"] | 0.0f;
  out.manualTimeoutMs = doc["manualTimeoutMs"] | MANUAL_OVERRIDE_MAX_MS;
  JsonArray arr = doc["profiles"].as<JsonArray>(); if (arr.size() != PROFILE_COUNT) return false;
  for (size_t i=0;i<PROFILE_COUNT;i++) {
    JsonObject p = arr[i].as<JsonObject>(); Profile &v=out.profiles[i];
    if (p.isNull()) return false;
    v.airTargetC=p["airTargetC"]|NAN; v.airMinC=p["airMinC"]|NAN; v.airMaxC=p["airMaxC"]|NAN;
    v.rhTarget=p["rhTarget"]|NAN; v.rhMin=p["rhMin"]|NAN; v.rhMax=p["rhMax"]|NAN;
    v.tempHyst=p["tempHyst"]|NAN; v.rhHyst=p["rhHyst"]|NAN;
    v.mistPulseMs=p["mistPulseMs"]|0; v.mistRestMs=p["mistRestMs"]|0UL;
    v.provisional=p["provisional"]|true;
  }
  return validateConfig(out);
}
static bool readSlot(char slot, Config &candidate) {
  if (!nvs.begin("saffron", true)) return false;
  String pKey = String(slot) + "_payload"; String cKey = String(slot) + "_crc"; String rKey = String(slot) + "_rev";
  bool has = nvs.isKey(pKey.c_str()) && nvs.isKey(cKey.c_str()) && nvs.isKey(rKey.c_str());
  String payload = has ? nvs.getString(pKey.c_str(), "") : "";
  uint32_t crc = has ? nvs.getUInt(cKey.c_str(), 0) : 0;
  uint32_t rev = has ? nvs.getUInt(rKey.c_str(), 0) : 0;
  nvs.end();
  if (!has || payload.length() == 0 || crc32Of(payload) != crc) return false;
  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, payload) || !jsonToConfig(doc, candidate)) return false;
  if (candidate.revision != rev) return false;
  return true;
}
static bool loadConfig() {
  Config a,b; bool va=readSlot('a',a), vb=readSlot('b',b);
  if (!va && !vb) { configLoaded=false; configRecoveryRequired=true; storageOkay=true; cfg=freshCommissioningConfig(); return false; }
  cfg = (va && vb) ? ((a.revision >= b.revision) ? a : b) : (va ? a : b);
  configLoaded=true; configRecoveryRequired=false; storageOkay=true;
  previousModeLabel = cfg.automaticMode ? "AUTOMATIC" : "MANUAL";
  lastSaveStatus = "LOADED REVISION " + String(cfg.revision);
  addEvent("CONFIG_LOAD", "Loaded validated configuration revision " + String(cfg.revision));
  // A power cut invalidates monotonic timer deadlines. Persist the expired metadata before
  // control is considered ready; never reconstruct or replay a forced actuator command.
  if (cfg.manualCoolingOverrideActive || cfg.manualMistOverrideActive) {
    Config next=cfg; next.manualCoolingOverrideActive=false; next.manualMistOverrideActive=false;
    manualCoolingUntil=manualMistUntil=0;
    addEvent("MANUAL_OVERRIDE_EXPIRED_AT_BOOT", "Saved manual override metadata cleared; forced outputs will not be replayed");
    if (!saveConfig(next, "Expired manual overrides cleared during boot recovery")) {
      // Clear runtime metadata even if storage is degraded. Timer deadlines remain zero,
      // and startup output gates still require a fresh recovery sequence.
      cfg.manualCoolingOverrideActive=false; cfg.manualMistOverrideActive=false;
      addEvent("CONFIG_RECOVERY_WARNING", "Could not persist manual-override expiry; outputs remain safe and metadata will be cleared again on next boot");
    }
  }
  runtimeSafetyLockout = cfg.estopLatched;
  if (cfg.estopLatched) { recoveryStage=SAFETY_LOCKOUT; lockoutReason="Persistent emergency-stop / safety fault is latched"; }
  return true;
}
static bool saveConfig(Config next, const String &reason) {
  if (!validateConfig(next)) { lastSaveStatus="REJECTED: VALIDATION FAILED"; return false; }
  next.schema=CONFIG_SCHEMA_VERSION; next.revision=cfg.revision+1;
  DynamicJsonDocument doc(8192); configToJson(next, doc);
  String payload; if (serializeJson(doc,payload)==0 || payload.length()==0) { lastSaveStatus="FAILED: SERIALIZATION"; return false; }
  char slot = (next.revision % 2 == 0) ? 'a' : 'b';
  String pKey=String(slot)+"_payload", cKey=String(slot)+"_crc", rKey=String(slot)+"_rev";
  if (!nvs.begin("saffron", false)) { lastSaveStatus="FAILED: NVS OPEN"; return false; }
  size_t written = nvs.putString(pKey.c_str(), payload);
  String verifyPayload=nvs.getString(pKey.c_str(), "");
  bool okay = written == payload.length() && verifyPayload == payload;
  if (okay) okay = nvs.putUInt(cKey.c_str(), crc32Of(payload)) == sizeof(uint32_t);
  if (okay) okay = nvs.putUInt(rKey.c_str(), next.revision) == sizeof(uint32_t);
  nvs.end();
  if (!okay) { lastSaveStatus="FAILED: WRITE/READBACK; prior slot retained"; addEvent("CONFIG_SAVE_FAIL", lastSaveStatus); return false; }
  Config check; if (!readSlot(slot, check) || check.revision != next.revision) { lastSaveStatus="FAILED: POST-WRITE VALIDATION"; return false; }
  cfg=next; configLoaded=true; configRecoveryRequired=false; storageOkay=true;
  lastSaveStatus="SAVED REVISION " + String(cfg.revision);
  addEvent("CONFIG_SAVE", reason + " (revision " + String(cfg.revision) + ")");
  return true;
}
static bool sensorsFresh() {
  uint32_t now=millis();
  bool a=meas.dhtValid && now-meas.dhtAt <= SENSOR_STALE_MS;
  bool c=meas.coolantValid && now-meas.coolantAt <= SENSOR_STALE_MS;
  return a && c;
}
static bool coreInterlocksOkay(String &why) {
  if (configRecoveryRequired || !configLoaded) { why="No valid persisted configuration; commissioning required"; return false; }
  if (!sensorsFresh()) { why="Air or DS18B20 sensor data is missing, invalid, or stale"; return false; }
  if (cfg.estopLatched) { why="Persistent emergency-stop / safety fault is latched"; return false; }
  return true;
}
static bool coolingHardwareConfigured(String &why) {
  if (!cfg.commissioningRecordAcknowledged) { why="Commissioning record has not been explicitly reviewed and saved"; return false; }
  if (Pins::PELTIER_1_ENABLE < 0 || Pins::PELTIER_2_ENABLE < 0) { why="Peltier command GPIOs are disabled in config.h until the driver topology and safe polarity are verified"; return false; }
  if (!Commissioning::PELTIER_DRIVER_VERIFIED || !cfg.peltierDriverVerified) { why="Peltier driver is not verified in both firmware and persisted commissioning record"; return false; }
  if (!Commissioning::COOLANT_PUMP_VERIFIED || !cfg.coolantPumpVerified || Pins::COOLANT_PUMP_ENABLE < 0) { why="Verified coolant pump output is not configured"; return false; }
  if (!Commissioning::HOT_SIDE_FAN_VERIFIED || !cfg.hotSideFanVerified || Pins::HOT_SIDE_FAN_ENABLE < 0) { why="Verified hot-side fan output is not configured"; return false; }
  if (!Commissioning::FLOW_SENSOR_VERIFIED || !cfg.flowSensorVerified || Pins::FLOW_SWITCH_INPUT < 0) { why="Verified coolant-flow feedback is not configured"; return false; }
  if (!Commissioning::HOT_SIDE_SENSOR_VERIFIED || !cfg.hotSideSensorVerified || Pins::HOT_SIDE_SENSOR_INDEX < 0) { why="Verified hot-side temperature probe is not configured"; return false; }
  if (Pins::HOT_SIDE_SENSOR_INDEX == 0) { why="Hot-side probe must have a verified sensor index distinct from the coolant-loop probe"; return false; }
  return true;
}
static bool coolingHardwareInterlocksOkay(String &why) {
  if (!coolingHardwareConfigured(why)) return false;
  if (!meas.hotSideValid || millis()-meas.hotSideAt>SENSOR_STALE_MS) { why="Fresh hot-side temperature reading is unavailable"; return false; }
  if (!flowOkay) { why="Coolant flow has not been confirmed"; return false; }
  if (meas.hotSideC >= cfg.hotSideLimitC) { why="Hot-side thermal limit reached"; return false; }
  return true;
}
static bool mistHardwareConfigured(String &why) {
  if (!cfg.commissioningRecordAcknowledged) { why="Commissioning record has not been explicitly reviewed and saved"; return false; }
  if (Pins::MIST_ENABLE < 0) { why="Misting command GPIO is disabled in config.h until the driver topology and safe polarity are verified"; return false; }
  if (!Commissioning::MIST_DRIVER_VERIFIED || !cfg.mistDriverVerified) { why="Misting driver is not verified in both firmware and persisted commissioning record"; return false; }
  if (!Commissioning::RESERVOIR_SENSOR_VERIFIED || !cfg.reservoirSensorVerified || Pins::RESERVOIR_OK_INPUT < 0) { why="Verified reservoir-level feedback is not configured"; return false; }
  return true;
}
static bool latchSafetyFault(const String &reason) {
  runtimeSafetyLockout=true;
  setOutputsSafe();
  if (!cfg.estopLatched && configLoaded) {
    Config next=cfg; next.estopLatched=true; next.automaticMode=false;
    next.manualCoolingOverrideActive=false; next.manualMistOverrideActive=false;
    saveConfig(next, "Safety fault latched: " + reason);
  }
  recoveryStage=SAFETY_LOCKOUT; recoveryFinished=false; lockoutReason=reason;
  addEvent("SAFETY_LOCKOUT", reason);
  return false;
}
static bool mistHardwareInterlocksOkay(String &why) {
  if (!mistHardwareConfigured(why)) return false;
  if (!reservoirOkay) { why="Reservoir-level interlock is not satisfied"; return false; }
  if (!meas.dhtValid || millis()-meas.dhtAt>SENSOR_STALE_MS) { why="Humidity sensor invalid or stale"; return false; }
  return true;
}
static void updateSensors() {
  uint32_t now=millis();
  if (now-lastSampleAt >= SENSOR_SAMPLE_MS) {
    lastSampleAt=now;
    const bool wasDhtValid=meas.dhtValid;
    float rh=dht.readHumidity(), t=dht.readTemperature();
    if (isfinite(t) && t >= -20 && t <= 70 && isfinite(rh) && rh >= 0 && rh <= 100) {
      meas.airC=t+cfg.airOffsetC; meas.rh=rh+cfg.rhOffset; meas.dhtAt=now;
      meas.dhtValid=finiteBetween(meas.airC,-20,70) && finiteBetween(meas.rh,0,100);
    } else { meas.dhtValid=false; }
    if (wasDhtValid != meas.dhtValid) addEvent(meas.dhtValid ? "DHT22_VALID" : "DHT22_INVALID", meas.dhtValid ? "DHT22 returned fresh plausible air temperature/RH" : "DHT22 reading failed plausibility checks; dependent outputs are inhibited");
  }
  if (now-lastDsRequestAt > 1000 && ds18.isConversionComplete()) {
    const bool wasCoolantValid=meas.coolantValid;
    const bool wasHotSideValid=meas.hotSideValid;
    float t=ds18.getTempCByIndex(0);
    if (isfinite(t) && t > -55 && t < 125 && fabsf(t-85.0f)>0.05f && fabsf(t+127.0f)>0.05f) {
      meas.coolantC=t+cfg.coolantOffsetC; meas.coolantAt=now; meas.coolantValid=true;
    } else { meas.coolantValid=false; }
    if (Pins::HOT_SIDE_SENSOR_INDEX >= 0) {
      float h=ds18.getTempCByIndex(Pins::HOT_SIDE_SENSOR_INDEX);
      if (isfinite(h) && h > -55 && h < 125 && fabsf(h-85.0f)>0.05f && fabsf(h+127.0f)>0.05f) {
        meas.hotSideC=h+cfg.hotSideOffsetC; meas.hotSideAt=now; meas.hotSideValid=finiteBetween(meas.hotSideC,-20,140);
      } else meas.hotSideValid=false;
    }
    if (wasCoolantValid != meas.coolantValid) addEvent(meas.coolantValid ? "DS18B20_COOLANT_VALID" : "DS18B20_COOLANT_INVALID", meas.coolantValid ? "Coolant probe returned a plausible reading" : "Coolant probe failed plausibility checks; Peltier outputs are inhibited");
    if (Pins::HOT_SIDE_SENSOR_INDEX >= 0 && wasHotSideValid != meas.hotSideValid) addEvent(meas.hotSideValid ? "DS18B20_HOTSIDE_VALID" : "DS18B20_HOTSIDE_INVALID", meas.hotSideValid ? "Hot-side probe returned a plausible reading" : "Hot-side probe failed checks; Peltier outputs are inhibited");
    ds18.requestTemperatures(); lastDsRequestAt=now;
  }
  flowOkay = Pins::FLOW_SWITCH_INPUT >= 0 && digitalRead(Pins::FLOW_SWITCH_INPUT)==HIGH;
  reservoirOkay = Pins::RESERVOIR_OK_INPUT >= 0 && digitalRead(Pins::RESERVOIR_OK_INPUT)==HIGH;
}
static void updateRecovery() {
  if (configRecoveryRequired || !configLoaded) { recoveryStage=CONFIG_RECOVERY_REQUIRED; lockoutReason="No valid saved configuration; explicit commissioning required"; setOutputsSafe(); return; }
  if (cfg.estopLatched || runtimeSafetyLockout) { recoveryStage=SAFETY_LOCKOUT; lockoutReason=cfg.estopLatched ? "Persistent safety latch requires intentional clearance" : "Runtime safety lockout requires intentional clearance; verify hardware protection and storage health"; setOutputsSafe(); return; }
  if (!sensorsFresh()) {
    recoveryStage=SENSOR_RECOVERY; lockoutReason="Waiting for fresh valid DHT22 and DS18B20 measurements";
    bootSensorsPassed=false; recoveryFinished=false; recoveryPumpStarted=false; setOutputsSafe(); return;
  }
  bootSensorsPassed=true;
  // Recovery is a boot/rehabilitation sequence, not a per-loop pump pulse. Once it
  // has completed, the normal control loop supervises demand and runtime interlocks.
  if (recoveryFinished && recoveryStage == RECOVERY_COMPLETE && cfg.automaticMode) return;
  if (!cfg.automaticMode) {
    if (recoveryStage != MANUAL_INTERVENTION_REQUIRED) {
      recoveryStage=MANUAL_INTERVENTION_REQUIRED;
      recoveryFinished=true;
      setOutputsSafe();
      lockoutReason="Previous mode was manual; forced actuator commands are never restored after reboot";
    } else if (!coolingDemand && !mistDemand) {
      lockoutReason="Manual mode restored; outputs remain off until a fresh timed operator request";
    }
    return;
  }
  String why;
  if (!coolingHardwareConfigured(why)) {
    // A missing/ uncommissioned cooling pathway locks the Peltiers specifically. Do not
    // make otherwise-safe monitoring or an independently verified misting subsystem
    // dependent on a Peltier loop that is intentionally absent from this installation.
    setOutputsSafe(); recoveryPumpStarted=false;
    recoveryStage=RECOVERY_COMPLETE; recoveryFinished=true;
    String mistWhy;
    lockoutReason="Saved automatic mode restored; cooling output remains locked: " + why;
    if (!mistHardwareConfigured(mistWhy)) lockoutReason += "; misting output remains locked: " + mistWhy;
    else if (!reservoirOkay) lockoutReason += "; misting awaits a valid reservoir-level interlock";
    addEvent("RECOVERY_COMPLETE", "Configuration and core sensors restored; each actuator remains independently gated by its commissioning and live interlocks");
    return;
  }
  // A missing misting driver or reservoir sensor keeps misting disabled but does not
  // prevent a verified cooling loop from completing its own startup checks.
  recoveryStage=COOLING_LOOP_CHECK; recoveryFinished=false;
  // Prime the loop and verify flow before allowing any TEC command after a reboot.
  setPeltierPins(LOW); setMistPin(LOW);
  if (!recoveryPumpStarted) {
    digitalWrite(Pins::COOLANT_PUMP_ENABLE,HIGH); digitalWrite(Pins::HOT_SIDE_FAN_ENABLE,HIGH);
    pumpCommand=true; fanCommand=true; recoveryPumpStarted=true; recoveryCheckStartedAt=millis();
    lockoutReason="Priming coolant loop and validating flow / hot-side sensor";
    return;
  }
  const uint32_t elapsed=millis()-recoveryCheckStartedAt;
  if (elapsed < Commissioning::PUMP_PRIME_MS) { lockoutReason="Cooling-loop prime in progress"; return; }
  if (!flowOkay) {
    if (elapsed > Commissioning::PUMP_PRIME_MS + 10000UL) {
      recoveryPumpStarted=false;
      latchSafetyFault("No coolant flow confirmed after startup prime period");
    } else lockoutReason="Waiting for coolant-flow switch to confirm flow";
    return;
  }
  if (!meas.hotSideValid || millis()-meas.hotSideAt>SENSOR_STALE_MS) {
    if (elapsed > Commissioning::PUMP_PRIME_MS + 15000UL) { recoveryPumpStarted=false; latchSafetyFault("Hot-side temperature sensor failed to provide a fresh reading during recovery"); }
    else lockoutReason="Waiting for valid hot-side temperature reading";
    return;
  }
  if (meas.hotSideC>=cfg.hotSideLimitC) {
    recoveryPumpStarted=false; latchSafetyFault("Hot-side temperature at or above configured limit during recovery"); return;
  }
  digitalWrite(Pins::COOLANT_PUMP_ENABLE,LOW); digitalWrite(Pins::HOT_SIDE_FAN_ENABLE,LOW);
  pumpCommand=false; fanCommand=false; recoveryPumpStarted=false;
  recoveryStage=RECOVERY_COMPLETE; recoveryFinished=true;
  String mistWhy;
  if (!mistHardwareConfigured(mistWhy) || !reservoirOkay) {
    lockoutReason = "Automatic cooling recovery passed; misting remains locked: " +
      (mistWhy.length() ? mistWhy : String("reservoir-level interlock is not satisfied"));
  } else {
    lockoutReason="Saved automatic mode restored; startup cooling interlocks passed; misting will be checked independently";
  }
  addEvent("RECOVERY_COMPLETE", "Persisted phase/configuration restored and cooling startup interlocks passed; misting remains independently guarded");
}
static void updateControl() {
  if (configRecoveryRequired || !configLoaded || cfg.estopLatched || runtimeSafetyLockout || !sensorsFresh()) {
    if (recoveryStage != COOLING_LOOP_CHECK || !recoveryPumpStarted) setOutputsSafe();
    coolingDemand=false; mistDemand=false; return;
  }
  const bool explicitManualRequest = recoveryStage==MANUAL_INTERVENTION_REQUIRED && !cfg.automaticMode && (coolingDemand || mistDemand);
  if (recoveryStage != RECOVERY_COMPLETE && recoveryStage != CONTROL_READY && !explicitManualRequest) {
    setPeltierPins(LOW); setMistPin(LOW);
    coolingCommand=false; mistCommand=false;
    // The recovery checker may intentionally run the pump/fan for a short flow test.
    if (recoveryStage != COOLING_LOOP_CHECK || !recoveryPumpStarted) {
      if (Pins::COOLANT_PUMP_ENABLE>=0) digitalWrite(Pins::COOLANT_PUMP_ENABLE,LOW);
      if (Pins::HOT_SIDE_FAN_ENABLE>=0) digitalWrite(Pins::HOT_SIDE_FAN_ENABLE,LOW);
      pumpCommand=false; fanCommand=false;
    }
    return;
  }
  const Profile &p=cfg.profiles[cfg.phase];
  if (cfg.automaticMode) {
    if (!coolingDemand && meas.airC > p.airTargetC + p.tempHyst) coolingDemand=true;
    else if (coolingDemand && meas.airC < p.airTargetC - p.tempHyst) coolingDemand=false;
    if (!mistDemand && meas.rh < p.rhTarget - p.rhHyst) mistDemand=true;
    else if (mistDemand && meas.rh > p.rhTarget + p.rhHyst) mistDemand=false;
  } else {
    const bool coolingExpired = manualCoolingUntil != 0 && static_cast<int32_t>(millis() - manualCoolingUntil) >= 0;
    const bool mistExpired = manualMistUntil != 0 && static_cast<int32_t>(millis() - manualMistUntil) >= 0;
    if (manualCoolingUntil == 0 || coolingExpired) coolingDemand=false;
    if (manualMistUntil == 0 || mistExpired) mistDemand=false;
    if (coolingExpired || mistExpired) {
      Config next=cfg;
      if (coolingExpired) { manualCoolingUntil=0; next.manualCoolingOverrideActive=false; addEvent("MANUAL_COOLING_TIMEOUT", "Timed manual cooling request expired; output demand cleared"); }
      if (mistExpired) { manualMistUntil=0; next.manualMistOverrideActive=false; addEvent("MANUAL_MIST_TIMEOUT", "Timed manual misting request expired; output demand cleared"); }
      if (!saveConfig(next, "Expired manual override metadata cleared")) addEvent("CONFIG_SAVE_WARNING", "Manual timeout output was cleared but override metadata could not be saved");
    }
  }

  String why;
  const bool baseOkay=coreInterlocksOkay(why);
  if (coolingDemand && baseOkay && coolingHardwareConfigured(why)) {
    if (!pumpCommand) {
      pumpCommand=true; fanCommand=true; coolingSince=millis();
      digitalWrite(Pins::COOLANT_PUMP_ENABLE,HIGH); digitalWrite(Pins::HOT_SIDE_FAN_ENABLE,HIGH);
      addEvent("COOLING_LOOP_START", "Pump/fan commanded on before TEC; waiting for flow and temperature proof");
    }
    bool flowAndThermal = coolingHardwareInterlocksOkay(why);
    if (millis()-coolingSince >= Commissioning::PUMP_PRIME_MS && flowAndThermal) {
      if (!coolingCommand && millis()-lastCoolingStop >= COOLING_MIN_OFF_MS) {
        setPeltierPins(HIGH);
        coolingCommand=true; addEvent("COOLING_ON", "Verified driver command enabled; both TECs share one logical on/off demand");
      }
    } else {
      setPeltierPins(LOW); coolingCommand=false;
      if (millis()-coolingSince > Commissioning::PUMP_PRIME_MS + 10000UL && !flowOkay) {
        latchSafetyFault("Coolant flow lost / not proven while cooling was requested"); return;
      }
      if (meas.hotSideValid && meas.hotSideC>=Commissioning::MAX_HOT_SIDE_C) {
        latchSafetyFault("Hot-side temperature reached configured limit while cooling was requested"); return;
      }
    }
  } else {
    if (coolingCommand) addEvent("COOLING_OFF", why.length()?why:"Temperature demand cleared");
    setPeltierPins(LOW);
    if (coolingCommand) lastCoolingStop=millis();
    coolingCommand=false;
    if (!coolingDemand && pumpCommand && millis()-lastCoolingStop >= Commissioning::PUMP_COOLDOWN_MS) {
      digitalWrite(Pins::COOLANT_PUMP_ENABLE,LOW); digitalWrite(Pins::HOT_SIDE_FAN_ENABLE,LOW);
      pumpCommand=false; fanCommand=false;
    }
  }

  bool mistInterlocks = coreInterlocksOkay(why) && mistHardwareInterlocksOkay(why);
  if (mistDemand && mistInterlocks) {
    if (!mistCommand && (millis()-lastMistStop >= p.mistRestMs)) { lastMistStart=millis(); mistCommand=true; setMistPin(HIGH); addEvent("MIST_ON", "Misting output enabled after interlocks"); }
    if (mistCommand && millis()-lastMistStart >= min((uint32_t)p.mistPulseMs,MIST_MAX_CONTINUOUS_MS)) {
      setMistPin(LOW); mistCommand=false; lastMistStop=millis(); addEvent("MIST_OFF", "Configured pulse complete");
    }
  } else if (mistCommand) {
    setMistPin(LOW); mistCommand=false; lastMistStop=millis(); addEvent("MIST_OFF", why.length()?why:"Misting demand cleared");
  }
}
static String currentLockout() {
  // Surface recovery-stage faults even when current demand is zero; otherwise the UI can
  // misleadingly say "no lockout" while automatic recovery is blocked before demand exists.
  if (configRecoveryRequired || !configLoaded || cfg.estopLatched || runtimeSafetyLockout) return lockoutReason;
  if (recoveryStage == SENSOR_RECOVERY || recoveryStage == COOLING_LOOP_CHECK || recoveryStage == SAFETY_LOCKOUT) return lockoutReason;
  if (!cfg.automaticMode) return "Manual mode: forced actuator commands do not restore after reboot";
  if (recoveryStage == RECOVERY_COMPLETE && lockoutReason.indexOf("remains locked") >= 0) return lockoutReason;
  String why;
  if (!coreInterlocksOkay(why)) return why;
  if (coolingDemand && !coolingHardwareInterlocksOkay(why)) return why;
  if (mistDemand && !mistHardwareInterlocksOkay(why)) return why;
  if (!cfg.automaticMode) return "Manual mode: manual outputs do not restore after reboot";
  if (!recoveryFinished) return lockoutReason;
  return "No active lockout; subsystem hardware interlocks still evaluated independently";
}
static void sendJson(int code, JsonDocument &doc) {
  String body; serializeJson(doc,body); server.send(code,"application/json",body);
}
static void sendError(int code, const String &message) {
  DynamicJsonDocument doc(512); doc["ok"]=false; doc["error"]=message; sendJson(code,doc);
}
static bool authorized() {
  if (String(NetworkConfig::API_KEY).startsWith("CHANGE_THIS")) return false;
  return server.header("X-API-Key") == String(NetworkConfig::API_KEY);
}
static bool requireAuth() { if (authorized()) return true; sendError(401,"Missing/invalid X-API-Key. Change the sample key in config.h first."); return false; }
static void statusJson(JsonDocument &doc) {
  doc["ok"]=true; doc["firmwareVersion"]=FW_VERSION; doc["schemaVersion"]=CONFIG_SCHEMA_VERSION;
  doc["phaseId"]=static_cast<uint8_t>(cfg.phase); doc["phase"]=phaseName(cfg.phase);
  doc["automaticMode"]=cfg.automaticMode; doc["mode"]=cfg.automaticMode?"AUTOMATIC":"MANUAL";
  doc["configLoaded"]=configLoaded; doc["configRecoveryRequired"]=configRecoveryRequired;
  doc["configurationRevision"]=cfg.revision; doc["configurationSaveStatus"]=lastSaveStatus;
  doc["recoveryStage"]=recoveryName(recoveryStage); doc["recoveryComplete"]=recoveryFinished;
  doc["previousMode"]=previousModeLabel; doc["lastSavedPhase"]=phaseName(cfg.phase); doc["lockoutReason"]=currentLockout();
  JsonObject commissioning=doc["commissioning"].to<JsonObject>();
  commissioning["recordAcknowledged"]=cfg.commissioningRecordAcknowledged;
  commissioning["peltierDriverVerified"]=cfg.peltierDriverVerified && Commissioning::PELTIER_DRIVER_VERIFIED;
  commissioning["mistDriverVerified"]=cfg.mistDriverVerified && Commissioning::MIST_DRIVER_VERIFIED;
  commissioning["coolantPumpVerified"]=cfg.coolantPumpVerified && Commissioning::COOLANT_PUMP_VERIFIED;
  commissioning["hotSideFanVerified"]=cfg.hotSideFanVerified && Commissioning::HOT_SIDE_FAN_VERIFIED;
  commissioning["flowSensorVerified"]=cfg.flowSensorVerified && Commissioning::FLOW_SENSOR_VERIFIED;
  commissioning["reservoirSensorVerified"]=cfg.reservoirSensorVerified && Commissioning::RESERVOIR_SENSOR_VERIFIED;
  commissioning["hotSideSensorVerified"]=cfg.hotSideSensorVerified && Commissioning::HOT_SIDE_SENSOR_VERIFIED;
  commissioning["hardwareEStopCircuitVerified"]=cfg.hardwareEStopCircuitVerified && Commissioning::HARDWARE_ESTOP_CIRCUIT_VERIFIED;
  commissioning["hotSideLimitC"]=cfg.hotSideLimitC;
  commissioning["manualCoolingOverrideActive"]=cfg.manualCoolingOverrideActive;
  commissioning["manualMistOverrideActive"]=cfg.manualMistOverrideActive;
  doc["bootRelativeMs"]=millis()-bootAt; doc["sensorDataFresh"]=sensorsFresh();
  JsonObject sensors=doc["sensors"].to<JsonObject>();
  if(meas.dhtValid){sensors["airTemperatureC"]=meas.airC;sensors["relativeHumidityPct"]=meas.rh;}else{sensors["airTemperatureC"]=nullptr;sensors["relativeHumidityPct"]=nullptr;}
  if(meas.coolantValid)sensors["coolantTemperatureC"]=meas.coolantC;else sensors["coolantTemperatureC"]=nullptr;
  if(meas.hotSideValid)sensors["hotSideTemperatureC"]=meas.hotSideC;else sensors["hotSideTemperatureC"]=nullptr;
  sensors["dhtValid"]=meas.dhtValid; sensors["ds18b20Valid"]=meas.coolantValid; sensors["hotSideValid"]=meas.hotSideValid;
  sensors["airAgeMs"]=meas.dhtAt?millis()-meas.dhtAt:0xFFFFFFFFUL;
  sensors["coolantAgeMs"]=meas.coolantAt?millis()-meas.coolantAt:0xFFFFFFFFUL;
  sensors["hotSideAgeMs"]=meas.hotSideAt?millis()-meas.hotSideAt:0xFFFFFFFFUL;
  sensors["lightSensor"]=Pins::LIGHT_SENSOR_INPUT<0?"NOT INSTALLED":"CONFIGURED - CALIBRATION REQUIRED";
  JsonObject outs=doc["outputs"].to<JsonObject>();
  outs["peltier1CommandedOn"]=coolingCommand; outs["peltier2CommandedOn"]=coolingCommand;
  outs["coolingDemand"]=coolingDemand; outs["pumpCommandedOn"]=pumpCommand; outs["fanCommandedOn"]=fanCommand;
  outs["mistCommandedOn"]=mistCommand; outs["actualElectricalFeedbackAvailable"]=false;
  JsonObject ints=doc["interlocks"].to<JsonObject>();
  ints["estopLatched"]=cfg.estopLatched; ints["flowFeedbackInstalled"]=Pins::FLOW_SWITCH_INPUT>=0 && Commissioning::FLOW_SENSOR_VERIFIED;
  ints["flowOkay"]=flowOkay; ints["reservoirFeedbackInstalled"]=Pins::RESERVOIR_OK_INPUT>=0 && Commissioning::RESERVOIR_SENSOR_VERIFIED;
  ints["reservoirOkay"]=reservoirOkay; ints["peltierDriverVerified"]=Commissioning::PELTIER_DRIVER_VERIFIED;
  ints["mistDriverVerified"]=Commissioning::MIST_DRIVER_VERIFIED;
  JsonObject p=doc["activeProfile"].to<JsonObject>(); const Profile &prof=cfg.profiles[cfg.phase];
  p["airTargetC"]=prof.airTargetC; p["airMinC"]=prof.airMinC; p["airMaxC"]=prof.airMaxC;
  p["rhTargetPct"]=prof.rhTarget; p["rhMinPct"]=prof.rhMin; p["rhMaxPct"]=prof.rhMax;
  p["tempHysteresisC"]=prof.tempHyst; p["rhHysteresisPct"]=prof.rhHyst; p["provisional"]=prof.provisional;
  JsonArray ev=doc["events"].to<JsonArray>();
  for (size_t i=0;i<eventCount;i++) { size_t idx=(eventHead+EVENT_CAPACITY-eventCount+i)%EVENT_CAPACITY; JsonObject o=ev.add<JsonObject>(); o["bootMs"]=events[idx].ms; o["kind"]=events[idx].kind; o["detail"]=events[idx].detail; }
}
static void handleStatus() { DynamicJsonDocument doc(8192); statusJson(doc); sendJson(200,doc); }
static void handleGetConfig() { DynamicJsonDocument doc(8192); configToJson(cfg,doc); doc["ok"]=true; doc["configLoaded"]=configLoaded; doc["saveStatus"]=lastSaveStatus; sendJson(200,doc); }
static void handleCommission() {
  if (!requireAuth()) return;
  DynamicJsonDocument req(512); if (deserializeJson(req,server.arg("plain")) || req["confirm"]!=true || req["commissioningAcknowledged"]!=true) { sendError(400,"Explicit confirm and commissioningAcknowledged=true are required"); return; }
  // Re-commissioning refreshes the stored snapshot from this firmware build without
  // wiping a valid phase/profile or clearing a persistent safety latch.
  Config next = (configLoaded && !configRecoveryRequired) ? cfg : freshCommissioningConfig();
  next.commissioningRecordAcknowledged=true;
  next.peltierDriverVerified=Commissioning::PELTIER_DRIVER_VERIFIED;
  next.mistDriverVerified=Commissioning::MIST_DRIVER_VERIFIED;
  next.coolantPumpVerified=Commissioning::COOLANT_PUMP_VERIFIED;
  next.hotSideFanVerified=Commissioning::HOT_SIDE_FAN_VERIFIED;
  next.flowSensorVerified=Commissioning::FLOW_SENSOR_VERIFIED;
  next.reservoirSensorVerified=Commissioning::RESERVOIR_SENSOR_VERIFIED;
  next.hotSideSensorVerified=Commissioning::HOT_SIDE_SENSOR_VERIFIED;
  next.hardwareEStopCircuitVerified=Commissioning::HARDWARE_ESTOP_CIRCUIT_VERIFIED;
  next.hotSideLimitC=Commissioning::MAX_HOT_SIDE_C;
  if (!saveConfig(next, configLoaded ? "Commissioning record reviewed/refreshed; existing phase and profiles preserved" : "Explicit first-time commissioning by operator")) { sendError(500,lastSaveStatus); return; }
  recoveryStage=SENSOR_RECOVERY; recoveryFinished=false; recoveryPumpStarted=false;
  addEvent("COMMISSION", "Commissioning record stored. This acknowledgement is not proof of safe wiring; output gates still require firmware flags, configured feedback, and live interlocks.");
  DynamicJsonDocument out(512); out["ok"]=true; out["message"]="Configuration saved; actual high-current outputs remain locked unless compile-time verification flags, configured feedback, and live interlocks all pass"; out["revision"]=cfg.revision; sendJson(200,out);
}
static void handleSetPhase() {
  if (!requireAuth()) return;
  if (configRecoveryRequired || !configLoaded) { sendError(409,"Explicit first-time commissioning is required before changing settings"); return; }
  DynamicJsonDocument req(512); if (deserializeJson(req,server.arg("plain")) || !req["phaseId"].is<uint8_t>()) { sendError(400,"phaseId must be integer 0..5"); return; }
  uint8_t id=req["phaseId"].as<uint8_t>(); if (id>=PROFILE_COUNT) { sendError(422,"phaseId out of range"); return; }
  Config next=cfg; next.phase=static_cast<Phase>(id);
  if (!saveConfig(next,"Phase changed to "+String(phaseName(next.phase)))) { sendError(422,lastSaveStatus); return; }
  addEvent("PHASE_CHANGE", String("Active phase committed: ") + phaseName(next.phase));
  DynamicJsonDocument out(512); out["ok"]=true; out["phaseId"]=id; out["phase"]=phaseName(next.phase); out["revision"]=cfg.revision; sendJson(200,out);
}
static void handleSetMode() {
  if (!requireAuth()) return;
  if (configRecoveryRequired || !configLoaded) { sendError(409,"Explicit first-time commissioning is required before changing settings"); return; }
  DynamicJsonDocument req(512); if (deserializeJson(req,server.arg("plain")) || !req["automaticMode"].is<bool>()) { sendError(400,"automaticMode boolean required"); return; }
  Config next=cfg; next.automaticMode=req["automaticMode"].as<bool>();
  // Any mode transition discards timed manual requests and starts with outputs off.
  next.manualCoolingOverrideActive=false; next.manualMistOverrideActive=false;
  manualCoolingUntil=manualMistUntil=0; coolingDemand=mistDemand=false; setOutputsSafe();
  if (!saveConfig(next,String("Operating mode set to ")+(next.automaticMode?"AUTOMATIC":"MANUAL"))) { sendError(500,lastSaveStatus); return; }
  if (cfg.automaticMode) {
    recoveryStage=SENSOR_RECOVERY; recoveryFinished=false; recoveryPumpStarted=false;
    lockoutReason="Automatic mode committed; validating fresh measurements and actuator-specific interlocks";
  } else {
    recoveryStage=MANUAL_INTERVENTION_REQUIRED; recoveryFinished=true;
    lockoutReason="Manual mode committed; all outputs start off and require a fresh timed request";
  }
  addEvent("MODE_CHANGE", String("Committed mode: ") + (next.automaticMode ? "AUTOMATIC" : "MANUAL"));
  DynamicJsonDocument out(512); out["ok"]=true; out["mode"]=cfg.automaticMode?"AUTOMATIC":"MANUAL"; out["revision"]=cfg.revision; sendJson(200,out);
}
static void handleUpdateProfile() {
  if (!requireAuth()) return;
  if (configRecoveryRequired || !configLoaded) { sendError(409,"Explicit first-time commissioning is required before changing settings"); return; }
  DynamicJsonDocument req(2048); if (deserializeJson(req,server.arg("plain")) || !req["phaseId"].is<uint8_t>() || !req["profile"].is<JsonObject>()) { sendError(400,"phaseId and profile object required"); return; }
  uint8_t id=req["phaseId"].as<uint8_t>(); if (id>=PROFILE_COUNT) { sendError(422,"phaseId out of range"); return; }
  Config next=cfg; JsonObject p=req["profile"].as<JsonObject>(); Profile &v=next.profiles[id];
  v.airTargetC=p["airTargetC"]|NAN; v.airMinC=p["airMinC"]|NAN; v.airMaxC=p["airMaxC"]|NAN;
  v.rhTarget=p["rhTarget"]|NAN; v.rhMin=p["rhMin"]|NAN; v.rhMax=p["rhMax"]|NAN;
  v.tempHyst=p["tempHyst"]|NAN; v.rhHyst=p["rhHyst"]|NAN;
  v.mistPulseMs=p["mistPulseMs"]|0; v.mistRestMs=p["mistRestMs"]|0UL; v.provisional=true;
  if (!validateProfile(v)) { sendError(422,"Profile failed numeric/range validation"); return; }
  if (!saveConfig(next,"Profile updated for phase "+String(phaseName(static_cast<Phase>(id))))) { sendError(500,lastSaveStatus); return; }
  addEvent("PROFILE_CHANGE", String("Committed provisional profile for ") + phaseName(static_cast<Phase>(id)));
  DynamicJsonDocument out(512); out["ok"]=true; out["revision"]=cfg.revision; out["message"]="Profile committed; values remain research-provisional until experimentally calibrated"; sendJson(200,out);
}
static void handleManualOutput(bool cooling) {
  if (!requireAuth()) return;
  DynamicJsonDocument req(512); if (deserializeJson(req,server.arg("plain")) || !req["enabled"].is<bool>()) { sendError(400,"enabled boolean required"); return; }
  if (cfg.automaticMode) { sendError(409,"Switch to MANUAL mode before issuing timed override"); return; }
  if (!req["enabled"].as<bool>()) {
    // De-energize first; a storage failure must never keep a requested output energized.
    if(cooling){manualCoolingUntil=0;coolingDemand=false;}else{manualMistUntil=0;mistDemand=false;}
    updateControl();
    Config next=cfg;
    if(cooling) next.manualCoolingOverrideActive=false; else next.manualMistOverrideActive=false;
    if(!saveConfig(next, cooling ? "Timed manual cooling request cancelled" : "Timed manual misting request cancelled")) { sendError(500,"Output request cancelled, but override metadata could not be saved: "+lastSaveStatus); return; }
    addEvent(cooling ? "MANUAL_COOLING_OFF" : "MANUAL_MIST_OFF", "Operator cancelled timed manual request");
  } else {
    uint32_t duration=req["durationMs"]|30000UL; if (duration<1000 || duration>cfg.manualTimeoutMs) { sendError(422,"durationMs must be at least 1000 ms and no greater than the saved manualTimeoutMs limit"); return; }
    String why;
    bool safe=coreInterlocksOkay(why) && (cooling?coolingHardwareConfigured(why):mistHardwareInterlocksOkay(why));
    if (!safe) { sendError(423,why); return; }
    Config next=cfg;
    if(cooling) next.manualCoolingOverrideActive=true; else next.manualMistOverrideActive=true;
    if(!saveConfig(next, cooling ? "Timed manual cooling override metadata saved" : "Timed manual misting override metadata saved")) { sendError(500,"Override was not activated because metadata could not be saved: "+lastSaveStatus); return; }
    if (cooling) { coolingDemand=true; manualCoolingUntil=millis()+duration; }
    else { mistDemand=true; manualMistUntil=millis()+duration; }
    addEvent(cooling ? "MANUAL_COOLING_ON_REQUEST" : "MANUAL_MIST_ON_REQUEST", "Timed manual request accepted; guarded control still evaluates interlocks");
    if (recoveryStage==MANUAL_INTERVENTION_REQUIRED) recoveryFinished=false;
  }
  DynamicJsonDocument out(512); out["ok"]=true; out["requestAccepted"]=true; out["note"]="This is a requested GPIO command, not proof of electrical output"; sendJson(200,out);
}
static void handleEStop() {
  if (!requireAuth()) return;
  runtimeSafetyLockout=true;
  recoveryStage=SAFETY_LOCKOUT; recoveryFinished=false; lockoutReason="Emergency stop latched; intentional clearance required";
  Config next=cfg; next.estopLatched=true; next.automaticMode=false;
  next.manualCoolingOverrideActive=false; next.manualMistOverrideActive=false;
  manualCoolingUntil=manualMistUntil=0; coolingDemand=mistDemand=false; setOutputsSafe();
  if (!saveConfig(next,"Emergency stop latched")) { sendError(500,"Outputs forced LOW, but persisting the E-stop failed: "+lastSaveStatus); return; }
  recoveryStage=SAFETY_LOCKOUT; recoveryFinished=false; lockoutReason="Emergency stop latched; intentional clearance required";
  addEvent("ESTOP_LATCHED", "Software emergency stop requested; configured GPIO commands forced LOW");
  DynamicJsonDocument out(512); out["ok"]=true; out["estopLatched"]=true; sendJson(200,out);
}
static void handleFactoryReset() {
  if (!requireAuth()) return;
  if (cfg.estopLatched || runtimeSafetyLockout) { sendError(423,"Clear the latched safety fault through the verified clearance procedure before factory reset"); return; }
  DynamicJsonDocument req(512);
  if (deserializeJson(req,server.arg("plain")) || req["confirm"]!=true || req["phrase"]!="RESET_SAFFRON_AERO") { sendError(400,"confirm=true and phrase RESET_SAFFRON_AERO are required"); return; }
  setOutputsSafe();
  if (!nvs.begin("saffron", false)) { sendError(500,"NVS could not be opened"); return; }
  bool cleared=nvs.clear(); nvs.end();
  if (!cleared) { sendError(500,"NVS clear failed; outputs remain off"); return; }
  cfg=freshCommissioningConfig(); configLoaded=false; configRecoveryRequired=true; recoveryFinished=false;
  recoveryStage=CONFIG_RECOVERY_REQUIRED; lockoutReason="Factory reset requested; explicit commissioning required";
  addEvent("FACTORY_RESET", "Configuration cleared by explicit operator action");
  DynamicJsonDocument out(512); out["ok"]=true; out["message"]="Settings cleared; outputs remain off until commissioning"; sendJson(200,out);
}
static void handleClearFault() {
  if (!requireAuth()) return;
  DynamicJsonDocument req(512); if (deserializeJson(req,server.arg("plain")) || req["confirm"]!=true) { sendError(400,"confirm=true required"); return; }
  if (!sensorsFresh()) { sendError(423,"Cannot clear lockout while sensor readings are invalid/stale"); return; }
  // Firmware/API cannot verify wiring, fuses, driver topology or a physical E-stop circuit.
  // Require those manual checks before the operator is allowed to clear software latch.
  if (!Commissioning::HARDWARE_ESTOP_CIRCUIT_VERIFIED || !cfg.hardwareEStopCircuitVerified || !cfg.commissioningRecordAcknowledged) { sendError(423,"Hardware E-stop circuit is not verified in both firmware and saved commissioning record; inspect hardware and recommission before clearing"); return; }
  Config next=cfg; next.estopLatched=false; next.automaticMode=true;
  next.manualCoolingOverrideActive=false; next.manualMistOverrideActive=false;
  manualCoolingUntil=manualMistUntil=0;
  if (!saveConfig(next,"Operator cleared latched fault after explicit confirmation")) { sendError(500,lastSaveStatus); return; }
  runtimeSafetyLockout=false;
  recoveryStage=SENSOR_RECOVERY; recoveryFinished=false;
  DynamicJsonDocument out(512); out["ok"]=true; out["message"]="Latch cleared; recovery and interlocks will run again"; sendJson(200,out);
}
static void handleEvents() { DynamicJsonDocument doc(4096); statusJson(doc); JsonVariant ev=doc["events"]; DynamicJsonDocument out(4096); out["ok"]=true; out["events"]=ev; sendJson(200,out); }
static void handleEventsCsv() {
  if (!fsOkay || !LittleFS.exists("/events.csv")) { sendError(404,"Persistent event log is not available"); return; }
  File f=LittleFS.open("/events.csv","r"); if(!f){sendError(500,"Could not open persistent event log");return;}
  server.streamFile(f,"text/csv"); f.close();
}
static void handleDiagnostics() {
  DynamicJsonDocument doc(2048); doc["ok"]=true; doc["firmwareVersion"]=FW_VERSION; doc["uptimeMs"]=millis()-bootAt;
  doc["storageOkay"]=storageOkay; doc["configLoaded"]=configLoaded; doc["revision"]=cfg.revision; doc["schemaVersion"]=CONFIG_SCHEMA_VERSION;
  doc["recoveryStage"]=recoveryName(recoveryStage); doc["lockoutReason"]=currentLockout();
  doc["wifiMode"]="AP enabled; STA is optional"; doc["apIp"]=WiFi.softAPIP().toString(); doc["stationConnected"]=WiFi.status()==WL_CONNECTED;
  doc["peltierDriverVerified"]=cfg.peltierDriverVerified && Commissioning::PELTIER_DRIVER_VERIFIED; doc["mistDriverVerified"]=cfg.mistDriverVerified && Commissioning::MIST_DRIVER_VERIFIED;
  doc["hotSideLimitC"]=cfg.hotSideLimitC; doc["commissioningRecordAcknowledged"]=cfg.commissioningRecordAcknowledged;
  doc["flowConfigured"]=Pins::FLOW_SWITCH_INPUT>=0; doc["hotSideSensorConfigured"]=Pins::HOT_SIDE_SENSOR_INDEX>=0;
  doc["reservoirConfigured"]=Pins::RESERVOIR_OK_INPUT>=0; sendJson(200,doc);
}
static const char FALLBACK_HTML[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>SAFFRON-AERO</title><style>body{font-family:system-ui;background:#101a19;color:#e9f0eb;max-width:850px;margin:auto;padding:1rem}section{background:#1b2926;border:1px solid #40534c;border-radius:12px;padding:1rem;margin:.7rem 0}button,select,input{padding:.65rem;margin:.25rem;background:#d4a85d;color:#16201d;border:0;border-radius:6px}small{color:#b8c8c0}.metric{font-size:1.7rem;font-weight:700}</style></head><body><h1>SAFFRON-AERO</h1><p>Offline ESP32 controller · local fallback UI · no cloud required</p><section><h2>Local access</h2><label>API key from your private config.h <input id="apikey" type="password" autocomplete="current-password" placeholder="Enter your API key"></label><button onclick="saveKey()">Use key for this tab</button><p><small>The key is not embedded in this page. Do not share it or expose this local controller to the public internet.</small></p></section><section><h2>Live status</h2><div id="status">Waiting for controller data…</div></section><section><h2>Phase and mode</h2><select id="phase"><option value="0">Corm Dormancy / Preparation</option><option value="1">Floral Induction</option><option value="2">Flowering</option><option value="3">Vegetative Growth</option><option value="4">Daughter-Corm Development</option><option value="5">Dormancy / Cycle Transition</option></select><button onclick="setPhase()">Commit phase</button><button onclick="setMode(true)">Automatic</button><button onclick="setMode(false)">Manual / outputs off</button><button id="commission" onclick="commission()" hidden>Commission base configuration</button><button onclick="estop()">EMERGENCY STOP</button><p><small>Commissioning only stores provisional settings; it does not verify or unlock the power electronics. Physical outputs are locked by default.</small></p></section><script>const phaseNames=['Corm Dormancy / Preparation','Floral Induction','Flowering','Vegetative Growth','Daughter-Corm Development','Dormancy / Cycle Transition'];const apiKey=()=>sessionStorage.getItem('saffronApiKey')||'';function saveKey(){const v=document.getElementById('apikey').value.trim();if(!v){alert('Enter the random API key configured in firmware config.h');return}sessionStorage.setItem('saffronApiKey',v);document.getElementById('apikey').value='';refresh()}async function api(path,body){let h={'Content-Type':'application/json'};if(apiKey())h['X-API-Key']=apiKey();let r=await fetch(path,{method:body?'POST':'GET',headers:h,body:body?JSON.stringify(body):undefined});let j=await r.json();if(!r.ok)throw new Error(j.error||r.status);return j}async function refresh(){try{let s=await api('/api/status');document.getElementById('phase').value=s.phaseId;document.getElementById('commission').hidden=!s.configRecoveryRequired;document.getElementById('status').innerHTML=`<b>${s.phase}</b><p>Air: ${s.sensors.airTemperatureC??'NO DATA'} °C · RH: ${s.sensors.relativeHumidityPct??'NO DATA'} %</p><p>Coolant: ${s.sensors.coolantTemperatureC??'NO DATA'} °C</p><p>Mode: ${s.mode} · Recovery: ${s.recoveryStage}</p><p>Persistence: ${s.configurationSaveStatus}</p><p>Outputs are GPIO commands, not verified electrical feedback.</p><p>Lockout: ${s.lockoutReason}</p>`}catch(e){document.getElementById('status').textContent='Controller/API unavailable: '+e.message}}async function setPhase(){try{await api('/api/phase',{phaseId:+document.getElementById('phase').value});refresh()}catch(e){alert(e.message)}}async function setMode(v){try{await api('/api/mode',{automaticMode:v});refresh()}catch(e){alert(e.message)}}async function commission(){if(!confirm('Save initial/reviewed provisional configuration? This does not commission power electronics.'))return;try{await api('/api/commission',{confirm:true,commissioningAcknowledged:true});refresh()}catch(e){alert(e.message)}}async function estop(){if(confirm('Latch software emergency stop and force configured output GPIOs LOW?')){try{await api('/api/estop',{});refresh()}catch(e){alert(e.message)}}}refresh();setInterval(refresh,3000);</script></body></html>)HTML";
static void handleRoot() { server.send(200,"text/html",String(FPSTR(FALLBACK_HTML))); }
static void updateOled() {
  if (!oledOkay || millis()-lastOledAt<OLED_UPDATE_MS) return;
  lastOledAt=millis(); display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0,0);
  display.print("SAFFRON-AERO "); display.println(FW_VERSION);
  // Show access details for the first 20 seconds without blocking sensor acquisition,
  // safety checks, the API server or the control loop.
  if (millis()-bootAt < 20000UL) {
    display.println("Local offline access:");
    display.print("AP IP: "); display.println(WiFi.softAPIP().toString());
    display.print("Recovery: "); display.println(recoveryName(recoveryStage));
    display.println("Outputs locked on boot");
    display.display();
    return;
  }
  display.print("Phase: "); display.println(phaseName(cfg.phase));
  display.print("Air: "); if(meas.dhtValid){display.print(meas.airC,1);display.print(" C ");}else display.print("NO DATA ");
  display.print("RH: "); if(meas.dhtValid){display.print(meas.rh,0);display.println("%");}else display.println("NO DATA");
  display.print("Coolant: "); if(meas.coolantValid){display.print(meas.coolantC,1);display.println(" C");}else display.println("NO DATA");
  display.print("Mode: "); display.println(cfg.automaticMode?"AUTO":"MANUAL");
  display.print("Recovery: "); display.println(recoveryName(recoveryStage));
  if (cfg.estopLatched || configRecoveryRequired || !sensorsFresh()) display.println("OUTPUT LOCKOUT");
  display.display();
}
void setup() {
  // Keep this order: safe GPIO setup precedes loading persisted state, sensor warmup,
  // network serving and any recovery decision. This cannot replace external pull-downs.
  bootAt=millis(); Serial.begin(115200); delay(80);
  if(Pins::PELTIER_1_ENABLE>=0)pinMode(Pins::PELTIER_1_ENABLE,OUTPUT);
  if(Pins::PELTIER_2_ENABLE>=0)pinMode(Pins::PELTIER_2_ENABLE,OUTPUT);
  if(Pins::MIST_ENABLE>=0)pinMode(Pins::MIST_ENABLE,OUTPUT);
  if(Pins::COOLANT_PUMP_ENABLE>=0)pinMode(Pins::COOLANT_PUMP_ENABLE,OUTPUT);
  if(Pins::HOT_SIDE_FAN_ENABLE>=0)pinMode(Pins::HOT_SIDE_FAN_ENABLE,OUTPUT);
  if(Pins::FLOW_SWITCH_INPUT>=0)pinMode(Pins::FLOW_SWITCH_INPUT,INPUT_PULLDOWN);
  if(Pins::RESERVOIR_OK_INPUT>=0)pinMode(Pins::RESERVOIR_OK_INPUT,INPUT_PULLDOWN);
  setOutputsSafe();
  fsOkay = LittleFS.begin(false);
  if (!fsOkay) Serial.println("WARNING: LittleFS unavailable; persistent event log disabled");
  recoveryStage=LOAD_PERSISTED_STATE;
  loadConfig();
  Wire.begin(Pins::OLED_SDA,Pins::OLED_SCL);
  oledOkay=display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  if(!oledOkay) Serial.println("OLED not detected; control continues without display");
  dht.begin(); ds18.begin(); ds18.setWaitForConversion(false); ds18.requestTemperatures(); lastDsRequestAt=millis();
  if(configRecoveryRequired) recoveryStage=CONFIG_RECOVERY_REQUIRED; else recoveryStage=SENSOR_RECOVERY;
  WiFi.mode(WIFI_AP_STA);
  bool ap=WiFi.softAP(NetworkConfig::AP_SSID,NetworkConfig::AP_PASSWORD);
  if(!ap) Serial.println("WARNING: AP startup failed");
  if(String(NetworkConfig::STA_SSID).length()>0) { WiFi.begin(NetworkConfig::STA_SSID,NetworkConfig::STA_PASSWORD); }
  setupRoutes(); server.begin();
  addEvent("BOOT",String("Firmware ")+FW_VERSION+" started; all load outputs forced LOW");
  Serial.printf("SAFFRON-AERO AP IP: %s\n",WiFi.softAPIP().toString().c_str());
  Serial.println("Replace sample AP password and API key before any real load commissioning.");
}
void loop() {
  server.handleClient();
  updateSensors(); updateRecovery(); updateControl(); updateOled();
  // No blocking network or control delays. Watchdog-yielding loop is maintained.
  delay(1);
}
