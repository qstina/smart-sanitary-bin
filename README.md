# 🌸 Smart Sanitary Bin System (CPC357 Project)

A cloud-native IoT ecosystem designed to support **Smart City (SDG 11)** initiatives through real-time waste monitoring, environmental risk assessment, and serverless data processing.

---

## 📌 Project Overview

The **Smart Sanitary Bin System** is an end-to-end solution for urban waste management. It utilizes localized sensors to monitor bin occupancy and environmental health, processing this data through a serverless cloud backend to provide actionable insights for city maintenance teams.

### Key Capabilities
- **Real-time Monitoring:** Track bin fill levels and environmental data (Temperature/Humidity).
- **Automated Alerts:** Instant Telegram notifications triggered server-side when bins reach capacity.
- **Remote Maintenance:** Cloud-based command polling to reset bins or unlock lids remotely.
- **Fleet Analytics:** Historical trend visualization for long-term city planning.

---

## 📂 Repository Structure & Live Resources

- **GitHub Repository:** [https://github.com/qstina/smart-sanitary-bin](https://github.com/qstina/smart-sanitary-bin)
- **Live Dashboard:** [https://cpc357-481402.web.app](https://cpc357-481402.web.app)
- **Backend API (Cloud Run):** [https://process-bin-data-238356769828.us-central1.run.app](https://process-bin-data-238356769828.us-central1.run.app)

```
.
├── main.py                # Backend API (Cloud Run)
├── requirements.txt       # Backend dependencies
├── public/                # Frontend dashboard (HTML, CSS, JS)
│   ├── index.html
│   ├── app.js
│   └── styles.css
├── smart_sanitary_bin.ino # ESP32 firmware
├── bulk_historical.py     # 30-day data simulation script
├── firebase.json          # Firebase Hosting config
├── .firebaserc            # Firebase project config
└── README.md
```

---

## 🛠️ Technical Stack & Hardware

| Component | Technology / Pin | Justification |
|-----------|------------------|---------------|
| Microcontroller | ESP32 (NodeMCU-32S) | Built-in WiFi and low power consumption for urban IoT. |
| Bin Lid Servo | SG90 Servo — Pin 16 | Automates lid opening for hygiene and remote control. |
| Level Sensor | HC-SR04 — Trig: 17, Echo: 4 | Ultrasonic sensing prevents fouling in waste environments. |
| Proximity Sensor | HC-SR04 — Trig: 5, Echo: 18 | Detects user approach to automate lid opening. |
| Health Sensor | DHT11 — Pin 27 | Monitors humidity/temp to assess bacterial growth risk. |
| Status LEDs | Pins 32, 33, 25 | Local visual feedback for bin fill state (Low, Med, High). |
| Cloud Backend | Google Cloud Run | Serverless scaling with pay-per-use efficiency. |
| Database | Google Firestore | Real-time NoSQL storage for live status and history. |
| Web Dashboard | Firebase Hosting | Secure SSL delivery and fast global content delivery. |

---

## ☁️ Cloud Architecture & System Design

This project utilizes a multi-service serverless architecture on Google Cloud Platform (GCP), ensuring high availability and scalability.

### Data Flow Logic
- **Edge Layer:** ESP32 nodes collect sensor data and perform WiFi triangulation for location tracking.
- **Ingestion Layer:** Data is sent via encrypted HTTP POST requests to the Google Cloud Run API.
- **Processing Layer:** Cloud Run validates data, triggers Telegram alerts via the Bot API, and updates Firestore documents.
- **Visualization Layer:** The Firebase-hosted dashboard uses real-time Firestore listeners to update UI charts and maps instantly without page refreshes.

---

## 🌍 SDG 11 Impact Analysis

This project directly contributes to SDG 11: Sustainable Cities and Communities:

- **Waste Management Efficiency:** Real-time data enables optimized collection routes, reducing fuel consumption and emissions.
- **Urban Hygiene:** Automated alerts prevent bin overflows in public spaces, reducing sanitary risks.
- **Data-Driven Planning:** Historical analytics help city planners identify high-demand zones for infrastructure improvement.

---

## 🚀 Setup & Installation Guide

### 1. Prerequisites & Dependencies

**Hardware:**
- ESP32, HC-SR04 Sensors (x2), DHT11, SG90 Servo motor.

**Software Libraries:**
- Arduino IDE: ArduinoJson, ESP32Servo, DHT Sensor Library.
- Python (Backend): functions-framework, google-cloud-firestore, google-cloud-pubsub, requests.

### 2. Google Cloud Platform (GCP) Setup

**Enable APIs:**
- Enable Cloud Run, Cloud Build, Firestore, and Pub/Sub APIs.

**Service Account:**
- Create a Service Account with Cloud Datastore Owner permissions.
- Download the JSON key as `service-account-key.json` for local simulation.

**Cloud Run Deployment:**
- Link your GitHub repo to Cloud Run via Cloud Build for CI/CD.
- Set Environment Variables: `TELEGRAM_BOT_TOKEN` and `TELEGRAM_CHAT_ID`.
- Deploy via CLI:

```bash
gcloud run deploy process-bin-data --source . --region us-central1 --allow-unauthenticated
```

### 3. Database & Hosting

**Firestore:**
- Create a database in Native Mode.

**Firebase Hosting:**

```bash
npm install -g firebase-tools
firebase login
firebase init hosting
firebase deploy
```

### 4. ESP32 Firmware Installation

1. Open `smart_sanitary_bin.ino` in Arduino IDE.
2. Update configuration:

```cpp
const char* cloudBridgeUrl = "YOUR_CLOUD_RUN_URL"; 
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

3. Flash the code to the ESP32.

### 5. Analytics Simulation

To populate the dashboard with 30 days of historical data for demonstration:

```bash
python bulk_historical.py
```

---

## 📊 Usage & Dashboard Features

Once deployed, access the live dashboard to:
- View real-time bin fill levels and environmental conditions
- Receive automatic Telegram alerts when bins need servicing
- Analyze historical trends and usage patterns
- Remotely control bin operations (lid unlock, system reset)
