import functions_framework
import json
from datetime import datetime
from google.cloud import pubsub_v1, firestore
import requests
import os

# Initialize clients
db = firestore.Client()
publisher = pubsub_v1.PublisherClient()

PROJECT_ID = "cpc357-481402" 
TOPIC_ID = "bin-data-topic"
topic_path = publisher.topic_path(PROJECT_ID, TOPIC_ID)

TELEGRAM_TOKEN = os.environ.get("TELEGRAM_BOT_TOKEN")
TELEGRAM_CHAT_ID = os.environ.get("TELEGRAM_CHAT_ID")

def send_telegram_alert(device_id, fill_level, lat, lon):
    if not TELEGRAM_TOKEN or not TELEGRAM_CHAT_ID:
        print("Telegram not configured")
        return

    message = (
        "🚨 *BIN FULL ALERT* 🚨\n\n"
        f"🗑 Bin ID: `{device_id}`\n"
        f"📊 Fill Level: {fill_level}%\n"
        f"📍 Location: https://maps.google.com/?q={lat},{lon}\n\n"
        "Please schedule collection."
    )

    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
    payload = {
        "chat_id": TELEGRAM_CHAT_ID,
        "text": message,
        "parse_mode": "Markdown"
    }

    try:
        requests.post(url, json=payload)
    except Exception as e:
        print(f"Telegram Error: {e}")

@functions_framework.http
def handle_pubsub(request):
    # --- HANDLE OPTIONS (CORS) ---
    if request.method == 'OPTIONS':
        headers = {
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'POST',
            'Access-Control-Allow-Headers': 'Content-Type',
            'Access-Control-Max-Age': '3600'
        }
        return ('', 204, headers)

    # --- HANDLE POST ---
    if request.method == 'POST':
        try:
            request_json = request.get_json(silent=True)
            if not request_json:
                return "No JSON received", 400

            device_id = request_json.get('device_id', 'ESP32_BIN_01')
            
            # 1. Command Check
            if request_json.get('command_check') == True:
                doc_ref = db.collection('commands').document(device_id)
                doc = doc_ref.get()
                if doc.exists:
                    action = doc.to_dict().get('action', 'NONE')
                    doc_ref.delete()
                    return action, 200
                return "NONE", 200
            
            # 2. Data Upload
            db_data = request_json.copy()
            db_data['timestamp'] = firestore.SERVER_TIMESTAMP
            
            # A. Save to History (For Charts)
            db.collection('bin_history').add(db_data)

            # B. Update Latest Status (For Dashboard) - CRITICAL UPDATE
            fill_level = db_data.get("fill_percentage") or 0
            lat = db_data.get("lat", 0.0)
            lon = db_data.get("lon", 0.0)
            
            status_ref = db.collection("bin_status").document(device_id)
            status_doc = status_ref.get()
            
            previous_full = False
            if status_doc.exists:
                previous_full = status_doc.to_dict().get("is_full", False)

            current_full = fill_level >= 95

            # Notification Logic
            if current_full and not previous_full:
                send_telegram_alert(device_id, fill_level, lat, lon)

            # Save ALL sensor data to the status document
            status_ref.set({
                "device_id": device_id,
                "is_full": current_full,
                "fill_percentage": fill_level,
                "temp_c": db_data.get("temp_c"),
                "humidity_pct": db_data.get("humidity_pct"),
                "lat": lat,
                "lon": lon,
                "last_updated": firestore.SERVER_TIMESTAMP
            }, merge=True)

            return "Success: Data logged", 200

        except Exception as e:
            return f"Error: {str(e)}", 500
    
    return "Method not allowed", 405