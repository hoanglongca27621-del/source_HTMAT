/*
 * MAY AP TRUNG ESP32 - FULL FIRMWARE
 * Board: ESP32 Dev Module, Arduino framework
 *
 * Hardware pin map (kept as supplied by the user):
 *   LCD I2C SDA=21 SCL=22, DHT11=4, MQ2 AO=34
 *   Fan relay IN=13, heater relay IN=27, buzzer output=26 (ACTIVE-HIGH, based on observed hardware behavior)
 *   POWER button=25, BUZZER-enable button=33, FAN override=18,
 *   HEATER override=19 (all buttons INPUT_PULLUP, pressed=LOW)
 *
 * Control policy requested:
 *   Heater AUTO hysteresis: ON below 36.5 C, OFF at/above 37.5 C.
 *   Fan AUTO: ON at/above 37.5 C or RH >=80%, or calibrated MQ2 raw alarm.
 *   Buzzer alarm: T<35.5 C or T>=37.5 C or MQ2 raw above configured threshold.
 *   Humidity thresholds affect LCD/fan only; they do not sound the buzzer.
 *   Physical FAN/HEATER button: invert current state for exactly 5 s,
 *   then return to AUTO. Remote forced states persist until AUTO is selected.
 *
 * IMPORTANT:
 *   - Sensor invalid/stale, system OFF, or over-temperature interlock always
 *     overrides manual/remote commands. Manual ON is not a safety bypass.
 *   - MQ2 ADC counts are NOT gas percent. Set the raw threshold only after
 *     calibrating the installed MQ2 module for the target gas/environment.
 *   - DHT11 is coarse (1 C resolution, limited accuracy); it cannot guarantee
 *     37.5 C incubation accuracy. Verify with a reference thermometer and
 *     install an independent thermostat/thermal fuse in series with heater.
 *   - Do not power bulb/fan/relay coils/buzzer from ESP32 GPIO or 3V3 pin.
 *     Use a separately rated 5 V supply, common GND, branch fusing, and verify
 *     the relay board accepts 3.3 V GPIO logic. A bare 5 V buzzer needs a driver.
 *   - Relay outputs are configured ACTIVE-HIGH by default (LOW=OFF), matching
 *     the requested "LOW when system off" relay logic. Change only after testing.
 *   - BUZZER_ACTIVE_LEVEL is HIGH, so buzzer OFF is GPIO LOW. Verify this
 *     polarity against the installed buzzer driver/module before connecting it.
 */
#include "esp_heap_caps.h"
#include <mbedtls/x509_crt.h>
#include <string.h>
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"

#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <stdio.h>

// -------------------- PINS --------------------
#define DHTPIN              4
#define DHTTYPE             DHT11
#define MQ2_PIN             34
#define RELAY_FAN_PIN       13
#define RELAY_HEATER_PIN    27
#define BUZZER_PIN          26
#define BTN_POWER           25
#define BTN_BUZZER          33
#define BTN_FAN             18
#define BTN_HEATER          19
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22

#define RELAY_ACTIVE_LEVEL  HIGH
#define BUZZER_ACTIVE_LEVEL HIGH

// -------------------- CONTROL CONSTANTS --------------------
static constexpr uint32_t DHT_INTERVAL_MS       = 2500;
static constexpr uint32_t SENSOR_STALE_MS       = 6500;
static constexpr uint32_t BUTTON_DEBOUNCE_MS    = 35;
static constexpr uint32_t LOCAL_OVERRIDE_MS    = 5000;
static constexpr uint32_t DISPLAY_NOTICE_MS     = 1500;
static constexpr uint32_t POWER_NOTICE_MS       = 2000;
static constexpr uint32_t LCD_UPDATE_MS         = 2000;
static constexpr uint32_t LCD_BLINK_MS          = 450;
static constexpr uint32_t TELEMETRY_UPDATE_MS    = 1000;
static constexpr uint32_t MQ2_UPDATE_MS         = 500;
static constexpr uint32_t LOG_INTERVAL_MS       = 10000;

static constexpr float HEATER_ON_BELOW_C        = 36.5f;
static constexpr float HEATER_OFF_AT_C          = 37.5f;
static constexpr float TEMP_LOW_ALARM_C         = 35.5f;
static constexpr float HUM_LOW_ALARM_PCT        = 40.0f;
static constexpr float HUM_HIGH_ALARM_PCT       = 80.0f;
static constexpr float HARD_TEMP_CUTOFF_C       = 39.0f; // software interlock only
static constexpr size_t MAX_MESSAGE_BYTES       = 1024;

// Fixed raw-ADC threshold requested by the user. This is NOT a gas concentration.
static constexpr uint16_t MQ2_ALARM_RAW_DEFAULT = 410;

// -------------------- OBJECTS --------------------
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------- SHARED STATE --------------------
struct ControlState {
  float setpoint;       // retained for telemetry/config; AUTO is hysteresis-based, not PID
  float kp;             // reserved; not used by this thermostat controller
  float ki;             // reserved; not used by this thermostat controller
  float kd;             // reserved; not used by this thermostat controller
  bool systemPower;     // equivalent to systemEnabled
  bool manualMode;      // true only while a physical 5 s override is active
  bool heaterState;     // last commanded heater state (logical ON/OFF)
  bool fanState;        // last commanded fan state (logical ON/OFF)
  bool buzzerMuted;     // true means buzzer switch is disabled
  int8_t heaterRemoteOverride; // -1=AUTO, 0=FORCE OFF, 1=FORCE ON
  int8_t fanRemoteOverride;    // -1=AUTO, 0=FORCE OFF, 1=FORCE ON
  bool mq2AlarmEnabled;
  uint16_t mq2AlarmRaw;
};

ControlState control = {
  37.5f, 0.0f, 0.0f, 0.0f,
  true, false, false, false, false,
  -1, -1, true, MQ2_ALARM_RAW_DEFAULT
};

portMUX_TYPE controlMux = portMUX_INITIALIZER_UNLOCKED;

WiFiClientSecure mqttTlsClient;
PubSubClient mqttClient(mqttTlsClient);
char mqttTopicState[96];
char mqttTopicEvents[96];
char mqttTopicCommands[96];
char mqttTopicAvailability[96];
void publishLogEvent(uint8_t category, const char *message);

ControlState getControlSnapshot() {
  ControlState s;
  portENTER_CRITICAL(&controlMux);
  s = control;
  portEXIT_CRITICAL(&controlMux);
  return s;
}

// The following sensor/timer/physical-output variables are only accessed by loop().
float currentTemp = NAN;
float currentHum = NAN;
uint16_t mq2Raw = 0;
bool sensorValid = false;
bool overtempLatched = false;
bool heaterRelayOn = false;
bool fanRelayOn = false;
bool buzzerOutputOn = false;
bool autoHeaterState = false;
bool autoFanState = false;
bool heaterLocalOverrideActive = false;
bool fanLocalOverrideActive = false;
bool heaterLocalTarget = false;
bool fanLocalTarget = false;
uint32_t heaterLocalUntilMs = 0;
uint32_t fanLocalUntilMs = 0;
uint32_t lastGoodSensorMs = 0;
uint32_t lastDhtReadMs = 0;
uint32_t lastMq2ReadMs = 0;
uint32_t lastTelemetryPublishMs = 0;
uint32_t lastLcdUpdateMs = 0;
uint32_t lastLogMs = 0;
uint8_t dhtConsecutiveFailures = 0;
bool dhtFaultLogged = false;
bool heaterRequestedOn = false;
bool fanRequestedOn = false;

// LCD transient notice state.
char displayNotice0[17] = "";
char displayNotice1[17] = "";
uint32_t displayNoticeUntilMs = 0;
uint32_t displayLockUntilMs = 0;
bool pendingClearHeaterLocal = false;
bool pendingClearFanLocal = false;
bool powerTransitionPhysicalPending = false;
bool displayBlinkVisible = false;
bool displayNoticeDrawn = false;
bool powerOffScreenDrawn = false;
struct QueuedLcdNotice { char row0[17]; char row1[17]; uint16_t durationMs; };
QueuedLcdNotice lcdNoticeQueue[4];
uint8_t lcdNoticeHead=0, lcdNoticeCount=0;
uint32_t lastDisplayBlinkMs = 0;

struct ButtonState {
  uint8_t pin;
  bool stableLevel;
  bool lastRawLevel;
  uint32_t lastChangeMs;
};
ButtonState buttons[] = {
  {BTN_POWER, HIGH, HIGH, 0},
  {BTN_BUZZER, HIGH, HIGH, 0},
  {BTN_FAN, HIGH, HIGH, 0},
  {BTN_HEATER, HIGH, HIGH, 0}
};

// -------------------- OUTPUT HELPERS --------------------
static inline uint8_t relayOffLevel() {
  return (RELAY_ACTIVE_LEVEL == HIGH) ? LOW : HIGH;
}
static inline uint8_t buzzerOffLevel() {
  return (BUZZER_ACTIVE_LEVEL == HIGH) ? LOW : HIGH;
}

void writeRelayPin(uint8_t pin, bool on) {
  digitalWrite(pin, on ? RELAY_ACTIVE_LEVEL : relayOffLevel());
}

void setHeaterRelay(bool on) {
  if (heaterRelayOn == on) return;
  writeRelayPin(RELAY_HEATER_PIN, on);
  heaterRelayOn = on;
}

void setFanRelay(bool on) {
  if (fanRelayOn == on) return;
  writeRelayPin(RELAY_FAN_PIN, on);
  fanRelayOn = on;
}

void setBuzzer(bool on) {
  // Always reassert the physical OFF level. This prevents a stale internal
  // state flag from leaving the buzzer energized after a power-off.
  if (!on) {
    digitalWrite(BUZZER_PIN, buzzerOffLevel());
    buzzerOutputOn = false;
    return;
  }
  if (buzzerOutputOn) return;
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LEVEL);
  buzzerOutputOn = true;
}

void resetLocalOverrides() {
  heaterLocalOverrideActive = false;
  fanLocalOverrideActive = false;
  heaterLocalTarget = false;
  fanLocalTarget = false;
  heaterLocalUntilMs = 0;
  fanLocalUntilMs = 0;
  portENTER_CRITICAL(&controlMux);
  pendingClearHeaterLocal = false;
  pendingClearFanLocal = false;
  control.manualMode = false;
  control.heaterRemoteOverride = -1;
  control.fanRemoteOverride = -1;
  portEXIT_CRITICAL(&controlMux);
}

void showDisplayNotice(const char *line0, const char *line1, uint32_t durationMs) {
  snprintf(displayNotice0, sizeof(displayNotice0), "%s", line0);
  snprintf(displayNotice1, sizeof(displayNotice1), "%s", line1);
  const uint32_t now = millis();
  displayNoticeUntilMs = now + durationMs;
  displayLockUntilMs = displayNoticeUntilMs;
  displayNoticeDrawn = false;
  lastLcdUpdateMs = now;
}

void queueNetworkDisplayNotice(const char *line0, const char *line1, uint32_t durationMs) {
  const ControlState s = getControlSnapshot();
  if (!s.systemPower) return; // OFF screen intentionally remains frozen
  const uint32_t now = millis();
  if ((int32_t)(displayNoticeUntilMs - now) <= 0 && lcdNoticeCount == 0) {
    showDisplayNotice(line0, line1, durationMs);
    return;
  }
  if (lcdNoticeCount >= 4) {
    // Keep the newest connection state; discard the oldest queued notice.
    lcdNoticeHead = (uint8_t)((lcdNoticeHead + 1U) % 4U);
    --lcdNoticeCount;
  }
  const uint8_t tail = (uint8_t)((lcdNoticeHead + lcdNoticeCount) % 4U);
  snprintf(lcdNoticeQueue[tail].row0, sizeof(lcdNoticeQueue[tail].row0), "%s", line0);
  snprintf(lcdNoticeQueue[tail].row1, sizeof(lcdNoticeQueue[tail].row1), "%s", line1);
  lcdNoticeQueue[tail].durationMs = (uint16_t)min(durationMs, 65535UL);
  ++lcdNoticeCount;
}

bool showNextQueuedDisplayNotice() {
  if (lcdNoticeCount == 0) return false;
  const QueuedLcdNotice next = lcdNoticeQueue[lcdNoticeHead];
  lcdNoticeHead = (uint8_t)((lcdNoticeHead + 1U) % 4U);
  --lcdNoticeCount;
  showDisplayNotice(next.row0, next.row1, next.durationMs);
  return true;
}

// -------------------- BLUETOOTH SPP + PERSISTENT EVENT LOG --------------------
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED) || !defined(CONFIG_BT_SPP_ENABLED)
#error "BluetoothSerial SPP requires an original ESP32 chip with Bluetooth Classic SPP support."
#endif
BluetoothSerial SerialBT;
static const char *BT_DEVICE_NAME = "HT-MayApTrung";
static constexpr uint8_t LOG_CAPACITY = 32;
static constexpr size_t LOG_TEXT_LEN = 44;
enum LogCategory : uint8_t { LOG_POWER=0, LOG_SENSOR=1, LOG_HEATER=2, LOG_FAN=3,
  LOG_ALARM=4, LOG_NETWORK=5, LOG_CONTROL=6, LOG_BLUETOOTH=7 };
struct LogEntry { uint32_t epoch; uint8_t category; char message[LOG_TEXT_LEN]; };
Preferences logPreferences;
Preferences networkPreferences;
LogEntry logEntries[LOG_CAPACITY];
uint8_t logWriteIndex=0, logEntryCount=0;
volatile int16_t pendingLogClearCategory=-2; // -2 none, -1 all, 0..7 category
volatile uint32_t pendingEpochSync=0;
volatile bool pendingLogsRefresh=false;
volatile uint8_t pendingMq2Notice=0;
bool btAuthenticated=false, btWasConnected=false;
bool btStateChangedPending=false;
bool btStartedLong=false;
bool btMemoryReleased=false;
bool btReleaseWaitLogged=false;
uint32_t btSetupStartedMs=0;
static constexpr uint32_t BT_SETUP_WINDOW_MS = 300000UL; // 5 phut sau lan hoat dong Bluetooth gan nhat
bool wifiProvisioningPending=false;   // da nhan SSID/password, dang cho ESP32 vao mang moi
bool wifiProvisioningConnected=false; // Wi-Fi moi da ket noi thanh cong
bool staWasConnected=false;
bool staConfigAvailable=false;
char pendingWifiSsid[33] = "";
char pendingWifiPassword[64] = "";
volatile bool pendingWifiSave = false;
bool lastLoggedPower=true, lastLoggedHeater=false, lastLoggedFan=false;
bool lastLoggedBuzzerEnabled=true, lastLoggedSensorValid=false;
bool lastLoggedMq2Enabled=true;
uint8_t lastLoggedAlarmMask=0, alarmLogCandidateMask=0;
uint32_t alarmLogCandidateSinceMs=0;
uint32_t lastLcdTopPageMs=0, lastLcdStatusMs=0, lastLcdScrollMs=0;
uint8_t lcdTopPage=1, mq2NoticeMode=0, lcdScrollOffset=0;
uint32_t mq2NoticeUntilMs=0;
char btRxLine[MAX_MESSAGE_BYTES+1];
size_t btRxLength=0;

uint32_t currentEpoch() {
  time_t t=time(nullptr);
  return (t>=1760000000 && t<4102444800LL)?(uint32_t)t:0;
}
void saveLogHistory() {
  logPreferences.putBytes("entries",logEntries,sizeof(logEntries));
  logPreferences.putUChar("head",logWriteIndex);
  logPreferences.putUChar("count",logEntryCount);
}
void loadLogHistory() {
  logPreferences.begin("ht-logs",false);
  if(logPreferences.getBytesLength("entries")==sizeof(logEntries)) {
    logPreferences.getBytes("entries",logEntries,sizeof(logEntries));
    logWriteIndex=logPreferences.getUChar("head",0)%LOG_CAPACITY;
    logEntryCount=min((uint8_t)LOG_CAPACITY,logPreferences.getUChar("count",0));
  } else memset(logEntries,0,sizeof(logEntries));
}
void addLog(uint8_t category,const char *message) {
  LogEntry &e=logEntries[logWriteIndex];
  e.epoch=currentEpoch(); e.category=category;
  snprintf(e.message,sizeof(e.message),"%s",message?message:"");
  logWriteIndex=(logWriteIndex+1)%LOG_CAPACITY;
  if(logEntryCount<LOG_CAPACITY)++logEntryCount;
  saveLogHistory();
  publishLogEvent(category, e.message);
  pendingLogsRefresh = true; // coalesce changes and send one bounded log snapshot
  Serial.printf("LOG[%u] %s%s\n",category,e.epoch?"":"[CHUA DONG BO GIO] ",e.message);
}
void clearLogHistory(int category) {
  LogEntry kept[LOG_CAPACITY]; uint8_t count=0;
  const uint8_t oldest=(logWriteIndex+LOG_CAPACITY-logEntryCount)%LOG_CAPACITY;
  for(uint8_t i=0;i<logEntryCount;++i) {
    const LogEntry &e=logEntries[(oldest+i)%LOG_CAPACITY];
    if(category<0 || e.category!=(uint8_t)category) kept[count++]=e;
  }
  memset(logEntries,0,sizeof(logEntries));
  for(uint8_t i=0;i<count;++i)logEntries[i]=kept[i];
  logEntryCount=count; logWriteIndex=count%LOG_CAPACITY;
  saveLogHistory();
  Serial.println(category<0?"Log history cleared":"Selected log category cleared");
}
void syncClockFromPhone(uint32_t epoch) {
  if(epoch<1760000000UL||epoch>4102444800UL)return;
  timeval tv; tv.tv_sec=(time_t)epoch; tv.tv_usec=0;
  settimeofday(&tv,nullptr); setenv("TZ","ICT-7",1); tzset();
}
void sendLogsToBluetooth() {
  if(btMemoryReleased || !btAuthenticated || !SerialBT.hasClient())return;
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<2200> doc;
#endif
  JsonArray arr=doc.createNestedArray("logs");
  const uint8_t sendCount=min((uint8_t)12,logEntryCount);
  const uint8_t oldest=(logWriteIndex+LOG_CAPACITY-sendCount)%LOG_CAPACITY;
  for(uint8_t i=0;i<sendCount;++i){
    const LogEntry &e=logEntries[(oldest+i)%LOG_CAPACITY];
    JsonObject o=arr.createNestedObject();o["ts"]=e.epoch;o["category"]=e.category;o["message"]=e.message;
  }
  char out[1800];size_t n=serializeJson(doc,out,sizeof(out));if(n>0&&n<sizeof(out))SerialBT.println(out);
}
bool applyControlCommand(JsonDocument &doc) {
  bool accepted=false;
  portENTER_CRITICAL(&controlMux);
  if(doc["system_power"].is<bool>()){control.systemPower=doc["system_power"].as<bool>();accepted=true;}
  if(doc["buzzer_enabled"].is<bool>()){control.buzzerMuted=!doc["buzzer_enabled"].as<bool>();accepted=true;}
  if(doc["heater_override"].is<int>()){int v=doc["heater_override"].as<int>();if(v>=-1&&v<=1){control.heaterRemoteOverride=(int8_t)v;pendingClearHeaterLocal=true;accepted=true;}}
  if(doc["fan_override"].is<int>()){int v=doc["fan_override"].as<int>();if(v>=-1&&v<=1){control.fanRemoteOverride=(int8_t)v;pendingClearFanLocal=true;accepted=true;}}
  if(doc["mq2_alarm_enabled"].is<bool>()){
    bool before=control.mq2AlarmEnabled;control.mq2AlarmEnabled=doc["mq2_alarm_enabled"].as<bool>();
    if(before!=control.mq2AlarmEnabled)pendingMq2Notice=control.mq2AlarmEnabled?1:2;
    accepted=true;
  }
  if(doc["wifi_config"].is<JsonObject>()) {
    const char *ssid=doc["wifi_config"]["ssid"]|"";
    const char *password=doc["wifi_config"]["password"]|"";
    if(strlen(ssid)>0 && strlen(ssid)<=32 && strlen(password)<=63) {
      strlcpy(pendingWifiSsid,ssid,sizeof(pendingWifiSsid));
      strlcpy(pendingWifiPassword,password,sizeof(pendingWifiPassword));
      pendingWifiSave=true;
      wifiProvisioningPending=true;
      wifiProvisioningConnected=false;
      accepted=true;
    }
  }
  portEXIT_CRITICAL(&controlMux);
  if(doc["time_sync"].is<uint32_t>()){pendingEpochSync=doc["time_sync"].as<uint32_t>();accepted=true;}
  if(doc["logs_get"].is<bool>()&&doc["logs_get"].as<bool>()){pendingLogsRefresh=true;accepted=true;}
  if(doc["logs_clear"].is<int>()){int v=doc["logs_clear"].as<int>();if(v>=-1&&v<=7){pendingLogClearCategory=(int16_t)v;accepted=true;}}
  return accepted;
}
void processBluetoothLine(const char *line) {
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<1024> doc;
#endif
  if(deserializeJson(doc,line)){SerialBT.println("{\"ok\":false,\"error\":\"invalid_json\"}");return;}
  if(!btAuthenticated){
    const char *u=doc["login"]["username"]|"";const char *p=doc["login"]["password"]|"";
    if(strcmp(u,BT_SETUP_USER)==0&&strcmp(p,BT_SETUP_PASSWORD)==0){btAuthenticated=true;SerialBT.println("{\"login_ok\":true,\"transport\":\"bluetooth\"}");btStateChangedPending=true;}
    else SerialBT.println("{\"login_ok\":false,\"error\":\"Sai thong tin Bluetooth\"}");
    return;
  }
  SerialBT.println(applyControlCommand(doc)?"{\"ok\":true}":"{\"ok\":false,\"error\":\"no_valid_fields\"}");
}
void processBluetoothInput() {
  if (btMemoryReleased || !btStarted) return;
  const bool connected=SerialBT.hasClient();
  if(connected!=btWasConnected){
    btWasConnected=connected;
    btAuthenticated=false;
    btRxLength=0;
    btStateChangedPending=true;
    btSetupStartedMs=millis(); // Moi lan dien thoai ket noi/ngat ket noi deu gia han cua so cau hinh
  }
  if(!connected)return;
  while(SerialBT.available()){
    char c=(char)SerialBT.read();
    if(c=='\n'||c=='\r'){
      if(btRxLength){btRxLine[btRxLength]='\0';processBluetoothLine(btRxLine);btRxLength=0;}
    } else if(btRxLength<sizeof(btRxLine)-1)btRxLine[btRxLength++]=c;
    else {btRxLength=0;SerialBT.println("{\"ok\":false,\"error\":\"message_too_long\"}");}
  }
}

void forceAllOutputsOff() {
  // Hard reassert each inactive pin; do not rely only on cached state flags.
  writeRelayPin(RELAY_HEATER_PIN, false);
  writeRelayPin(RELAY_FAN_PIN, false);
  heaterRelayOn = false;
  fanRelayOn = false;
  setBuzzer(false);
  digitalWrite(BUZZER_PIN, buzzerOffLevel());
  buzzerOutputOn = false;
  heaterRequestedOn = false;
  fanRequestedOn = false;
  portENTER_CRITICAL(&controlMux);
  control.heaterState = false;
  control.fanState = false;
  portEXIT_CRITICAL(&controlMux);
}

// -------------------- DHT11 / SENSOR SUPERVISION --------------------
void pollDHTSensor() {
  const ControlState s = getControlSnapshot();
  if (!s.systemPower) return; // system OFF: stop sensor polling as requested

  const uint32_t now = millis();
  if ((uint32_t)(now - lastDhtReadMs) < DHT_INTERVAL_MS) return;
  lastDhtReadMs = now;

  // Adafruit DHT library caches the physical frame across these two calls.
  const float h = dht.readHumidity();
  const float t = dht.readTemperature();
  const uint32_t sampleMs = millis();
  const bool valid = isfinite(t) && isfinite(h) && t >= 0.0f && t <= 50.0f && h >= 0.0f && h <= 100.0f;

  if (valid) {
    currentTemp = t;
    currentHum = h;
    lastGoodSensorMs = sampleMs;
    sensorValid = true;
    dhtConsecutiveFailures = 0;
    dhtFaultLogged = false;

    if (currentTemp >= HARD_TEMP_CUTOFF_C) overtempLatched = true;
    else if (overtempLatched && currentTemp <= (HARD_TEMP_CUTOFF_C - 1.0f)) overtempLatched = false;

    // AUTO heater hysteresis is updated only from a newly accepted sample.
    if (currentTemp < HEATER_ON_BELOW_C) autoHeaterState = true;
    else if (currentTemp >= HEATER_OFF_AT_C) autoHeaterState = false;

    // Fan alarm thresholds from user specification.
    const ControlState cs = getControlSnapshot();
    const bool gasAlarm = cs.mq2AlarmEnabled && cs.mq2AlarmRaw > 0 && mq2Raw > cs.mq2AlarmRaw;
    autoFanState = (currentTemp >= HEATER_OFF_AT_C) || (currentHum >= HUM_HIGH_ALARM_PCT) || gasAlarm;

    Serial.printf("DHT OK: T=%.1f C H=%.0f%%\n", currentTemp, currentHum);
  } else {
    if (dhtConsecutiveFailures < 255) ++dhtConsecutiveFailures;
    Serial.printf("WARN: DHT invalid, consecutive=%u\n", (unsigned)dhtConsecutiveFailures);
  }
}

void updateSensorFreshness() {
  const ControlState s = getControlSnapshot();
  if (!s.systemPower) {
    sensorValid = false;
    return;
  }
  const uint32_t now = millis();
  sensorValid = (lastGoodSensorMs != 0) && ((uint32_t)(now - lastGoodSensorMs) <= SENSOR_STALE_MS);
  if (!sensorValid && !dhtFaultLogged) {
    // Drop local/remote force requests on a sustained sensor fault. They must not
    // unexpectedly re-energize a load when the sensor later recovers.
    autoHeaterState = false;
    autoFanState = false;
    resetLocalOverrides();
    Serial.printf("ERROR: LOI DHT11 - no valid sample / stale %lu ms. All controlled outputs OFF.\n",
                  (unsigned long)(lastGoodSensorMs ? now - lastGoodSensorMs : now));
    dhtFaultLogged = true;
  }
}

// -------------------- CONTROL DECISION / RELAYS --------------------
void expireLocalOverrides() {
  const uint32_t now = millis();
  bool clearHeater = false;
  bool clearFan = false;
  portENTER_CRITICAL(&controlMux);
  clearHeater = pendingClearHeaterLocal;
  clearFan = pendingClearFanLocal;
  pendingClearHeaterLocal = false;
  pendingClearFanLocal = false;
  portEXIT_CRITICAL(&controlMux);

  if (clearHeater) heaterLocalOverrideActive = false;
  if (clearFan) fanLocalOverrideActive = false;
  bool localExpired = false;
  if (heaterLocalOverrideActive && (int32_t)(now - heaterLocalUntilMs) >= 0) {
    heaterLocalOverrideActive = false;
    localExpired = true;
  }
  if (fanLocalOverrideActive && (int32_t)(now - fanLocalUntilMs) >= 0) {
    fanLocalOverrideActive = false;
    localExpired = true;
  }
  if (localExpired) {
    displayBlinkVisible = false;
    lastLcdUpdateMs = now - LCD_UPDATE_MS;
  }
  portENTER_CRITICAL(&controlMux);
  control.manualMode = heaterLocalOverrideActive || fanLocalOverrideActive;
  portEXIT_CRITICAL(&controlMux);
}

void updateHeaterOutput() {
  const ControlState s = getControlSnapshot();
  if (!s.systemPower || !sensorValid || overtempLatched || currentTemp >= HARD_TEMP_CUTOFF_C) {
    heaterRequestedOn = false;
  } else if (s.heaterRemoteOverride == 0) {
    heaterRequestedOn = false;
  } else if (s.heaterRemoteOverride == 1) {
    // Remote force ON bypasses normal 36.5/37.5 hysteresis, but not hard safety cutoffs.
    heaterRequestedOn = (currentTemp < HEATER_OFF_AT_C);
  } else if (heaterLocalOverrideActive) {
    heaterRequestedOn = heaterLocalTarget && (currentTemp < HEATER_OFF_AT_C);
  } else {
    heaterRequestedOn = autoHeaterState;
  }

  setHeaterRelay(heaterRequestedOn);
  portENTER_CRITICAL(&controlMux);
  control.heaterState = heaterRelayOn;
  portEXIT_CRITICAL(&controlMux);
}

void updateFanOutput() {
  const ControlState s=getControlSnapshot();
  const bool gasAlarm=s.mq2AlarmEnabled&&s.mq2AlarmRaw>0&&mq2Raw>s.mq2AlarmRaw;
  if(!s.systemPower||!sensorValid)fanRequestedOn=false;
  else if(s.fanRemoteOverride==0)fanRequestedOn=false;
  else if(s.fanRemoteOverride==1)fanRequestedOn=true;
  else if(fanLocalOverrideActive)fanRequestedOn=fanLocalTarget;
  else fanRequestedOn=(currentTemp>=HEATER_OFF_AT_C)||(currentHum>=HUM_HIGH_ALARM_PCT)||gasAlarm;
  setFanRelay(fanRequestedOn);
  portENTER_CRITICAL(&controlMux);control.fanState=fanRelayOn;portEXIT_CRITICAL(&controlMux);
}

bool environmentAlarmActive() {
  if(!sensorValid)return false;
  const ControlState s=getControlSnapshot();
  const bool gasAlarm=s.mq2AlarmEnabled&&s.mq2AlarmRaw>0&&mq2Raw>s.mq2AlarmRaw;
  // Only these three conditions can sound the buzzer per the latest requirement.
  return currentTemp<TEMP_LOW_ALARM_C || currentTemp>=HEATER_OFF_AT_C || gasAlarm;
}

void updateBuzzer() {
  const ControlState s=getControlSnapshot();
  if(!s.systemPower||!sensorValid||s.buzzerMuted||!environmentAlarmActive()){setBuzzer(false);return;}
  setBuzzer((millis()%600U)<300U); // 300 ms ON / 300 ms OFF
}


// -------------------- BUTTONS --------------------
void handleButtonPress(uint8_t index) {
  const uint32_t now = millis();
  if (index == 0) {
    portENTER_CRITICAL(&controlMux);
    control.systemPower = !control.systemPower;
    portEXIT_CRITICAL(&controlMux);
    powerTransitionPhysicalPending = true;
    return;
  }

  if (index == 1) {
    bool muted;
    portENTER_CRITICAL(&controlMux);
    control.buzzerMuted = !control.buzzerMuted;
    muted = control.buzzerMuted;
    portEXIT_CRITICAL(&controlMux);
    if (muted) {
      // Immediate physical mute; setBuzzer(false) always writes the OFF level.
      setBuzzer(false);
      digitalWrite(BUZZER_PIN, buzzerOffLevel());
      buzzerOutputOn = false;
    }
    showDisplayNotice("CONG TAC COI:", muted ? "DA TAT" : "DA BAT", DISPLAY_NOTICE_MS);
    return;
  }

  const ControlState s = getControlSnapshot();
  if (!s.systemPower || !sensorValid) return;

  if (index == 2) { // Fan local override for 5 seconds
    const bool newTarget = !fanRelayOn;
    fanLocalOverrideActive = true;
    fanLocalTarget = newTarget;
    fanLocalUntilMs = now + LOCAL_OVERRIDE_MS;
    portENTER_CRITICAL(&controlMux);
    control.fanRemoteOverride = -1; // physical action takes priority over previous remote force
    control.manualMode = true;
    portEXIT_CRITICAL(&controlMux);
    showDisplayNotice("QUAT THONG GIO:", newTarget ? "DANG BAT" : "DA TAT", DISPLAY_NOTICE_MS);
    displayBlinkVisible = true;
    Serial.printf("LOCAL FAN override=%s for 5000 ms\n", newTarget ? "ON" : "OFF");
    return;
  }

  if (index == 3) { // Heater local override for 5 seconds
    const bool newTarget = !heaterRelayOn;
    heaterLocalOverrideActive = true;
    heaterLocalTarget = newTarget;
    heaterLocalUntilMs = now + LOCAL_OVERRIDE_MS;
    portENTER_CRITICAL(&controlMux);
    control.heaterRemoteOverride = -1;
    control.manualMode = true;
    portEXIT_CRITICAL(&controlMux);
    showDisplayNotice("DEN SUOI:", newTarget ? "DANG BAT" : "DA TAT", DISPLAY_NOTICE_MS);
    displayBlinkVisible = true;
    Serial.printf("LOCAL HEATER override=%s for 5000 ms\n", newTarget ? "ON" : "OFF");
  }
}

void handleButtons() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < (sizeof(buttons) / sizeof(buttons[0])); ++i) {
    const bool raw = digitalRead(buttons[i].pin);
    if (raw != buttons[i].lastRawLevel) {
      buttons[i].lastRawLevel = raw;
      buttons[i].lastChangeMs = now;
    }
    if ((uint32_t)(now - buttons[i].lastChangeMs) >= BUTTON_DEBOUNCE_MS && raw != buttons[i].stableLevel) {
      buttons[i].stableLevel = raw;
      if (buttons[i].stableLevel == LOW) handleButtonPress(i);
    }
  }
}

void sendTelemetryToBluetooth() {
  if (btMemoryReleased || !btAuthenticated || !SerialBT.hasClient()) return;
  const ControlState s = getControlSnapshot();
  const bool gasAlarm = s.mq2AlarmEnabled && s.mq2AlarmRaw > 0 && mq2Raw > s.mq2AlarmRaw;
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<640> doc;
#endif
  doc["sensor_ok"] = sensorValid;
  if (sensorValid) { doc["temp"] = currentTemp; doc["hum"] = currentHum; }
  else { doc["temp"] = nullptr; doc["hum"] = nullptr; }
  doc["power"] = s.systemPower;
  doc["heater_on"] = heaterRelayOn;
  doc["fan_on"] = fanRelayOn;
  doc["heater_source"] = (s.heaterRemoteOverride == 1) ? "REMOTE_ON" : (s.heaterRemoteOverride == 0) ? "REMOTE_OFF" : heaterLocalOverrideActive ? "LOCAL_5S" : "AUTO";
  doc["fan_source"] = (s.fanRemoteOverride == 1) ? "REMOTE_ON" : (s.fanRemoteOverride == 0) ? "REMOTE_OFF" : fanLocalOverrideActive ? "LOCAL_5S" : "AUTO";
  doc["buzzer_enabled"] = !s.buzzerMuted;
  doc["buzzer_on"] = buzzerOutputOn;
  doc["mq2_raw"] = mq2Raw;
  doc["mq2_enabled"] = s.mq2AlarmEnabled;
  doc["gas_alarm"] = gasAlarm;
  doc["overtemp"] = overtempLatched;
  char out[700]; const size_t n = serializeJson(doc, out, sizeof(out));
  if (n > 0 && n < sizeof(out)) SerialBT.println(out);
}

// -------------------- LCD STATUS --------------------
void lcdPrintScroll(uint8_t row, const char *message, bool allowScroll) {
  const size_t len = strlen(message);
  char visible[17];
  if (len <= 16) snprintf(visible, sizeof(visible), "%-16s", message);
  else if (!allowScroll) { memcpy(visible, message, 16); visible[16] = '\0'; }
  else {
    if (millis() - lastLcdScrollMs >= 300) {
      lastLcdScrollMs = millis(); lcdScrollOffset = (uint8_t)((lcdScrollOffset + 1) % (len + 3));
    }
    for (uint8_t i=0; i<16; ++i) { size_t pos=(lcdScrollOffset+i)%(len+3); visible[i]=(pos<len)?message[pos]:' '; }
    visible[16]='\0';
  }
  lcd.setCursor(0,row); lcd.print(visible);
}

uint8_t currentAlarmMask() {
  if (!sensorValid) return 0;
  const ControlState s=getControlSnapshot(); uint8_t mask=0;
  if (currentTemp<TEMP_LOW_ALARM_C) mask|=0x01;
  if (currentTemp>=HEATER_OFF_AT_C) mask|=0x02;
  if (s.mq2AlarmEnabled && s.mq2AlarmRaw>0 && mq2Raw>s.mq2AlarmRaw) mask|=0x04;
  if (currentHum>=HUM_HIGH_ALARM_PCT) mask|=0x08;
  if (currentHum<=HUM_LOW_ALARM_PCT) mask|=0x10;
  return mask;
}

const char *currentLcdStatus() {
  if (!sensorValid) return "LOI DHT11!";
  const ControlState s=getControlSnapshot();
  const bool gasAlarm=s.mq2AlarmEnabled && s.mq2AlarmRaw>0 && mq2Raw>s.mq2AlarmRaw;
  if (currentTemp>=HEATER_OFF_AT_C) return "QUA NHIET!!!";
  if (gasAlarm) return "CO KHI DOC!";
  if (currentTemp<TEMP_LOW_ALARM_C) return "NHIET DO THAP!";
  if (currentHum>=HUM_HIGH_ALARM_PCT) return "DO AM CAO!";
  if (currentHum<=HUM_LOW_ALARM_PCT) return "DO AM THAP!";
  if (!mqttClient.connected()) return "NO CLOUD LINK";
  if ((heaterLocalOverrideActive||fanLocalOverrideActive)&&displayBlinkVisible) return "VE CD THU CONG";
  if (heaterRelayOn) return "DANG SUOI";
  return "AN TOAN";
}

void updateLCD() {
  const uint32_t now=millis(); const ControlState s=getControlSnapshot();
  if (!s.systemPower) {
    if (!powerOffScreenDrawn) { lcd.setCursor(0,0);lcd.print("HE THONG: TAT   ");lcd.setCursor(0,1);lcd.print("                ");powerOffScreenDrawn=true; }
    return; // freeze LCD and stop sensor polling while system is OFF
  }
  powerOffScreenDrawn=false;
  if ((int32_t)(displayNoticeUntilMs-now)>0) {
    if (!displayNoticeDrawn) { lcdScrollOffset=0;lastLcdScrollMs=now;lcdPrintScroll(0,displayNotice0,true);lcdPrintScroll(1,displayNotice1,false);displayNoticeDrawn=true; }
    else if (now-lastLcdScrollMs>=300) lcdPrintScroll(0,displayNotice0,true);
    return;
  }
  if (displayNoticeUntilMs!=0) {
    displayNoticeUntilMs=0; displayLockUntilMs=0; displayNoticeDrawn=false;
    if (showNextQueuedDisplayNotice()) return;
    lastLcdTopPageMs=now-3000; lastLcdStatusMs=0;
  }
  if (!sensorValid) {
    if (now-lastLcdTopPageMs>=LCD_UPDATE_MS) {
      lastLcdTopPageMs=now;
      lcdPrintScroll(0,"LOI DHT11!",false);
      lcdPrintScroll(1,mqttClient.connected()?"CLOUD CONNECTED":"NO CLOUD LINK",true);
    }
    return;
  }
  // Alternate temperature/humidity and MQ2 raw ADC on row 0 every 3 seconds.
  if (now-lastLcdTopPageMs>=3000U) {
    lastLcdTopPageMs=now;lcdTopPage^=1;char top[17];
    if(lcdTopPage==0)snprintf(top,sizeof(top),"T:%4.1fC H:%2.0f%%",currentTemp,currentHum);
    else snprintf(top,sizeof(top),"MQ2 ADC tho:%u",(unsigned)mq2Raw);
    lcdPrintScroll(0,top,false);
  }
  const bool localOverride=heaterLocalOverrideActive||fanLocalOverrideActive;
  if(localOverride&&now-lastDisplayBlinkMs>=LCD_BLINK_MS){lastDisplayBlinkMs=now;displayBlinkVisible=!displayBlinkVisible;lastLcdStatusMs=0;}
  else if(!localOverride)displayBlinkVisible=false;
  const bool urgent=currentAlarmMask()!=0;
  if(mq2NoticeMode && (int32_t)(mq2NoticeUntilMs-now)>0 && !urgent){
    if(now-lastLcdStatusMs>=250U){lastLcdStatusMs=now;lcdPrintScroll(1,mq2NoticeMode==1?"TRANG THAI: BAT MQ2":"TRANG THAI: TAT MQ2",true);}
    return;
  }
  if(mq2NoticeMode&&(int32_t)(now-mq2NoticeUntilMs)>=0)mq2NoticeMode=0;
  if(now-lastLcdStatusMs>=250U){lastLcdStatusMs=now;lcdPrintScroll(1,currentLcdStatus(),true);}
}

// Event-driven logging only: no log entry for every 300 ms buzzer pulse.
// Alarm transitions must remain stable for 1.2 s before being written, avoiding
// log floods if the MQ2 analog value jitters around the configured threshold.
void recordStateChanges() {
  const ControlState s = getControlSnapshot();
  const uint32_t now = millis();

  if (s.systemPower != lastLoggedPower) {
    addLog(LOG_POWER, s.systemPower ? "He thong BAT" : "He thong TAT");
    lastLoggedPower = s.systemPower;
    if (!s.systemPower) {
      lastLoggedAlarmMask = 0;
      alarmLogCandidateMask = 0;
      alarmLogCandidateSinceMs = now;
    }
  }

  if (heaterRelayOn != lastLoggedHeater) {
    addLog(LOG_HEATER, heaterRelayOn ? "Den suoi BAT" : "Den suoi TAT");
    lastLoggedHeater = heaterRelayOn;
  }
  if (fanRelayOn != lastLoggedFan) {
    addLog(LOG_FAN, fanRelayOn ? "Quat BAT" : "Quat TAT");
    lastLoggedFan = fanRelayOn;
  }

  const bool buzzerEnabled = !s.buzzerMuted;
  if (buzzerEnabled != lastLoggedBuzzerEnabled) {
    addLog(LOG_CONTROL, buzzerEnabled ? "Cho phep coi bao dong" : "Tat tieng coi");
    lastLoggedBuzzerEnabled = buzzerEnabled;
    if (!buzzerEnabled) setBuzzer(false);
  }
  if (s.mq2AlarmEnabled != lastLoggedMq2Enabled) {
    addLog(LOG_CONTROL, s.mq2AlarmEnabled ? "Bat nhan dien MQ2" : "Tat nhan dien MQ2");
    lastLoggedMq2Enabled = s.mq2AlarmEnabled;
  }

  if (s.systemPower && sensorValid != lastLoggedSensorValid) {
    addLog(LOG_SENSOR, sensorValid ? "DHT11 da co du lieu hop le" : "LOI DHT11 - ngat cac ngo ra");
    lastLoggedSensorValid = sensorValid;
  }

  if (!s.systemPower || !sensorValid) {
    alarmLogCandidateMask = 0;
    alarmLogCandidateSinceMs = now;
    return;
  }

  const uint8_t currentMask = currentAlarmMask();
  if (currentMask != alarmLogCandidateMask) {
    alarmLogCandidateMask = currentMask;
    alarmLogCandidateSinceMs = now;
    return;
  }

  if (currentMask == lastLoggedAlarmMask || (uint32_t)(now - alarmLogCandidateSinceMs) < 1200U) return;

  if (currentMask & 0x02) addLog(LOG_ALARM, "QUA NHIET!!!");
  else if (currentMask & 0x04) addLog(LOG_ALARM, "CO KHI DOC! MQ2 vuot nguong ADC");
  else if (currentMask & 0x01) addLog(LOG_ALARM, "NHIET DO THAP");
  else if (currentMask & 0x08) addLog(LOG_ALARM, "DO AM CAO");
  else if (currentMask & 0x10) addLog(LOG_ALARM, "DO AM THAP");
  else addLog(LOG_ALARM, "Canh bao moi truong da het");

  lastLoggedAlarmMask = currentMask;
}

// -------------------- MQ2 --------------------

void updateMQ2() {
  const uint32_t now = millis();
  if ((uint32_t)(now - lastMq2ReadMs) >= MQ2_UPDATE_MS) {
    lastMq2ReadMs = now;
    mq2Raw = (uint16_t)analogRead(MQ2_PIN);
  }
}



void processPendingHostTasks() {
  char newSsid[33] = "";
  char newPassword[64] = "";
  bool applyWifi = false;
  portENTER_CRITICAL(&controlMux);
  if (pendingWifiSave) {
    strlcpy(newSsid, pendingWifiSsid, sizeof(newSsid));
    strlcpy(newPassword, pendingWifiPassword, sizeof(newPassword));
    pendingWifiSave = false;
    applyWifi = true;
  }
  portEXIT_CRITICAL(&controlMux);
  if (applyWifi) {
    networkPreferences.putString("ssid", newSsid);
    networkPreferences.putString("password", newPassword);
    staConfigAvailable = newSsid[0] != '\0';
    staWasConnected = false;
    mqttClient.disconnect();
    WiFi.disconnect(false, false);
    if (staConfigAvailable) {
      WiFi.begin(newSsid, newPassword);
      addLog(LOG_NETWORK, "Da luu Wi-Fi moi; ESP32 dang ket noi router");
    } else {
      addLog(LOG_NETWORK, "Da xoa cau hinh Wi-Fi router");
    }
  }
  const uint32_t epoch=pendingEpochSync;
  if(epoch){pendingEpochSync=0;syncClockFromPhone(epoch);}
  const int16_t clearCategory=pendingLogClearCategory;
  if(clearCategory!=-2){pendingLogClearCategory=-2;clearLogHistory(clearCategory);pendingLogsRefresh=true;}
  if(pendingLogsRefresh){pendingLogsRefresh=false;sendLogsToBluetooth();}
  if(btStateChangedPending){
    btStateChangedPending=false;
    if(btWasConnected)addLog(LOG_BLUETOOTH,"Bluetooth SPP connected");
    else{btAuthenticated=false;addLog(LOG_BLUETOOTH,"Bluetooth SPP disconnected");}
  }
}

// -------------------- NETWORK: STA + MQTT CLOUD --------------------
static constexpr uint32_t MQTT_RETRY_MS = 15000;
uint32_t lastMqttAttemptMs = 0;
void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
  (void)topic;
  if (length == 0 || length > MAX_MESSAGE_BYTES) return;
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<1024> doc;
#endif
  if (deserializeJson(doc, payload, length)) return;
  if (applyControlCommand(doc)) {
    Serial.println("[MQTT] Remote command accepted");
  } else {
    Serial.println("[MQTT] Ignored invalid/empty command");
  }
}

void publishTelemetry() {
  if (!mqttClient.connected()) return;
  const ControlState s = getControlSnapshot();
  const bool gasAlarm = s.mq2AlarmEnabled && s.mq2AlarmRaw > 0 && mq2Raw > s.mq2AlarmRaw;
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<768> doc;
#endif
  doc["device_id"] = DEVICE_ID;
  doc["sensor_ok"] = sensorValid;
  if (sensorValid) { doc["temp"] = currentTemp; doc["hum"] = currentHum; }
  else { doc["temp"] = nullptr; doc["hum"] = nullptr; }
  doc["power"] = s.systemPower;
  doc["manual"] = s.manualMode;
  doc["heater_on"] = heaterRelayOn;
  doc["fan_on"] = fanRelayOn;
  doc["heater_requested"] = heaterRequestedOn;
  doc["fan_requested"] = fanRequestedOn;
  doc["heater_source"] = (s.heaterRemoteOverride == 1) ? "REMOTE_ON" : (s.heaterRemoteOverride == 0) ? "REMOTE_OFF" : heaterLocalOverrideActive ? "LOCAL_5S" : "AUTO";
  doc["fan_source"] = (s.fanRemoteOverride == 1) ? "REMOTE_ON" : (s.fanRemoteOverride == 0) ? "REMOTE_OFF" : fanLocalOverrideActive ? "LOCAL_5S" : "AUTO";
  doc["buzzer_enabled"] = !s.buzzerMuted;
  doc["buzzer_on"] = buzzerOutputOn;
  doc["mq2_raw"] = mq2Raw;
  doc["mq2_enabled"] = s.mq2AlarmEnabled;
  doc["mq2_threshold"] = s.mq2AlarmRaw;
  doc["gas_alarm"] = gasAlarm;
  doc["overtemp"] = overtempLatched;
  doc["updated_at"] = currentEpoch();
  char out[900];
  const size_t n = serializeJson(doc, out, sizeof(out));
  if (n > 0 && n < sizeof(out)) mqttClient.publish(mqttTopicState, out, false);
}

void publishLogEvent(uint8_t category, const char *message) {
  if (!mqttClient.connected()) return;
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<256> doc;
#endif
  doc["device_id"] = DEVICE_ID;
  doc["ts"] = currentEpoch();
  doc["category"] = category;
  doc["message"] = message ? message : "";
  char out[320];
  const size_t n = serializeJson(doc, out, sizeof(out));
  if (n > 0 && n < sizeof(out)) mqttClient.publish(mqttTopicEvents, out, false);
}

void setupWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setHostname("mayaptrung");
  networkPreferences.begin("ht-network", false);

  // Use ONLY credentials stored by the Android app over Bluetooth SPP.
  // Do not fall back to WIFI_SSID/WIFI_PASSWORD from secrets.h.
  String savedSsid = networkPreferences.getString("ssid", "");
  String savedPassword = networkPreferences.getString("password", "");
  staConfigAvailable = savedSsid.length() > 0;
  if (staConfigAvailable) {
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    Serial.printf("[NET] Connecting to Bluetooth-configured Wi-Fi: %s\n", savedSsid.c_str());
  } else {
    Serial.println("[NET] No stored Wi-Fi credentials. Waiting for Bluetooth app configuration.");
  }

  snprintf(mqttTopicState, sizeof(mqttTopicState), "incubator/%s/state", DEVICE_ID);
  snprintf(mqttTopicEvents, sizeof(mqttTopicEvents), "incubator/%s/events", DEVICE_ID);
  snprintf(mqttTopicCommands, sizeof(mqttTopicCommands), "incubator/%s/commands", DEVICE_ID);
  snprintf(mqttTopicAvailability, sizeof(mqttTopicAvailability), "incubator/%s/availability", DEVICE_ID);


  // EMQX Serverless requires MQTT over TLS on port 8883.
  // MQTT_CA_CERT must contain the CA certificate downloaded from this exact
  // EMQX deployment's Overview page (not a certificate for another broker).
  // SNTP is configured only after Wi-Fi reaches WL_CONNECTED in maintainWiFi().
  Serial.println("[NTP] Waiting for Wi-Fi before SNTP setup.");

  mqttTlsClient.setCACert(MQTT_CA_CERT);
  mqttTlsClient.setTimeout(5000);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);
  mqttClient.setSocketTimeout(5);
  mqttClient.setKeepAlive(30);
}

void maintainWiFi() {
  const uint32_t now = millis();
  const bool connected = staConfigAvailable && WiFi.status() == WL_CONNECTED;
  if (connected && !staWasConnected) {
    Serial.printf("[NET] Wi-Fi connected, IP=%s\n", WiFi.localIP().toString().c_str());
    if (wifiProvisioningPending && WiFi.SSID().length() > 0) {
      wifiProvisioningPending = false;
      wifiProvisioningConnected = true;
      Serial.println("[BT] New Wi-Fi credentials connected successfully; Bluetooth will be released after the app disconnects.");
    }
    Serial.printf("[NET] Actual SSID: %s\n", WiFi.SSID().c_str());

Serial.printf("[NET] BSSID: %s\n", WiFi.BSSIDstr().c_str());

Serial.printf("[NET] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());

Serial.printf("[NET] Signal: %d dBm\n", WiFi.RSSI());
    configTzTime("ICT-7", "pool.ntp.org", "time.cloudflare.com");
    addLog(LOG_NETWORK, "ESP32 ket noi router Wi-Fi");
  } else if (!connected && staWasConnected) {
    addLog(LOG_NETWORK, "Mat Wi-Fi router");
  }
  staWasConnected = connected;

  // Print a bounded diagnostic while joining the router; do not flood Serial.
  static uint32_t lastWifiWaitLogMs = 0;
  if (staConfigAvailable && !connected &&
      (uint32_t)(now - lastWifiWaitLogMs) >= 10000UL) {
    lastWifiWaitLogMs = now;
    Serial.printf("[NET] Waiting for Wi-Fi; status=%d\n", (int)WiFi.status());
  }

  // Do not call WiFi.reconnect() periodically while the initial association
  // is still in progress. Arduino-ESP32 may report WL_DISCONNECTED during
  // association; reconnect() forces a disconnect/connect cycle and can emit
  // "sta is connecting, return error". setAutoReconnect(true) plus the
  // framework's STA disconnect handling manages retries without interrupting
  // an in-progress connection attempt.
}

void maintainMqtt() {
  // SPP chi duoc giai phong sau cua so cau hinh 60 giay.
  // Khong bat dau TLS truoc khi Classic BT memory duoc thu hoi.
  if (btStarted && !btMemoryReleased) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  // WiFiClientSecure validates certificate dates. configTzTime() is async,
  // so wait until SNTP has supplied a plausible epoch before TLS handshake.
  const time_t epochNow = time(nullptr);
  if (epochNow < 1700000000) {
    static uint32_t lastTimeWaitMessageMs = 0;
    const uint32_t waitNow = millis();
    if ((uint32_t)(waitNow - lastTimeWaitMessageMs) >= 10000UL) {
      lastTimeWaitMessageMs = waitNow;
      Serial.printf("[NTP] Waiting for clock sync; epoch=%ld\n", (long)epochNow);
    }
    return;
  }

  const uint32_t now = millis();
  if ((uint32_t)(now-lastMqttAttemptMs) < MQTT_RETRY_MS) return;
  lastMqttAttemptMs = now;
  Serial.printf("[MQTT] Connecting to %s:%u\n", MQTT_HOST, (unsigned)MQTT_PORT);
  printMemoryStatus("BEFORE MQTT TLS");
  const bool ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD,
                                     mqttTopicAvailability, 1, true, "offline");
  if (ok) {
    mqttClient.subscribe(mqttTopicCommands, 1);
    mqttClient.publish(mqttTopicAvailability, "online", true);
    addLog(LOG_NETWORK, "Da ket noi MQTT cloud");
    publishTelemetry();
  } else {
    Serial.printf("[MQTT] Connect failed, state=%d\n", mqttClient.state());
    printMemoryStatus("AFTER MQTT TLS FAILED");
    char tlsError[160] = {0};
    int tlsCode = mqttTlsClient.lastError(
        tlsError, sizeof(tlsError)
    );

    Serial.printf(
        "[TLS] lastError=%d, detail=%s\n",
        tlsCode, tlsError
    );

    Serial.printf(
        "[TIME] epoch=%ld, heap=%u\n",
        (long)time(nullptr),
        (unsigned)ESP.getFreeHeap()
    );
  }
}

// -------------------- HARDWARE INIT --------------------
void initHardware() {
  // Set output latches to electrically inactive levels before OUTPUT mode.
  digitalWrite(RELAY_FAN_PIN, (RELAY_ACTIVE_LEVEL == HIGH) ? LOW : HIGH);
  digitalWrite(RELAY_HEATER_PIN, (RELAY_ACTIVE_LEVEL == HIGH) ? LOW : HIGH);
  digitalWrite(BUZZER_PIN, buzzerOffLevel());
  pinMode(RELAY_FAN_PIN, OUTPUT);
  pinMode(RELAY_HEATER_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_FAN_PIN, (RELAY_ACTIVE_LEVEL == HIGH) ? LOW : HIGH);
  digitalWrite(RELAY_HEATER_PIN, (RELAY_ACTIVE_LEVEL == HIGH) ? LOW : HIGH);
  digitalWrite(BUZZER_PIN, buzzerOffLevel());

  pinMode(BTN_POWER, INPUT_PULLUP);
  pinMode(BTN_BUZZER, INPUT_PULLUP);
  pinMode(BTN_FAN, INPUT_PULLUP);
  pinMode(BTN_HEATER, INPUT_PULLUP);
  pinMode(MQ2_PIN, INPUT);
  analogReadResolution(12);

  dht.begin();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // required before lcd.init()
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("HT MAY AP TRUNG");
  lcd.setCursor(0, 1); lcd.print("KHOI DONG...");

  const uint32_t now = millis();
  for (auto &b : buttons) {
    b.stableLevel = digitalRead(b.pin);
    b.lastRawLevel = b.stableLevel;
    b.lastChangeMs = now;
  }
  lastDhtReadMs = now - DHT_INTERVAL_MS; // allow first sample immediately
  lastLcdUpdateMs = now - LCD_UPDATE_MS;
  lastLcdTopPageMs = now - 3000U;
  lastLcdStatusMs = 0;
  lastMq2ReadMs = now;
}

// -------------------- SETUP / LOOP --------------------
void testCACertificate() {
  Serial.println("\n===== TEST CA CERTIFICATE =====");

  size_t certLength = strlen(MQTT_CA_CERT);

  Serial.printf("[CA] Length: %u bytes\n",
                (unsigned)certLength);

  bool hasBegin =
    strstr(MQTT_CA_CERT,
           "-----BEGIN CERTIFICATE-----") != nullptr;

  bool hasEnd =
    strstr(MQTT_CA_CERT,
           "-----END CERTIFICATE-----") != nullptr;

  Serial.printf("[CA] BEGIN: %s\n",
                hasBegin ? "FOUND" : "MISSING");

  Serial.printf("[CA] END: %s\n",
                hasEnd ? "FOUND" : "MISSING");

  mbedtls_x509_crt cert;
  mbedtls_x509_crt_init(&cert);

  int result = mbedtls_x509_crt_parse(
      &cert,
      (const unsigned char *)MQTT_CA_CERT,
      certLength + 1
  );

  Serial.printf("[CA] MBEDTLS RESULT: %d\n", result);

  if (result == 0) {
    Serial.println("[CA] CERTIFICATE PARSE SUCCESS");
  } else {
    Serial.println("[CA] CERTIFICATE PARSE FAILED");
  }

  mbedtls_x509_crt_free(&cert);

  Serial.println("===== END TEST CA =====\n");
}
void printMemoryStatus(const char *stage) {
    const uint32_t caps =
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

    Serial.printf(
        "\n[MEMORY] %s\n"
        "Free heap: %u\n"
        "Minimum heap: %u\n"
        "Internal free: %u\n"
        "Internal largest block: %u\n"
        "Largest 8-bit block: %u\n\n",

        stage,

        (unsigned)ESP.getFreeHeap(),
        (unsigned)ESP.getMinFreeHeap(),

        (unsigned)heap_caps_get_free_size(caps),

        (unsigned)heap_caps_get_largest_free_block(caps),

        (unsigned)heap_caps_get_largest_free_block(
            MALLOC_CAP_8BIT
        )
    );
}
// Chi giai phong Bluetooth khi het cua so cau hinh va dien thoai da ngat SPP.
// Neu dien thoai dang ket noi, firmware cho dien thoai ngat ket noi truoc.
// Sau khi release, muon cau hinh Bluetooth lai thi reboot ESP32.
void serviceBluetoothMemoryRelease() {
  if (!btStarted || btMemoryReleased) return;

  // Neu app vua gui Wi-Fi moi: chi giai phong sau khi ESP32 that su vao duoc mang moi.
  // Neu Wi-Fi sai, giu Bluetooth de app co the gui lai cau hinh.
  if (wifiProvisioningPending) return;

  // Khi da nap Wi-Fi moi thanh cong, cho app dong SPP truoc khi release.
  // Neu khong co cau hinh moi, cho 5 phut sau lan Bluetooth hoat dong cuoi cung.
  const bool provisionedAndConnected = wifiProvisioningConnected;
  const bool setupWindowExpired =
      (uint32_t)(millis() - btSetupStartedMs) >= BT_SETUP_WINDOW_MS;

  if (!provisionedAndConnected && !setupWindowExpired) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (SerialBT.hasClient()) {
    if (!btReleaseWaitLogged) {
      Serial.println("[BT] Waiting for phone to disconnect SPP before releasing Bluetooth memory.");
      btReleaseWaitLogged = true;
    }
    return;
  }
  btReleaseWaitLogged = false;

  Serial.println("[BT] Stopping Bluetooth SPP...");
  SerialBT.end();
  delay(100); // cho controller/host dung hoan toan
  SerialBT.memrelease();

  btMemoryReleased = true;
  btAuthenticated = false;
  btWasConnected = false;
  btStateChangedPending = false;
  btRxLength = 0;

  Serial.println("[BT] Classic Bluetooth memory released; reboot required to use SPP again.");
  printMemoryStatus("AFTER BT CLASSIC RELEASE");
}

void setup() {
  Serial.begin(115200);
  testCACertificate();
  loadLogHistory();
  initHardware();
if (!SerialBT.begin(BT_DEVICE_NAME, false, true)) {
    Serial.println("ERROR: Bluetooth SPP failed to start");
} else {
    btStartedLong = true;
    btSetupStartedMs = millis();
    SerialBT.setPin(BT_SETUP_PIN, strlen(BT_SETUP_PIN));
    Serial.println("Bluetooth SPP ready - BLE disabled");
    Serial.println("[BT] 60-second Wi-Fi setup window started");
}

  Serial.printf("[MEM] after Bluetooth: free=%u min=%u\n",(unsigned)ESP.getFreeHeap(),(unsigned)ESP.getMinFreeHeap());
  setupWiFi();
  Serial.println("HT May ap trung ready. Wi-Fi/MQTT cloud mode.");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  addLog(LOG_POWER, "Firmware khoi dong");
}

void loop() {
  handleButtons();
  processBluetoothInput();
  serviceBluetoothMemoryRelease();
  maintainWiFi();
  maintainMqtt();

  static bool previousSystemPower = true;
  ControlState s = getControlSnapshot();
  const uint32_t loopNow = millis();

  // One central power transition path covers physical buttons and cloud/BT commands.
  if (s.systemPower != previousSystemPower) {
    if (!s.systemPower) {
      resetLocalOverrides();
      lcdNoticeHead = 0;
      lcdNoticeCount = 0;
      displayNoticeUntilMs = 0;
      displayNoticeDrawn = false;
      autoHeaterState = false;
      autoFanState = false;
      sensorValid = false;
      lastGoodSensorMs = 0;
      dhtConsecutiveFailures = 0;
      dhtFaultLogged = false;
      lastLoggedSensorValid = false;
      lastLoggedAlarmMask = 0;
      alarmLogCandidateMask = 0;
      alarmLogCandidateSinceMs = loopNow;
      forceAllOutputsOff();
      if (powerTransitionPhysicalPending) showDisplayNotice("HE THONG: TAT", "TAT CAC NGO RA", POWER_NOTICE_MS);
      Serial.println("POWER OFF: heater, fan and buzzer OFF; DHT/MQ2 polling stopped.");
    } else {
      sensorValid = false;
      lastGoodSensorMs = 0;
      lastDhtReadMs = loopNow - DHT_INTERVAL_MS;
      dhtFaultLogged = false;
      autoHeaterState = false;
      autoFanState = false;
      lcdTopPage = 1;
      lastLcdTopPageMs = loopNow - 3000U;
      lastLoggedAlarmMask = 0;
      alarmLogCandidateMask = 0;
      alarmLogCandidateSinceMs = loopNow;
      if (powerTransitionPhysicalPending) showDisplayNotice("HE THONG: BAT", "DANG KHOI DONG", POWER_NOTICE_MS);
      Serial.println("POWER ON: waiting for a fresh DHT sample before enabling outputs.");
    }
    previousSystemPower = s.systemPower;
    powerTransitionPhysicalPending = false;
    s = getControlSnapshot();
  }

  if (s.systemPower) {
    updateMQ2();
    pollDHTSensor();
    updateSensorFreshness();
    expireLocalOverrides();
    if (!sensorValid) {
      // Sensor invalid/stale: no automatic or forced load command is allowed.
      forceAllOutputsOff();
    } else {
      updateHeaterOutput();
      updateFanOutput();
      updateBuzzer();
    }
  } else {
    sensorValid = false;
    const ControlState offState = getControlSnapshot();
    if (heaterLocalOverrideActive || fanLocalOverrideActive || offState.manualMode ||
        offState.heaterRemoteOverride != -1 || offState.fanRemoteOverride != -1) resetLocalOverrides();
    forceAllOutputsOff();
  }

  processPendingHostTasks();
  recordStateChanges();
  updateLCD();

  const uint32_t now = millis();
  if ((uint32_t)(now - lastTelemetryPublishMs) >= TELEMETRY_UPDATE_MS) {
    lastTelemetryPublishMs = now;
    publishTelemetry();
    sendTelemetryToBluetooth();
  }
  if ((uint32_t)(now - lastLogMs) >= LOG_INTERVAL_MS) {
    lastLogMs = now;
    const ControlState cs = getControlSnapshot();
    Serial.printf(
      "HEALTH heap=%u minheap=%u power=%s sensor=%s T=%.1f H=%.0f heater_req=%s heater_out=%s fan_req=%s fan_out=%s buzzer=%s MQ2=%u gas=%s STA=%s MQTT=%u BT=%s\n",
      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), cs.systemPower ? "ON" : "OFF", sensorValid ? "OK" : "ERR",
      sensorValid ? currentTemp : NAN, sensorValid ? currentHum : NAN,
      heaterRequestedOn ? "ON" : "OFF", heaterRelayOn ? "ON" : "OFF",
      fanRequestedOn ? "ON" : "OFF", fanRelayOn ? "ON" : "OFF",
      buzzerOutputOn ? "ON" : "OFF", mq2Raw,
      (cs.mq2AlarmEnabled && mq2Raw > cs.mq2AlarmRaw) ? "ALARM" : "OK",
      WiFi.status() == WL_CONNECTED ? "CONNECTED" : "NO_ROUTER",
      mqttClient.connected() ? 1 : 0,
      btMemoryReleased ? "RELEASED" : (btAuthenticated ? "AUTH" : (btWasConnected ? "CONNECTED" : "NO_CLIENT")));
  }
}

