# 🌸 Smart Sanitary Bin System (CPC357 Project)

A cloud-native IoT ecosystem designed to support **Smart City (SDG 11)** initiatives through real-time waste monitoring, environmental risk assessment, and serverless data processing.

---

## 📌 Project Overview

The **Smart Sanitary Bin System** integrates:

- **IoT hardware** (ESP32, sensors)  
- **Serverless cloud backend** (Cloud Run)  
- **Realtime dashboard** (Firebase Hosting)  
- **Telegram notification system** for immediate alerts  

Key capabilities for city administrators:

- Monitor bin fill levels in real time  
- Detect hygiene risks (temperature & humidity)  
- Identify offline or malfunctioning bins  
- Analyse historical trends for fleet-wide planning  

---

## 📂 Repository Structure & Live Resources

- **GitHub Repository:** https://github.com/qstina/smart-sanitary-bin  
- **Live Dashboard:** https://cpc357-481402.web.app  
- **Backend API (Cloud Run):** https://process-bin-data-238356769828.us-central1.run.app  

```text
.
├── main.py                  # Backend API (Cloud Run)
├── requirements.txt         # Backend dependencies
├── public/                  # Frontend dashboard (HTML, CSS, JS)
│   ├── index.html
│   ├── app.js
│   └── styles.css
├── smart_sanitary_bin.ino   # ESP32 firmware
├── bulk_historical.py       # 30-day data simulation script
├── firebase.json            # Firebase Hosting config
├── .firebaserc              # Firebase project config
└── README.md
```
---

## 🛠️ Technical Stack & Hardware

| Component | Technology / Pin | Justification |
|-----------|-----------------|---------------|
| **Microcontroller** | ESP32 (NodeMCU-32S) | Built-in WiFi, low power consumption, suitable for urban IoT deployments |
| **Bin Lid Servo** | SG90 Servo — Pin 16 | Automates lid opening/closing for hygiene and remote control |
| **Level Sensor** | HC-SR04 Ultrasonic — Trig: 17, Echo: 4 | Measures bin fill height without contact, preventing sensor fouling |
| **Lid Proximity Sensor** | HC-SR04 Ultrasonic — Trig: 5, Echo: 18 | Detects user approach to open lid automatically |
| **Health Sensor** | DHT11 — Pin 27 | Monitors humidity and temperature to assess hygiene risk |
| **Status LEDs** | Green: 32, Yellow: 33, Red: 25 | Visual indicators for bin fill state (Low, Medium, High) |
| **Cloud Backend** | Google Cloud Run | Serverless scaling with pay-per-use, handles data ingestion, commands, and alert logic |
| **Database** | Google Firestore | Real-time NoSQL database, supports live dashboard updates and historical analytics |
| **Web Dashboard** | Firebase Hosting | Secure SSL, fast global delivery, decoupled from backend for reliability |

---

## ☁️ Cloud Architecture & System Design

This project uses a **multi-service serverless architecture** on **Google Cloud Platform (GCP)**, following **microservices principles** for scalability, reliability, and cost efficiency.

---

### 🔹 Backend API — Google Cloud Run

- Central **serverless backend** for the IoT system  
- Receives secure HTTP POST requests from ESP32 IoT nodes  
- Handles:
  - Bin sensor data ingestion  
  - Real-time state updates (`bin_status`)  
  - Command polling for remote bin reset/unlock  
  - Alert trigger logic for Telegram notifications  
- Auto-scales with traffic demand  
- Integrated with **GitHub CI/CD**: changes to `main.py` trigger automatic redeployment  

---

### 🔹 Frontend Web Dashboard — Firebase Hosting

- Hosts the **real-time monitoring dashboard**  
- Public URL: 👉 https://cpc357-481402.web.app  
- Responsibilities:
  - Live bin status visualization  
  - Historical usage charts and analytics  
  - Map-based bin location display  
- Uses Firestore real-time listeners for **instant UI updates without page refresh**  

---

### 🔹 Database — Google Firestore

- NoSQL document database storing:
  - **Real-time bin state** (`bin_status`)  
  - **Historical records** (`bin_history`)  
- Enables:
  - Fleet-wide monitoring  
  - Offline detection using `last_updated`  
  - Weekly and monthly analytics  

---

## 🔔 Alert & Notification System — Telegram Bot Integration

To ensure **immediate response** without continuous dashboard monitoring, the system uses **Telegram Bot notifications**.  
Alerts are generated **server-side in Cloud Run**, ensuring reliability even if the dashboard is offline.

---

### 🔹 Alert Workflow

```text
ESP32 Sensor Node
   │
   │  HTTP POST (sensor data)
   ▼
Google Cloud Run API
   │
   │  Validate & process data
   │
   │  Compare current vs previous bin state
   ▼
Google Firestore
   │
   │  Read last known status (bin_status)
   ▼
Telegram Bot API
   │
   │  HTTPS message delivery
   ▼
Maintenance Staff / Authorities
