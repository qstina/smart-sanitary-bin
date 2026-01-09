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
const int BIN_HEIGHT = 26;
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
unsigned long lastHeartbeat = 0;
bool binLocked = false;
bool unlockRequested = false;
bool unlocking = false;
unsigned long unlockStart = 0;
const unsigned long UNLOCK_DURATION = 5000;
const unsigned long HEARTBEAT_INTERVAL = 45000; // Keep dashboard active

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LID_TRIG, OUTPUT); pinMode(LID_ECHO, INPUT);
  pinMode(LEVEL_TRIG, OUTPUT); pinMode(LEVEL_ECHO, INPUT);
  pinMode(LED_GREEN, OUTPUT); pinMode(LED_YELLOW, OUTPUT); pinMode(LED_RED, OUTPUT);

  // WiFi connection with detailed prints
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✓ CONNECTED!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  syncTimeBypassNTP();
  dht.begin();
  binServo.attach(SERVO_PIN, 500, 2400);
  binServo.write(0);

  Serial.println("Getting location...");
  getWiFiTriangulation();
  Serial.printf("Location Fixed: %.6f, %.6f\n", binLat, binLon);

  Serial.println("Establishing handshake...");
  readAndUploadSensors("System boot / handshake");
  Serial.println("\n=== SYSTEM READY ===\n");
}

// ================= LOOP =================
void loop() {
  long lidDist = readDistance(LID_TRIG, LID_ECHO);

  // Check for remote unlock commands from the dashboard
  if (binLocked && (millis() % 5000 < 200)) {
    Serial.println("☁ Bin FULL -> checking cloud commands...");
    checkWebCommands();
  }

  // Periodic heartbeat to prevent dashboard "Offline" status
  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    Serial.println("💓 Sending heartbeat to stay active...");
    readAndUploadSensors("Heartbeat");
    lastHeartbeat = millis();
  }

  // Handle Remote Unlock State
  if (unlockRequested && !unlocking) {
    Serial.println("🔓 REMOTE UNLOCK RECEIVED: Opening lid");
    binServo.write(120);
    unlockStart = millis();
    unlocking = true;
    unlockRequested = false;
  }

  if (unlocking && millis() - unlockStart >= UNLOCK_DURATION) {
    binServo.write(0);
    delay(2000);
    Serial.println("🔒 Remote unlock duration finished.");
    unlocking = false;
    readAndUploadSensors("Remote unlock completed");
  }

  // Proximity Lid Control
  if (!unlocking && lidDist > 1 && lidDist < 15) {
    if (binLocked) {
      Serial.println("❌ Bin locked (FULL). Denying access.");
      for(int i=0; i<3; i++) {
        digitalWrite(LED_RED, HIGH); delay(100); digitalWrite(LED_RED, LOW); delay(100);
      }
    } else {
      Serial.println("🚶 Object detected: Opening lid");
      binServo.write(120);
      delay(3000);
      binServo.write(0);
      delay(2000);
      Serial.println("🔒 Lid cycle complete.");
      readAndUploadSensors("Proximity lid open");
    }
  }
  delay(200);
}

// ================= CLOUD FUNCTIONS =================
void readAndUploadSensors(const char* reason) {
  Serial.println("\n📡 SENSOR UPDATE START");
  Serial.printf("Reason: %s\n", reason);

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  long dist = readDistance(LEVEL_TRIG, LEVEL_ECHO);
  int fill = constrain(map(dist, BIN_HEIGHT, FULL_THRESHOLD, 0, 100), 0, 100);

  Serial.printf("🗑 Trash level: %ld cm (%d%%)\n", dist, fill);
  Serial.printf("🌡 Temperature: %.1f °C | 💧 Humidity: %.1f %%\n", t, h);

  StaticJsonDocument<512> doc;
  doc["device_id"] = "ESP32_BIN_01";
  doc["trash_level_cm"] = dist;
  doc["temp_c"] = t;
  doc["humidity_pct"] = h;
  doc["fill_percentage"] = fill;
  doc["lat"] = binLat;
  doc["lon"] = binLon;

  String body;
  serializeJson(doc, body);
  Serial.println("Sending JSON to Cloud:");
  Serial.println(body);

  WiFiClientSecure client;
  client.setInsecure(); // Required for Cloud Run HTTPS
  HTTPClient http;
  
  if (http.begin(client, cloudBridgeUrl)) {
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    Serial.printf("☁ Cloud response: HTTP %d\n", code);
    
    if(code == 200) {
      Serial.println("✓ Data uploaded successfully");
    } else {
      Serial.printf("✗ Upload failed. Server Response: %s\n", http.getString().c_str());
    }
    http.end();
  } else {
    Serial.println("✗ Connection to cloud bridge failed.");
  }

  binLocked = (fill >= 95);
  Serial.printf("🔒 Bin locked status: %s\n", binLocked ? "YES" : "NO");
  updateLEDs(fill);
  Serial.println("📡 SENSOR UPDATE END\n");
}

void checkWebCommands() {
  StaticJsonDocument<128> doc;
  doc["device_id"] = "ESP32_BIN_01";
  doc["command_check"] = true;
  String body;
  serializeJson(doc, body);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  if (http.begin(client, cloudBridgeUrl)) {
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);

    if (code == 200) {
      String res = http.getString();
      res.trim();
      Serial.printf("Command Received: [%s]\n", res.c_str());
      if (res.indexOf("RESET") >= 0) {
        unlockRequested = true;
        binLocked = false;
      }
    }
    http.end();
  }
}

// ================= UTILITIES =================
void updateLEDs(int p) {
  digitalWrite(LED_GREEN, p <= 50);
  digitalWrite(LED_YELLOW, p > 50 && p < 95);
  digitalWrite(LED_RED, p >= 95);
}

long readDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long dur = pulseIn(echo, HIGH, 25000);
  return dur == 0 ? 100 : dur * 0.034 / 2;
}

void syncTimeBypassNTP() {
  Serial.println("Syncing time from Google...");
  HTTPClient http;
  if (http.begin("http://www.google.com")) {
    const char* keys[] = {"Date"};
    http.collectHeaders(keys, 1);
    if (http.GET() > 0) {
      String date = http.header("Date");
      Serial.println("Date Header: " + date);
      struct tm tm;
      if (strptime(date.c_str(), "%a, %d %b %Y %H:%M:%S", &tm)) {
        time_t t = mktime(&tm);
        struct timeval tv = {.tv_sec = t};
        settimeofday(&tv, NULL);
        Serial.println("✓ Time synced.");
      }
    }
    http.end();
  }
}

void getWiFiTriangulation() {
  Serial.println("Scanning WiFi for Geolocation...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found. Skipping.");
    return;
  }
  
  // Use a smaller document size to save RAM
  StaticJsonDocument<768> doc;
  doc["considerIp"] = "true";
  JsonArray arr = doc.createNestedArray("wifiAccessPoints");
  
  // Limit to 3 strongest networks to save memory
  for (int i = 0; i < min(n, 3); i++) {
    JsonObject w = arr.createNestedObject();
    w["macAddress"] = WiFi.BSSIDstr(i);
    w["signalStrength"] = WiFi.RSSI(i);
  }

  WiFiClientSecure client;
  client.setInsecure(); // Required to bypass certificate validation

  HTTPClient http;
  String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + String(googleApiKey);

  http.setTimeout(10000); 

  Serial.println("Connecting to Google Geolocation API...");
  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");

    String body;
    serializeJson(doc, body);
    int code = http.POST(body);
    
    Serial.printf("Geolocation API HTTP Code: %d\n", code);

    if (code == 200) {
      String response = http.getString();
      StaticJsonDocument<256> r;
      deserializeJson(r, response);
      binLat = r["location"]["lat"];
      binLon = r["location"]["lng"];
      Serial.printf("✓ Location Fixed: %.6f, %.6f\n", binLat, binLon);
    } else {
      Serial.print("✗ Geolocation Failed. Error: ");
      Serial.println(http.errorToString(code).c_str());
      if (code > 0) Serial.println("Response: " + http.getString());
    }
    http.end();
  } else {
    Serial.println("✗ Unable to create connection to Google.");
  }
}