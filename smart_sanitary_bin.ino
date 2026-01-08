#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include "time.h"

// --- Configuration ---
const char* ssid = "cslab";
const char* password = "aksesg31";
const char* cloudBridgeUrl = "https://process-bin-data-238356769828.us-central1.run.app";
const char* googleApiKey = "AIzaSyBZ_0AJTeRsy8QkDaf3Xzs99yCoPNWpdvc";

// Constraints
const int BIN_HEIGHT = 26.2;
const int FULL_THRESHOLD = 6;

// Pins
const int LID_TRIG = 5;
const int LID_ECHO = 18;
const int LEVEL_TRIG = 17;
const int LEVEL_ECHO = 4;
const int SERVO_PIN = 16;
const int DHTPIN = 27;
const int LED_GREEN = 32;
const int LED_YELLOW = 33;
const int LED_RED = 25;

#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
Servo binServo;

// --- Global Variables ---
float binLat = 0.0, binLon = 0.0;
unsigned long lastCloudUpdate = 0;
unsigned long lastCommandCheck = 0;
bool systemReady = false;
bool fullNotified = false;

// --- Lock Logic ---
bool binLocked = false;
unsigned long lastUnlockTime = 0;
const unsigned long GRACE_PERIOD = 15000;

// --- Remote Unlock State Machine ---
bool unlockRequested = false;
bool unlocking = false;
unsigned long unlockStart = 0;
const unsigned long UNLOCK_DURATION = 5000;

// --- Timing ---
unsigned long commandInterval = 5000;

// --- Function Prototypes ---
void readAndUploadSensors(const char* reason);
void getWiFiTriangulation();
long readDistance(int trig, int echo);
void checkWebCommands();
void updateLEDs(int percentage);
void syncTimeBypassNTP();

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✓ CONNECTED!");

  syncTimeBypassNTP();

  pinMode(LID_TRIG, OUTPUT); pinMode(LID_ECHO, INPUT);
  pinMode(LEVEL_TRIG, OUTPUT); pinMode(LEVEL_ECHO, INPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  dht.begin();
  ESP32PWM::allocateTimer(0);
  binServo.setPeriodHertz(50);
  binServo.attach(SERVO_PIN, 500, 2400);
  binServo.write(0);

  Serial.println("Getting location...");
  getWiFiTriangulation();

  Serial.println("Establishing handshake...");
  readAndUploadSensors("System boot / handshake");

  systemReady = true;
  Serial.println("\n=== SYSTEM READY ===\n");
}

// ================= LOOP =================
void loop() {
  long lidDist = readDistance(LID_TRIG, LID_ECHO);

  // ---- Cloud command check (ONLY when bin is locked / full) ----
  if (binLocked) {
    commandInterval = 3000;
    if (millis() - lastCommandCheck >= commandInterval) {
      Serial.println("☁ Bin FULL → checking cloud commands...");
      checkWebCommands();
      lastCommandCheck = millis();
    }
  } else {
    lastCommandCheck = millis();
  }

  // ---- Remote unlock handling ----
  if (unlockRequested && !unlocking) {
    Serial.println("🔓 Remote unlock: opening lid");
    binServo.write(120);
    unlockStart = millis();
    unlocking = true;
    unlockRequested = false;
  }

  if (unlocking && millis() - unlockStart >= UNLOCK_DURATION) {
    binServo.write(0);
    delay(2000);
    Serial.println("🔒 Remote unlock finished, lid is closed.");
    unlocking = false;
    readAndUploadSensors("Remote unlock completed");
  }

  // ---- Proximity-based lid control ----
  if (!unlocking && lidDist > 1 && lidDist < 15) {
    Serial.println("🚶 Object detected near bin");
    if (binLocked) {
      Serial.println("❌ Bin locked (FULL)");
      digitalWrite(LED_RED, LOW);
      delay(100);
      digitalWrite(LED_RED, HIGH);
    } else {
      Serial.println("✅ Opening lid");
      binServo.write(120);
      delay(3000);
      binServo.write(0);
      delay(2000);
      Serial.println("🔒 Lid closed");
      readAndUploadSensors("Proximity lid open");
    }
  }

  // OPTIONAL: Add this inside your loop() if you want the bin to stay "Active" when idle
  if (millis() - lastCloudUpdate >= 60000) { // Every 60 seconds
      readAndUploadSensors("Periodic Heartbeat");
  }

  delay(200); // 0.2s
}

// ================= SENSOR UPLOAD =================
void readAndUploadSensors(const char* reason) {
  Serial.println("\n📡 SENSOR UPDATE START");
  Serial.printf("Reason: %s\n", reason);

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  long trashLevel = readDistance(LEVEL_TRIG, LEVEL_ECHO);

  int fillPerc = map(trashLevel, BIN_HEIGHT, FULL_THRESHOLD, 0, 100);
  fillPerc = constrain(fillPerc, 0, 100);

  Serial.printf("🗑 Trash level: %ld cm (%d%%)\n", trashLevel, fillPerc);
  Serial.printf("🌡 Temperature: %.1f °C\n", t);
  Serial.printf("💧 Humidity: %.1f %%\n", h);

  // --- Compose JSON with fill_percentage ---
  StaticJsonDocument<256> doc;
  doc["device_id"] = "ESP32_BIN_01";
  doc["trash_level_cm"] = trashLevel;
  doc["temp_c"] = t;
  doc["humidity_pct"] = h;
  doc["lat"] = binLat;
  doc["lon"] = binLon;
  doc["fill_percentage"] = fillPerc;   // <--- important

  // --- Upload to Cloud ---
  String body;
  serializeJson(doc, body);
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  if (http.begin(client, cloudBridgeUrl)) {
      http.addHeader("Content-Type", "application/json");
      int code = http.POST(body);
      Serial.printf("☁ Cloud response: %d\n", code);
      http.end();
  }

  // Lock logic
  bool inGrace = (millis() - lastUnlockTime < GRACE_PERIOD);
  binLocked = (fillPerc >= 95 && !inGrace);
  Serial.printf("🔒 Bin locked: %s\n", binLocked ? "YES" : "NO");
  updateLEDs(fillPerc);

  // Notify cloud ONLY ONCE when bin becomes full
  if (binLocked && !fullNotified) {
    Serial.println("🚨 Bin just became FULL → notifying workers");
    fullNotified = true;
  } 

  // Reset flag when bin is no longer full
  if (!binLocked) fullNotified = false;

  lastCloudUpdate = millis();
  Serial.println("📡 SENSOR UPDATE END\n");
}

// ================= CHECK CLOUD COMMANDS =================
void checkWebCommands() {
  if (WiFi.status() != WL_CONNECTED) return;

  digitalWrite(LED_YELLOW, HIGH);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (http.begin(client, cloudBridgeUrl)) {
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<128> doc;
    doc["device_id"] = "ESP32_BIN_01";
    doc["command_check"] = true;

    String body;
    serializeJson(doc, body);
    int code = http.POST(body);
    Serial.printf("Checking cloud commands, HTTP code: %d\n", code);

    if (code == 200) {
      String payload = http.getString();
      Serial.printf("Payload: %s\n", payload.c_str());
      if (payload.indexOf("RESET") >= 0) {
        Serial.println("🔓 REMOTE UNLOCK RECEIVED");
        unlockRequested = true;
        lastUnlockTime = millis();
        binLocked = false;
      }
    }
    http.end();
  }
  digitalWrite(LED_YELLOW, LOW);
}

// ================= LED UPDATE =================
void updateLEDs(int p) {
  digitalWrite(LED_GREEN, p <= 50);
  digitalWrite(LED_YELLOW, p > 50 && p < 95); // yellow stops at 94%
  digitalWrite(LED_RED, p >= 95);             // red shows 95%+
}

// ================= WIFI TRIANGULATION =================
void getWiFiTriangulation() {
  int n = WiFi.scanNetworks();
  if (n == 0) return;

  StaticJsonDocument<1024> doc;
  doc["considerIp"] = true;
  JsonArray arr = doc.createNestedArray("wifiAccessPoints");

  for (int i = 0; i < min(n, 5); i++) {
    JsonObject w = arr.createNestedObject();
    w["macAddress"] = WiFi.BSSIDstr(i);
    w["signalStrength"] = WiFi.RSSI(i);
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + String(googleApiKey);

  if (http.begin(client, url)) {
    String body;
    serializeJson(doc, body);
    if (http.POST(body) == 200) {
      StaticJsonDocument<256> r;
      deserializeJson(r, http.getString());
      binLat = r["location"]["lat"];
      binLon = r["location"]["lng"];
    }
    http.end();
  }
}

// ================= TIME SYNC =================
void syncTimeBypassNTP() {
  HTTPClient http;
  if (http.begin("http://www.google.com")) {
    const char* keys[] = {"Date"};
    http.collectHeaders(keys, 1);
    if (http.GET() > 0) {
      struct tm tm;
      if (strptime(http.header("Date").c_str(), "%a, %d %b %Y %H:%M:%S", &tm)) {
        time_t t = mktime(&tm);
        struct timeval tv = {.tv_sec = t};
        settimeofday(&tv, NULL);
      }
    }
    http.end();
  }
}

// ================= ULTRASONIC SENSOR =================
long readDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long d = pulseIn(echo, HIGH, 25000);
  return d == 0 ? 100 : d * 0.034 / 2;
}
