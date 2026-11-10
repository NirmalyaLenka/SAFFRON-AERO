/*
  SAFFRON-AERO — offline ESP32 controller + local web dashboard
  =============================================================
  Hardware assumptions (verify before connecting any load):
    ESP32 Dev Module
    DHT22 DATA       -> GPIO 16
    DS18B20 DATA     -> GPIO 4, with 4.7k pull-up to 3V3
    SSD1306 OLED     -> SDA GPIO 21, SCL GPIO 22, I2C address 0x3C
    DS3231 RTC       -> same I2C bus (SDA 21 / SCL 22), optional but strongly
                        recommended for a true daily schedule while offline
    Peltier driver   -> PWM control input GPIO 25 (NOT the Peltier power wires)
    Humidifier relay -> GPIO 26 (NOT the humidifier power wires)

  IMPORTANT:
  - A Peltier module must use a correctly rated MOSFET/driver and separate power
    supply. Never power a Peltier directly from an ESP32 pin. Use a proper heat
    sink and hot-side fan. Do not PWM an AC/mains load with this GPIO output.
  - DS18B20 must be attached to the Peltier cold plate for the requested
    cold-plate cutoff at (selected chamber target - 7 C). If it is in the soil,
    that cutoff is not measuring the cold plate and should not be relied on.
  - Invalid/missing air or DS18B20 readings force both outputs OFF.
  - AUTO uses a proportional chamber-temperature controller capped by the
    Cooling % slider. Humidifier schedule is split into evenly spaced daily slots;
    a pulse is skipped if RH is already at/above the phase target.
  - With no DS3231 RTC, scheduler falls back to intervals since boot. It cannot
    know real clock time or fully preserve a calendar-day schedule through power
    loss. Install a DS3231 for accurate offline daily scheduling.
  - Phase profiles are research-informed initial values, not universal agronomic
    prescriptions. Edit each phase in the dashboard and validate for your corms.

  Libraries (Arduino Library Manager):
    DHT sensor library by Adafruit; Adafruit Unified Sensor; OneWire;
    DallasTemperature; Adafruit GFX Library; Adafruit SSD1306; RTClib by Adafruit.
  ESP32 board package: Espressif Systems. Supports Arduino-ESP32 core 2.x/3.x.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <time.h>
#include <sys/time.h>
#include <esp_arduino_version.h>

// -------------------- User hardware configuration --------------------
static constexpr uint8_t PIN_DHT22 = 16;
static constexpr uint8_t PIN_DS18B20 = 4;
static constexpr uint8_t PIN_OLED_SDA = 21;
static constexpr uint8_t PIN_OLED_SCL = 22;
static constexpr uint8_t PIN_PELTIER_PWM = 25;
static constexpr uint8_t PIN_HUMIDIFIER_RELAY = 26;
static constexpr bool HUMIDIFIER_RELAY_ACTIVE_HIGH = true; // set false for active-LOW relay boards
static constexpr uint32_t PELTIER_PWM_HZ = 1000;
static constexpr uint8_t PELTIER_PWM_BITS = 8;
static constexpr uint8_t PELTIER_PWM_CHANNEL = 0; // used on Arduino-ESP32 core 2.x
static constexpr float COLD_PLATE_OFFSET_C = 7.0f;
static constexpr float AIR_DEADBAND_C = 0.4f;
static constexpr float HUMIDITY_HYSTERESIS_RH = 2.0f;
static constexpr uint32_t SENSOR_INTERVAL_MS = 2500;
static constexpr uint32_t MANUAL_HUMIDIFIER_MAX_ON_MS = 60000; // hard stop after 60 s
static constexpr uint32_t MAX_HUMIDIFIER_PULSE_SECONDS = 60;

const char* WIFI_SSID = "Dihhh";       // open Wi-Fi; no password
const char* WIFI_PASSWORD = "";
const char* AP_SSID = "SAFFRON-AERO";
const char* AP_PASSWORD = "ChangeMe8266!"; // change before deployment

// -------------------- Phase profile starter values --------------------
// Values are editable and saved separately for each phase in Preferences.
enum PhaseId : uint8_t {
  PHASE_DORMANCY = 0,
  PHASE_FLORAL_INDUCTION,
  PHASE_BUD_SPROUTING,
  PHASE_FLOWERING,
  PHASE_VEGETATIVE,
  PHASE_CORM_MATURATION,
  PHASE_COUNT
};

const char* PHASE_NAMES[PHASE_COUNT] = {
  "Corm Dormancy",
  "Floral Induction",
  "Bud Sprouting",
  "Flowering",
  "Vegetative Growth",
  "Corm Maturation / Senescence"
};

// Published field/controlled studies report different temperatures depending on
// cultivar, timing and cultivation method. These are initial setpoints to EDIT.
float phaseTempC[PHASE_COUNT] = {18.3f, 23.0f, 19.8f, 17.0f, 8.0f, 13.0f};
float phaseRH[PHASE_COUNT]    = {65.5f, 65.7f, 67.7f, 75.5f, 77.7f, 68.4f};

DHT dht(PIN_DHT22, DHT22);
OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
RTC_DS3231 rtc;
WebServer server(80);
Preferences prefs;

uint8_t currentPhase = PHASE_DORMANCY;
String operatingMode = "AUTO"; // AUTO or MANUAL
uint8_t coolingPercent = 70;    // Manual duty / Auto maximum duty cap
bool scheduleEnabled = true;
uint8_t humidifierPulsesPerDay = 10;
uint8_t humidifierPulseSeconds = 5;
bool manualHumidifierOn = false;
bool actualHumidifierOn = false;
bool rtcAvailable = false;
bool apMode = false;
bool oledAvailable = false;
bool sensorsValid = false;
bool coldPlateBlocked = false;
float airTempC = NAN;
float humidityRH = NAN;
float coldPlateTempC = NAN;
uint8_t actualCoolingPercent = 0;
String safetyMessage = "Starting up";
unsigned long lastSensorReadMs = 0;
unsigned long lastOLEDRefreshMs = 0;
unsigned long lastWiFiRetryMs = 0;
unsigned long humidifierPulseStartMs = 0;
unsigned long manualHumidifierStartMs = 0;
unsigned long fallbackScheduleStartMs = 0;
int32_t lastScheduleDayKey = -1;
int16_t lastScheduleSlot = -1;

String phaseKey(char prefix, uint8_t i) {
  String k(prefix);
  k += String(i);
  return k;
}

String numberOrInvalid(float v, uint8_t digits = 1) {
  if (isnan(v) || v < -100.0f || v > 150.0f) return "Invalid";
  return String(v, static_cast<unsigned int>(digits));
}

bool validPhaseIndex(int idx) {
  return idx >= 0 && idx < PHASE_COUNT;
}

float targetTempC() { return phaseTempC[currentPhase]; }
float targetRH() { return phaseRH[currentPhase]; }
float coldPlateCutoffC() { return targetTempC() - COLD_PLATE_OFFSET_C; }

void setRelay(bool on) {
  actualHumidifierOn = on;
  digitalWrite(PIN_HUMIDIFIER_RELAY,
               on == HUMIDIFIER_RELAY_ACTIVE_HIGH ? HIGH : LOW);
}

void setPeltierDuty(uint8_t percent) {
  percent = constrain(percent, 0, 100);
  actualCoolingPercent = percent;
  uint32_t duty = map(percent, 0, 100, 0, (1 << PELTIER_PWM_BITS) - 1);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_PELTIER_PWM, duty);
#else
  ledcWrite(PELTIER_PWM_CHANNEL, duty);
#endif
}

void allOutputsOff(const String& reason) {
  setPeltierDuty(0);
  setRelay(false);
  safetyMessage = reason;
}

void saveAllSettings() {
  prefs.putUChar("phase", currentPhase);
  prefs.putString("mode", operatingMode);
  prefs.putUChar("cool", coolingPercent);
  prefs.putBool("hSched", scheduleEnabled);
  prefs.putUChar("hCount", humidifierPulsesPerDay);
  prefs.putUChar("hSecs", humidifierPulseSeconds);
  for (uint8_t i = 0; i < PHASE_COUNT; ++i) {
    prefs.putFloat(phaseKey('t', i).c_str(), phaseTempC[i]);
    prefs.putFloat(phaseKey('r', i).c_str(), phaseRH[i]);
  }
}

void loadAllSettings() {
  currentPhase = prefs.getUChar("phase", PHASE_DORMANCY);
  if (!validPhaseIndex(currentPhase)) currentPhase = PHASE_DORMANCY;
  operatingMode = prefs.getString("mode", "AUTO");
  if (operatingMode != "AUTO" && operatingMode != "MANUAL") operatingMode = "AUTO";
  coolingPercent = constrain(prefs.getUChar("cool", 70), 0, 100);
  scheduleEnabled = prefs.getBool("hSched", true);
  humidifierPulsesPerDay = constrain(prefs.getUChar("hCount", 10), 1, 48);
  humidifierPulseSeconds = constrain(prefs.getUChar("hSecs", 5), 1, MAX_HUMIDIFIER_PULSE_SECONDS);
  for (uint8_t i = 0; i < PHASE_COUNT; ++i) {
    phaseTempC[i] = prefs.getFloat(phaseKey('t', i).c_str(), phaseTempC[i]);
    phaseRH[i] = prefs.getFloat(phaseKey('r', i).c_str(), phaseRH[i]);
    phaseTempC[i] = constrain(phaseTempC[i], 0.0f, 35.0f);
    phaseRH[i] = constrain(phaseRH[i], 30.0f, 95.0f);
  }
  lastScheduleDayKey = (int32_t)prefs.getLong("lastDay", -1);
  lastScheduleSlot = prefs.getShort("lastSlot", -1);
}

void readSensors() {
  float rh = dht.readHumidity();
  float t = dht.readTemperature();
  ds18b20.requestTemperatures();
  float cold = ds18b20.getTempCByIndex(0);
  bool airGood = !isnan(t) && !isnan(rh) && t >= -10.0f && t <= 60.0f && rh >= 0.0f && rh <= 100.0f;
  bool coldGood = cold != DEVICE_DISCONNECTED_C && cold >= -55.0f && cold <= 125.0f;
  airTempC = airGood ? t : NAN;
  humidityRH = airGood ? rh : NAN;
  coldPlateTempC = coldGood ? cold : NAN;
  // This interlock assumes the DS18B20 is physically attached to the cold plate.
  sensorsValid = airGood && coldGood;
}

void drawOLED() {
  if (!oledAvailable) return;
  // Refresh at most once every 5 seconds to reduce unnecessary I2C traffic.
  if (lastOLEDRefreshMs != 0 && millis() - lastOLEDRefreshMs < 5000UL) return;
  lastOLEDRefreshMs = millis();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SAFFRON-AERO");
  display.print("Air: "); display.print(numberOrInvalid(airTempC)); display.println(" C");
  display.print("RH : "); display.print(numberOrInvalid(humidityRH)); display.println(" %");
  display.print("Cold: "); display.print(numberOrInvalid(coldPlateTempC)); display.println(" C");
  display.print("Set : "); display.print(targetTempC(), 1); display.println(" C");
  display.print("Mode: "); display.print(operatingMode); display.print(" "); display.print(actualCoolingPercent); display.println("%");
  display.print("Phase: "); display.println(PHASE_NAMES[currentPhase]);
  display.display();
}

void startHumidifierPulse(uint32_t seconds) {
  if (!sensorsValid || isnan(humidityRH)) return;
  setRelay(true);
  humidifierPulseStartMs = millis();
  humidifierPulseSeconds = constrain(seconds, 1, MAX_HUMIDIFIER_PULSE_SECONDS);
}

void updateHumidifier() {
  unsigned long nowMs = millis();
  // Manual mode: direct ON/OFF control with a 60-second hard-stop safeguard.
  if (operatingMode == "MANUAL") {
    if (!manualHumidifierOn) {
      setRelay(false);
      return;
    }
    if (!sensorsValid) {
      manualHumidifierOn = false;
      setRelay(false);
      safetyMessage = "Humidifier inhibited: invalid sensor reading";
      return;
    }
    if (manualHumidifierStartMs == 0) manualHumidifierStartMs = nowMs;
    if (nowMs - manualHumidifierStartMs >= MANUAL_HUMIDIFIER_MAX_ON_MS) {
      manualHumidifierOn = false;
      manualHumidifierStartMs = 0;
      setRelay(false);
      safetyMessage = "Manual humidifier timed out after 60 s";
      return;
    }
    setRelay(true);
    return;
  }

  manualHumidifierStartMs = 0;
  if (actualHumidifierOn && nowMs - humidifierPulseStartMs >= (uint32_t)humidifierPulseSeconds * 1000UL) {
    setRelay(false);
  }
  if (!scheduleEnabled || !sensorsValid || isnan(humidityRH)) return;
  if (actualHumidifierOn) return;

  int32_t dayKey;
  int16_t slot;
  if (rtcAvailable) {
    DateTime now = rtc.now();
    dayKey = (int32_t)(now.year() * 10000L + now.month() * 100L + now.day());
    uint32_t secDay = now.hour() * 3600UL + now.minute() * 60UL + now.second();
    slot = (int16_t)(((uint64_t)secDay * humidifierPulsesPerDay) / 86400ULL);
  } else {
    uint64_t elapsed = (uint64_t)(nowMs - fallbackScheduleStartMs);
    uint64_t dayMs = elapsed % 86400000ULL;
    dayKey = (int32_t)(elapsed / 86400000ULL);
    slot = (int16_t)((dayMs * humidifierPulsesPerDay) / 86400000ULL);
  }

  if (dayKey != lastScheduleDayKey) {
    lastScheduleDayKey = dayKey;
    lastScheduleSlot = -1;
    prefs.putLong("lastDay", lastScheduleDayKey);
    prefs.putShort("lastSlot", lastScheduleSlot);
  }
  if (slot != lastScheduleSlot && slot > lastScheduleSlot) {
    // Mark slot before starting to prevent duplicates after a reboot.
    lastScheduleSlot = slot;
    prefs.putShort("lastSlot", lastScheduleSlot);
    // Do not add moisture when the phase RH target is already met.
    if (humidityRH < targetRH() - HUMIDITY_HYSTERESIS_RH) {
      startHumidifierPulse(humidifierPulseSeconds);
      safetyMessage = "Scheduled humidifier pulse";
    }
  }
}

void updateControl() {
  if (!sensorsValid) {
    manualHumidifierOn = false;
    allOutputsOff("SAFETY: invalid/missing sensor; outputs OFF");
    return;
  }

  // Hysteresis prevents rapid switching when the cold plate hovers near cutoff.
  if (coldPlateTempC <= coldPlateCutoffC()) coldPlateBlocked = true;
  else if (coldPlateTempC >= coldPlateCutoffC() + 1.0f) coldPlateBlocked = false;

  uint8_t demand = 0;
  if (operatingMode == "MANUAL") {
    demand = coolingPercent;
  } else {
    float error = airTempC - targetTempC();
    if (error > AIR_DEADBAND_C) {
      // Proportional-only starting point; tune after testing with real chamber load.
      float calculated = error * 18.0f;
      demand = (uint8_t)constrain((int)roundf(calculated), 0, (int)coolingPercent);
    } else {
      demand = 0;
    }
  }

  if (coldPlateBlocked) {
    setPeltierDuty(0);
    safetyMessage = "Peltier OFF: cold plate reached target - 7 C";
  } else {
    setPeltierDuty(demand);
    safetyMessage = (operatingMode == "AUTO") ? "AUTO temperature control active" : "MANUAL cooling slider active";
  }
  updateHumidifier();
}

String jsonStatus() {
  String j = "{";
  j += "\"airTemp\":\"" + numberOrInvalid(airTempC) + "\",";
  j += "\"humidity\":\"" + numberOrInvalid(humidityRH) + "\",";
  j += "\"coldTemp\":\"" + numberOrInvalid(coldPlateTempC) + "\",";
  j += "\"phase\":\"" + String(PHASE_NAMES[currentPhase]) + "\",";
  j += "\"phaseIndex\":" + String(currentPhase) + ",";
  j += "\"mode\":\"" + operatingMode + "\",";
  j += "\"targetTemp\":" + String(targetTempC(), 1) + ",";
  j += "\"targetRH\":" + String(targetRH(), 1) + ",";
  j += "\"coldCutoff\":" + String(coldPlateCutoffC(), 1) + ",";
  j += "\"coolingLimit\":" + String(coolingPercent) + ",";
  j += "\"actualCooling\":" + String(actualCoolingPercent) + ",";
  j += "\"scheduleEnabled\":" + String(scheduleEnabled ? "true" : "false") + ",";
  j += "\"pulseCount\":" + String(humidifierPulsesPerDay) + ",";
  j += "\"pulseSeconds\":" + String(humidifierPulseSeconds) + ",";
  j += "\"humidifierOn\":" + String(actualHumidifierOn ? "true" : "false") + ",";
  j += "\"rtc\":" + String(rtcAvailable ? "true" : "false") + ",";
  j += "\"sensorsValid\":" + String(sensorsValid ? "true" : "false") + ",";
  j += "\"safety\":\"" + safetyMessage + "\",";
  j += "\"network\":\"" + String(apMode ? "Access Point" : "Wi-Fi station") + "\"";
  j += "}";
  return j;
}

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SAFFRON-AERO Control</title><style>
:root{color-scheme:dark}*{box-sizing:border-box}body{font-family:system-ui,Arial;margin:0;background:#101820;color:#edf2f5}
main{max-width:900px;margin:auto;padding:16px}.hero{padding:14px 4px 8px}h1{color:#efc777;margin:0;font-size:1.8rem}h2{font-size:1.05rem;margin:0 0 12px;color:#e7c37b}
.muted{color:#acbac4;font-size:.88rem}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:12px}.card{background:#1b2a35;border:1px solid #30414c;border-radius:13px;padding:16px;margin:12px 0}
.stat{font-size:1.55rem;font-weight:700}.unit{font-size:.85rem;color:#abbac4}.row{display:flex;gap:10px;align-items:center;justify-content:space-between;flex-wrap:wrap;margin:9px 0}
label{display:block;color:#cbd5db;font-size:.9rem;margin-bottom:5px}input,select,button{font:inherit;border-radius:8px;border:1px solid #475965;padding:10px;background:#111e27;color:#edf2f5;max-width:100%}input[type=range]{padding:0;width:100%;accent-color:#e9be65}input[type=number]{width:115px}button{cursor:pointer;background:#dfb85e;color:#17212a;border:0;font-weight:700}button.secondary{background:#334754;color:#fff}button.danger{background:#b64d4d;color:white}.value{font-weight:700;color:#f2cc79}.pill{display:inline-block;border-radius:99px;background:#314451;padding:5px 10px;font-size:.8rem}.warn{color:#ffd28a}.small{font-size:.82rem}.wide{width:100%}hr{border:0;border-top:1px solid #354752;margin:14px 0}
</style></head><body><main><div class="hero"><h1>SAFFRON-AERO</h1><div class="muted">Offline climate control · ESP32 local dashboard</div></div>
<div class="grid">
 <div class="card"><h2>Air temperature</h2><div class="stat"><span id="air">—</span> <span class="unit">°C</span></div><div class="muted">Target: <span id="tTemp">—</span> °C</div></div>
 <div class="card"><h2>Relative humidity</h2><div class="stat"><span id="rh">—</span> <span class="unit">% RH</span></div><div class="muted">Target: <span id="tRH">—</span> % RH</div></div>
 <div class="card"><h2>Cold-plate probe</h2><div class="stat"><span id="cold">—</span> <span class="unit">°C</span></div><div class="muted">Peltier cutoff: <span id="cutoff">—</span> °C (target − 7 °C)</div></div>
 <div class="card"><h2>Outputs</h2><div>Peltier: <span class="value" id="coolOut">0</span>%</div><div>Humidifier: <span class="value" id="humOut">OFF</span></div><div class="muted" id="net">Network: —</div></div>
</div>
<div class="card"><h2>Phase and operating mode</h2><div class="grid"><div><label for="phase">Growth phase</label><select id="phase" class="wide" onchange="setPhase()"><option value="0">Corm Dormancy</option><option value="1">Floral Induction</option><option value="2">Bud Sprouting</option><option value="3">Flowering</option><option value="4">Vegetative Growth</option><option value="5">Corm Maturation / Senescence</option></select></div><div><label for="mode">Mode</label><select id="mode" class="wide" onchange="setMode()"><option>AUTO</option><option>MANUAL</option></select></div></div><div class="muted" style="margin-top:10px">AUTO loads this phase's target temperature and RH. Phase values can be edited below and are saved across power loss.</div></div>
<div class="card"><h2>Phase target settings</h2><div class="grid"><div><label for="targetTempInput">Target air temperature (°C)</label><input type="number" id="targetTempInput" min="0" max="35" step="0.5"></div><div><label for="targetRHInput">Target relative humidity (%)</label><input type="number" id="targetRHInput" min="30" max="95" step="1"></div></div><div class="row"><button onclick="saveTargets()">Save selected phase targets</button><span class="muted">Saved per phase</span></div></div>
<div class="card"><h2>Peltier cooling control</h2><div class="row"><span id="sliderLabel">Max cooling percentage</span><span class="value"><span id="coolVal">70</span>%</span></div><input type="range" min="0" max="100" value="70" id="coolSlider" oninput="previewCooling(this.value)" onchange="saveCooling(this.value)"><div class="muted" id="coolHint">In AUTO this is the maximum PWM cap. In MANUAL this is the requested PWM duty.</div><hr><div class="muted">AUTO uses air temperature feedback with a proportional controller. The DS18B20 must measure the Peltier cold plate for the target − 7 °C cutoff to be meaningful.</div></div>
<div class="card"><h2>Humidifier schedule</h2><div class="row"><label for="sched" style="margin:0">Enable scheduled humidifier in AUTO</label><input id="sched" type="checkbox" checked onchange="saveSchedule()"></div><div class="grid"><div><label for="count">Pulses per day (1–48)</label><input type="number" id="count" min="1" max="48" value="10"></div><div><label for="seconds">Seconds per pulse (1–60)</label><input type="number" id="seconds" min="1" max="60" value="5"></div></div><div class="row"><button onclick="saveSchedule()">Save schedule</button><span class="pill">Default: 10 × 5 seconds/day</span></div><div class="muted">Scheduled slots are evenly spaced through the day. A pulse runs only when RH is at least 2% below the selected phase target; it is skipped if RH is already high enough. DS3231 RTC is required for reliable wall-clock scheduling offline.</div></div>
<div class="card"><h2>Manual humidifier</h2><div class="row"><div><div id="manualHumState" class="value">OFF</div><div class="muted">Available in MANUAL mode. Automatic safety timeout: 60 seconds.</div></div><button id="humButton" class="secondary" onclick="toggleManualHum()">Turn ON</button></div></div>
<div class="card"><h2>System safety</h2><div id="safety" class="warn">Checking sensors…</div><div class="muted" id="sensorState"></div><div class="muted" id="rtcState"></div><div class="muted" style="margin-top:8px">Peltier and humidifier outputs are forced OFF if readings are invalid. Test first with loads disconnected, then with a current-limited/appropriate driver stage.</div></div>
<div class="muted small">Phase presets are starting values based on published saffron studies and regional observations, not universal setpoints. Cultivar, corm condition and cultivation protocol matter.</div>
</main><script>
let loaded=false;const $=id=>document.getElementById(id);
async function req(path,method='POST'){let r=await fetch(path,{method});let t=await r.text();if(!r.ok)throw new Error(t||'Request failed');return t;}
function previewCooling(v){$('coolVal').textContent=v;}
function apply(d){loaded=true;$('air').textContent=d.airTemp;$('rh').textContent=d.humidity;$('cold').textContent=d.coldTemp;$('tTemp').textContent=d.targetTemp.toFixed(1);$('tRH').textContent=d.targetRH.toFixed(1);$('cutoff').textContent=d.coldCutoff.toFixed(1);$('coolOut').textContent=d.actualCooling;$('humOut').textContent=d.humidifierOn?'ON':'OFF';$('net').textContent='Network: '+d.network;$('phase').value=d.phaseIndex;$('mode').value=d.mode;$('targetTempInput').value=d.targetTemp;$('targetRHInput').value=d.targetRH;$('coolSlider').value=d.coolingLimit;$('coolVal').textContent=d.coolingLimit;$('sched').checked=d.scheduleEnabled;$('count').value=d.pulseCount;$('seconds').value=d.pulseSeconds;$('safety').textContent=d.safety;$('sensorState').textContent='Sensor interlock: '+(d.sensorsValid?'VALID':'INVALID — outputs OFF');$('rtcState').textContent=d.rtc?'DS3231 RTC detected: calendar scheduling enabled.':'No DS3231 detected: schedule uses time since boot, not wall-clock time.';$('manualHumState').textContent=d.humidifierOn?'ON':'OFF';$('humButton').textContent=d.humidifierOn?'Turn OFF':'Turn ON';$('sliderLabel').textContent=d.mode==='AUTO'?'Max cooling percentage':'Manual cooling percentage';$('coolHint').textContent=d.mode==='AUTO'?'In AUTO this is the maximum PWM cap; the temperature controller varies duty below this limit.':'In MANUAL this is the requested PWM duty, subject to the cold-plate safety cutoff.';}
async function refresh(){try{let r=await fetch('/api/status');apply(await r.json());}catch(e){$('safety').textContent='Dashboard connection error: '+e.message;}}
async function setPhase(){try{await req('/api/phase?value='+$('phase').value);await refresh();}catch(e){alert(e.message);}}
async function setMode(){try{await req('/api/mode?value='+encodeURIComponent($('mode').value));await refresh();}catch(e){alert(e.message);}}
async function saveTargets(){let t=Number($('targetTempInput').value),r=Number($('targetRHInput').value);if(!Number.isFinite(t)||t<0||t>35||!Number.isFinite(r)||r<30||r>95){alert('Temperature must be 0–35 °C and RH must be 30–95%.');return;}try{await req('/api/targets?temp='+t+'&rh='+r);await refresh();}catch(e){alert(e.message);}}
async function saveCooling(v){try{await req('/api/cooling?value='+v);await refresh();}catch(e){alert(e.message);}}
async function saveSchedule(){let n=Number($('count').value),s=Number($('seconds').value);if(!Number.isInteger(n)||n<1||n>48||!Number.isInteger(s)||s<1||s>60){alert('Pulses must be 1–48 per day and pulse duration 1–60 seconds.');return;}try{await req('/api/schedule?enabled='+$('sched').checked+'&count='+n+'&seconds='+s);await refresh();}catch(e){alert(e.message);}}
async function toggleManualHum(){try{await req('/api/manual-humidifier?on='+!($('manualHumState').textContent==='ON'));await refresh();}catch(e){alert(e.message);}}
refresh();setInterval(refresh,2500);
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }
void handleStatus() { server.send(200, "application/json", jsonStatus()); }

void handlePhase() {
  if (!server.hasArg("value")) { server.send(400, "text/plain", "Missing phase index"); return; }
  int idx = server.arg("value").toInt();
  if (!validPhaseIndex(idx)) { server.send(400, "text/plain", "Invalid phase index"); return; }
  currentPhase = (uint8_t)idx;
  prefs.putUChar("phase", currentPhase);
  coldPlateBlocked = false;
  server.send(200, "text/plain", "Phase loaded: " + String(PHASE_NAMES[currentPhase]));
}

void handleMode() {
  if (!server.hasArg("value")) { server.send(400, "text/plain", "Missing mode"); return; }
  String m = server.arg("value");
  if (m != "AUTO" && m != "MANUAL") { server.send(400, "text/plain", "Mode must be AUTO or MANUAL"); return; }
  operatingMode = m;
  manualHumidifierOn = false;
  manualHumidifierStartMs = 0;
  setRelay(false);
  prefs.putString("mode", operatingMode);
  server.send(200, "text/plain", "Mode saved: " + operatingMode);
}

void handleTargets() {
  if (!server.hasArg("temp") || !server.hasArg("rh")) { server.send(400, "text/plain", "Missing target temperature or RH"); return; }
  float t = server.arg("temp").toFloat();
  float rh = server.arg("rh").toFloat();
  if (t < 0 || t > 35 || rh < 30 || rh > 95) { server.send(400, "text/plain", "Temperature must be 0-35 C and RH 30-95%"); return; }
  phaseTempC[currentPhase] = t;
  phaseRH[currentPhase] = rh;
  prefs.putFloat(phaseKey('t', currentPhase).c_str(), t);
  prefs.putFloat(phaseKey('r', currentPhase).c_str(), rh);
  coldPlateBlocked = false;
  server.send(200, "text/plain", "Phase targets saved");
}

void handleCooling() {
  if (!server.hasArg("value")) { server.send(400, "text/plain", "Missing cooling percentage"); return; }
  int value = server.arg("value").toInt();
  if (value < 0 || value > 100) { server.send(400, "text/plain", "Cooling percentage must be 0-100"); return; }
  coolingPercent = (uint8_t)value;
  prefs.putUChar("cool", coolingPercent);
  server.send(200, "text/plain", "Cooling percentage saved");
}

void handleSchedule() {
  if (!server.hasArg("enabled") || !server.hasArg("count") || !server.hasArg("seconds")) { server.send(400, "text/plain", "Missing schedule fields"); return; }
  int count = server.arg("count").toInt();
  int seconds = server.arg("seconds").toInt();
  if (count < 1 || count > 48 || seconds < 1 || seconds > (int)MAX_HUMIDIFIER_PULSE_SECONDS) { server.send(400, "text/plain", "Pulses 1-48; duration 1-60 seconds"); return; }
  scheduleEnabled = server.arg("enabled") == "true";
  humidifierPulsesPerDay = (uint8_t)count;
  humidifierPulseSeconds = (uint8_t)seconds;
  lastScheduleSlot = -1; // begin using the updated schedule at the current slot
  prefs.putBool("hSched", scheduleEnabled);
  prefs.putUChar("hCount", humidifierPulsesPerDay);
  prefs.putUChar("hSecs", humidifierPulseSeconds);
  prefs.putShort("lastSlot", lastScheduleSlot);
  server.send(200, "text/plain", "Humidifier schedule saved");
}

void handleManualHumidifier() {
  if (!server.hasArg("on")) { server.send(400, "text/plain", "Missing on=true/false"); return; }
  bool on = server.arg("on") == "true";
  if (operatingMode != "MANUAL") { server.send(409, "text/plain", "Switch to MANUAL mode before using direct humidifier control"); return; }
  if (on && !sensorsValid) { server.send(409, "text/plain", "Sensor interlock: humidifier cannot start until readings are valid"); return; }
  manualHumidifierOn = on;
  manualHumidifierStartMs = on ? millis() : 0;
  if (!on) setRelay(false);
  server.send(200, "text/plain", on ? "Manual humidifier ON (60-second safety timeout)" : "Manual humidifier OFF");
}

void startNetwork() {
  Serial.println();
  Serial.println("--- Wi-Fi diagnostics ---");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Scan first so Serial Monitor tells us whether the router is visible.
  Serial.print("Scanning for SSID: ");
  Serial.println(WIFI_SSID);
  int found = WiFi.scanNetworks(false, true);
  bool targetSeen = false;
  if (found < 0) {
    Serial.printf("Wi-Fi scan failed, code: %d\n", found);
  } else {
    for (int i = 0; i < found; ++i) {
      String name = WiFi.SSID(i);
      Serial.printf("  %s | RSSI %d dBm | channel %d | encryption %d\n",
                    name.c_str(), WiFi.RSSI(i), WiFi.channel(i),
                    (int)WiFi.encryptionType(i));
      if (name == WIFI_SSID) targetSeen = true;
    }
  }
  WiFi.scanDelete();
  Serial.printf("Target network visible: %s\n", targetSeen ? "YES" : "NO");

  // Keep the ESP32 access point active even while trying the router.
  // This means the dashboard remains reachable if the router rejects the ESP32.
  WiFi.mode(WIFI_AP_STA);
  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("Fallback AP: %s\n", apStarted ? "STARTED" : "FAILED");
  if (apStarted) {
    Serial.print("Fallback dashboard: http://");
    Serial.println(WiFi.softAPIP());
  }

  Serial.printf("Connecting to open Wi-Fi '%s' ...\n", WIFI_SSID);
  // Use the no-password overload for an explicitly open network.
  WiFi.begin(WIFI_SSID);
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000UL) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    Serial.println("STA CONNECTED.");
    Serial.print("Router dashboard: http://");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("Fallback AP is also active: SAFFRON-AERO");
  } else {
    apMode = true;
    Serial.printf("STA CONNECTION FAILED. WiFi.status() = %d\n", (int)WiFi.status());
    Serial.println("Use the SAFFRON-AERO access point shown above.");
    Serial.println("If Dihhh was not visible, check 2.4 GHz Wi-Fi and router range.");
    Serial.println("If Dihhh was visible but rejected, verify it is truly open (no password/captive portal). ");
  }
  Serial.println("-------------------------");
}

void setupPwm() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  bool ok = ledcAttach(PIN_PELTIER_PWM, PELTIER_PWM_HZ, PELTIER_PWM_BITS);
  if (!ok) Serial.println("WARNING: LEDC attach failed; cooling output unavailable.");
  ledcWrite(PIN_PELTIER_PWM, 0);
#else
  ledcSetup(PELTIER_PWM_CHANNEL, PELTIER_PWM_HZ, PELTIER_PWM_BITS);
  ledcAttachPin(PIN_PELTIER_PWM, PELTIER_PWM_CHANNEL);
  ledcWrite(PELTIER_PWM_CHANNEL, 0);
#endif
}

void setup() {
  Serial.begin(115200);
  delay(250);

  // Drive outputs to safe states before sensor, Wi-Fi or web setup.
  pinMode(PIN_HUMIDIFIER_RELAY, OUTPUT);
  digitalWrite(PIN_HUMIDIFIER_RELAY, HUMIDIFIER_RELAY_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PIN_PELTIER_PWM, OUTPUT);
  digitalWrite(PIN_PELTIER_PWM, LOW);
  setRelay(false);

  prefs.begin("saffron-aero", false);
  loadAllSettings();

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  oledAvailable = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledAvailable) Serial.println("OLED not detected; continuing without display.");

  rtcAvailable = rtc.begin();
  if (rtcAvailable) {
    if (rtc.lostPower()) {
      // Initialises RTC to firmware compilation time. After that, its coin cell
      // preserves time through ESP32 power loss.
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println("RTC lost power; initialized from firmware compile time.");
    }
    DateTime now = rtc.now();
    Serial.printf("DS3231 time: %04d-%02d-%02d %02d:%02d:%02d\n", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  } else {
    Serial.println("No DS3231 detected: daily schedule will use time since boot.");
    fallbackScheduleStartMs = millis();
    lastScheduleDayKey = -1;
    lastScheduleSlot = -1;
  }

  dht.begin();
  ds18b20.begin();
  setupPwm();
  setPeltierDuty(0);
  setRelay(false);

  // Do not restore loads immediately after power loss. Re-read sensors and let
  // updateControl enforce interlocks before any actuator can be enabled.
  readSensors();
  allOutputsOff("Boot recovery: outputs held OFF pending valid sensor checks");
  drawOLED();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/phase", HTTP_POST, handlePhase);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/targets", HTTP_POST, handleTargets);
  server.on("/api/cooling", HTTP_POST, handleCooling);
  server.on("/api/schedule", HTTP_POST, handleSchedule);
  server.on("/api/manual-humidifier", HTTP_POST, handleManualHumidifier);
  server.begin();
  startNetwork();
  lastWiFiRetryMs = millis();
  lastSensorReadMs = millis();
  Serial.println("SAFFRON-AERO controller ready. Check serial output for dashboard URL.");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (now - lastSensorReadMs >= SENSOR_INTERVAL_MS) {
    lastSensorReadMs = now;
    readSensors();
    updateControl();
    drawOLED();
  }
  // Retry the router connection every 30 seconds while keeping the AP available.
  if (WiFi.status() != WL_CONNECTED && millis() - lastWiFiRetryMs >= 30000UL) {
    lastWiFiRetryMs = millis();
    Serial.printf("Retrying Wi-Fi SSID %s...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID); // open network
  }
  // Continue schedule pulse timing between sensor samples.
  updateHumidifier();
  delay(2);
}