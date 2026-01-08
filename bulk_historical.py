from datetime import datetime, timedelta, timezone
import random
from google.cloud import firestore

# --- Firestore setup ---
key_path = r"C:\Users\Nitro\Documents\SEM 7\CPC357\project\cpc357-481402-478f51f5b2d5.json"
db = firestore.Client.from_service_account_json(key_path)
print("Firestore client initialized successfully!")

# --- Configuration ---
BIN_IDS = ["ESP32_BIN_01", "ESP32_BIN_02", "ESP32_BIN_03"]
NUM_DAYS = 30
ENTRIES_PER_DAY = 4
BIN_HEIGHT_CM = 25

BIN_LOCATIONS = {
    "ESP32_BIN_01": (5.314, 100.312),
    "ESP32_BIN_02": (5.316, 100.315),
    "ESP32_BIN_03": (5.318, 100.310),
}

MIN_FILL_STEP = 1
MAX_FILL_STEP = 8

def random_temp():
    return round(random.uniform(24.0, 32.0), 1)

def random_humidity():
    return round(random.uniform(40.0, 70.0), 1)

# --- Generate historical data ---
for bin_id in BIN_IDS:
    lat, lon = BIN_LOCATIONS[bin_id]
    fill_percentage = random.randint(5, 20)  # start low

    for day_offset in range(NUM_DAYS, 0, -1):
        for entry in range(ENTRIES_PER_DAY):
            # timestamp for historical record
            timestamp = datetime.now(timezone.utc) - timedelta(days=day_offset, hours=entry * (24 // ENTRIES_PER_DAY))

            # smooth fill progression
            fill_step = random.randint(MIN_FILL_STEP, MAX_FILL_STEP)
            fill_percentage = min(fill_percentage + fill_step, 100)

            trash_level_cm = round(BIN_HEIGHT_CM * (1 - fill_percentage / 100), 1)

            doc_data = {
                "device_id": bin_id,
                "fill_percentage": fill_percentage,
                "trash_level_cm": trash_level_cm,
                "temp_c": random_temp(),
                "humidity_pct": random_humidity(),
                "lat": lat,
                "lon": lon,
                "timestamp": timestamp
            }

            db.collection("bin_history").add(doc_data)

    # --- Update current live bin status ---
    # Use the last fill_percentage but set last_updated to now
    is_full = fill_percentage >= 95
    db.collection("bin_status").document(bin_id).set({
        "device_id": bin_id,
        "fill_percentage": fill_percentage,
        "is_full": is_full,
        "temp_c": doc_data["temp_c"],
        "humidity_pct": doc_data["humidity_pct"],
        "lat": lat,
        "lon": lon,
        "last_updated": datetime.now(timezone.utc)  # now = live
    })

print("Historical data + live bin status uploaded successfully!")
