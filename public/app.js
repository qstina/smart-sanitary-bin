// --- FIREBASE CONFIG ---
firebase.initializeApp({
    apiKey: "AIzaSyDWIYyhoz-nRExgTY0MIVuk7baonFy6ndU",
    projectId: "cpc357-481402"
});

const db = firebase.firestore();
const BIN_HEIGHT = 25;
const FULL_THRESHOLD = 5;

// TEST SETTING: 30 seconds for rapid testing
const OFFLINE_THRESHOLD_MS = 30000; 

// Global state to track all bins for synchronization
let fleetState = {
    "ESP32_BIN_01": { lastSeen: null, occupancy: 0, isFull: false, lat: null, lon: null },
    "ESP32_BIN_02": { lastSeen: null, occupancy: 0, isFull: false, lat: null, lon: null },
    "ESP32_BIN_03": { lastSeen: null, occupancy: 0, isFull: false, lat: null, lon: null }
};

let disposalChart, riskGauge, envRadarChart, fleetChart, map, marker;

window.onload = function() {
    updateClock();
    initCharts();
    startGlobalListeners();
    // Synchronization interval
    setInterval(syncFleetStatusUI, 5000); 
    loadFleetHistoricalData();
};

// --- VIEW NAVIGATION ---
function switchView(viewName) {
    document.querySelectorAll('.view-section').forEach(el => el.classList.add('hidden'));
    document.querySelectorAll('.view-section').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));

    if (viewName === 'fleet') {
        document.getElementById('view-fleet').classList.remove('hidden');
        document.getElementById('view-fleet').classList.add('active');
        document.getElementById('pageTitle').innerText = "Fleet Overview";
        document.querySelectorAll('.nav-item')[0].classList.add('active');
    } else {
        document.getElementById('view-detail').classList.remove('hidden');
        document.getElementById('view-detail').classList.add('active');
        document.getElementById('pageTitle').innerText = "Monitor: ESP32_BIN_01";
        document.querySelectorAll('.nav-item')[1].classList.add('active');
        if(disposalChart) disposalChart.resize();
        if(map) google.maps.event.trigger(map, "resize");
    }
}

// --- GLOBAL LISTENERS ---
function startGlobalListeners() {
    // 1. Real-time Status Listener
    db.collection("bin_status").onSnapshot(snapshot => {
        snapshot.forEach(doc => {
            const data = doc.data();
            const binId = doc.id;
            
            if (fleetState[binId]) {
                // Handle Firestore Timestamp
                fleetState[binId].lastSeen = data.last_updated ? data.last_updated.toDate() : new Date();
                fleetState[binId].occupancy = data.fill_percentage || 0;
                fleetState[binId].isFull = data.is_full || false;
                fleetState[binId].lat = data.lat;
                fleetState[binId].lon = data.lon;
                fleetState[binId].temp = data.temp_c;
                fleetState[binId].hum = data.humidity_pct;
            }

            if(binId === "ESP32_BIN_01") {
                updateDetailMonitor(data, fleetState[binId].occupancy);
                updateLocationUI(data.lat, data.lon, "fleet-location-01");
                
                const syncEl = document.getElementById("syncTimeSidebar");
                if(syncEl && fleetState[binId].lastSeen) {
                    syncEl.innerText = "Sync: " + fleetState[binId].lastSeen.toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'});
                }
            }
        });
        syncFleetStatusUI();
    });

    // 2. Historical Listener (Charts & ETA)
    db.collection("bin_history")
    .where("device_id", "==", "ESP32_BIN_01")
    .orderBy("timestamp", "desc")
    .limit(15)
    .onSnapshot(snapshot => {
        if (snapshot.empty) return;
        const docs = snapshot.docs.reverse();
        if(disposalChart) {
            disposalChart.data.labels = docs.map(d => {
                const ts = d.data().timestamp;
                return ts ? ts.toDate().toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'}) : "";
            });
            disposalChart.data.datasets[0].data = docs.map(d => d.data().fill_percentage);
            disposalChart.update();
        }
        
        const latest = docs[docs.length - 1].data();
        if(document.getElementById("rawLevel")) {
            document.getElementById("rawLevel").innerText = (latest.trash_level_cm || 0) + " cm";
        }
        calculateETA(docs);
    });
}

/**
 * MASTER SYNC: Updates the UI
 * Formats "Last Seen" into seconds, minutes, hours, or days.
 */
function syncFleetStatusUI() {
    let activeCount = 0;
    let alertCount = 0;
    let healthSum = 0;
    const now = new Date();
    const binIds = Object.keys(fleetState);

    binIds.forEach(id => {
        const bin = fleetState[id];
        const suffix = id.split('_').pop();
        
        // 1. Calculate Human-Readable Time Difference
        let timeAgoText = "Never";
        let isOffline = true;

        if (bin.lastSeen) {
            const diffMs = now - bin.lastSeen;
            const diffSec = Math.floor(diffMs / 1000);
            const diffMin = Math.floor(diffSec / 60);
            const diffHr = Math.floor(diffMin / 60);
            const diffDay = Math.floor(diffHr / 24);

            isOffline = diffMs > OFFLINE_THRESHOLD_MS;

            if (diffSec < 60) {
                timeAgoText = `${diffSec}s ago`;
            } else if (diffMin < 60) {
                timeAgoText = `${diffMin}m ago`;
            } else if (diffHr < 24) {
                timeAgoText = `${diffHr}h ago`;
            } else {
                timeAgoText = `${diffDay}d ago`;
            }
        }

        // 2. Update Status Badges
        const statusEl = document.getElementById(`fleet-status-${suffix}`);
        if (statusEl) {
            if (isOffline) {
                statusEl.innerText = "Offline";
                statusEl.className = "status-badge offline";
            } else if (bin.isFull) {
                statusEl.innerText = "FULL";
                statusEl.className = "status-badge full";
            } else {
                statusEl.innerText = "Active";
                statusEl.className = "status-badge active";
            }
        }

        // 3. Update Last Seen Label on Card
        const lastSeenEl = document.getElementById(`fleet-last-seen-${suffix}`);
        if (lastSeenEl) {
            lastSeenEl.innerText = "Last Seen: " + timeAgoText;
            lastSeenEl.style.color = isOffline ? "#be123c" : "#64748b"; 
        }

        // 4. Update Sidebar Sync (specifically for Bin 01)
        if (id === "ESP32_BIN_01") {
            const sidebarSync = document.getElementById("syncTimeSidebar");
            if (sidebarSync) sidebarSync.innerText = "Sync: " + timeAgoText;
        }

        // 5. Update Fill Text Persistence
        const fillEl = document.getElementById(`fleet-fill-text-${suffix}`);
        if (fillEl) {
            fillEl.innerText = (bin.occupancy === 0 && !bin.lastSeen) ? "--%" : Math.round(bin.occupancy) + "%";
            fillEl.style.opacity = isOffline ? "0.6" : "1";
        }

        if (!isOffline) activeCount++;
        if (bin.isFull) alertCount++;
        healthSum += isOffline ? 0 : 100;

        // Monitor View Status Badge
        if (id === "ESP32_BIN_01" && document.getElementById("detail-status")) {
            const detStatus = document.getElementById("detail-status");
            detStatus.innerText = isOffline ? "Offline" : (bin.isFull ? "FULL" : "Active");
            detStatus.className = `status-badge ${isOffline ? 'offline' : (bin.isFull ? 'full' : 'active')}`;
        }
    });

    // Update Top Stats Bar
    if (document.getElementById("statActiveBins")) document.getElementById("statActiveBins").innerText = activeCount;
    if (document.getElementById("statAlerts")) document.getElementById("statAlerts").innerText = alertCount;
    if (document.getElementById("statHealth")) document.getElementById("statHealth").innerText = Math.round(healthSum / binIds.length) + "%";
}

// --- MONITOR UI UPDATES ---
function updateDetailMonitor(data, occupancy) {
    const rounded = Math.round(occupancy) + "%";
    if(document.getElementById("occupancy")) document.getElementById("occupancy").innerText = rounded;
    if(document.getElementById("barFill")) document.getElementById("barFill").style.width = occupancy + "%";
    if(document.getElementById("temp")) document.getElementById("temp").innerText = data.temp_c ?? "--";
    if(document.getElementById("hum")) document.getElementById("hum").innerText = data.humidity_pct ?? "--";
    if(document.getElementById("detail-fill-text")) document.getElementById("detail-fill-text").innerText = rounded;
    
    const risk = Math.round((occupancy * 0.5) + (data.humidity_pct * 0.3) + ((data.temp_c/50)*100*0.2));
    updateRiskUI(risk);

    if(envRadarChart) {
        envRadarChart.data.datasets[0].data = [(data.temp_c/45)*100, data.humidity_pct, occupancy, risk];
        envRadarChart.update();
    }
    try { updateMap(data); } catch(e) {}
}

// --- CHART INITIALIZATION ---
function initCharts() {
    const ctx1 = document.getElementById("disposalChart");
    if(ctx1) {
        disposalChart = new Chart(ctx1, {
            type: "line",
            data: { labels: [], datasets: [{ label: "Fill Velocity", data: [], borderColor: "#055C43", backgroundColor: "rgba(5, 92, 67, 0.1)", fill: true, tension: 0.4 }]},
            options: { plugins: { legend: { display: false }}, scales: { y: { beginAtZero: true, max: 100 }}}
        });
    }
    const ctx2 = document.getElementById("riskGauge");
    if(ctx2) {
        riskGauge = new Chart(ctx2, {
            type: "doughnut",
            data: { labels: ["Risk", "Safe"], datasets: [{ data: [0, 100], backgroundColor: ["#055C43", "#e2e8f0"], borderWidth: 0 }]},
            options: { cutout: '80%', plugins: { legend: { display: false }}}
        });
    }
    const ctx3 = document.getElementById("envRadarChart");
    if(ctx3) {
        envRadarChart = new Chart(ctx3, {
            type: 'radar',
            data: { labels: ['Heat', 'Humidity', 'Fill %', 'Risk'], datasets: [{ data: [0,0,0,0], borderColor: '#055C43', backgroundColor: 'rgba(5, 92, 67, 0.2)' }]},
            options: { scales: { r: { display: false, suggestMin: 0, suggestMax: 100 } }, plugins: { legend: { display: false } }}
        });
    }
    const ctx4 = document.getElementById("fleetChart");
    if (ctx4) {
        fleetChart = new Chart(ctx4, {
            type: 'bar',
            data: { labels: [], datasets: [{ label: 'Avg Fill %', data: [], borderRadius: 8 }] },
            options: { plugins: { legend: { display: false } }, scales: { y: { beginAtZero: true, max: 100 } }}
        });
    }
}

// --- UTILITIES & TOOLS ---
function updateClock() {
    setInterval(() => {
        const now = new Date();
        const el = document.getElementById('liveClock');
        if(el) el.innerText = now.toLocaleString('en-US', { weekday: 'long', year: 'numeric', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit', second: '2-digit' });
    }, 1000);
}

async function loadFleetHistoricalData() {
    const days = parseInt(document.getElementById("fleetTimeFilter").value);
    const cutoffDate = new Date();
    cutoffDate.setDate(cutoffDate.getDate() - days);

    try {
        const snapshot = await db.collection("bin_history").where("timestamp", ">=", cutoffDate).get();
        const binData = { "ESP32_BIN_01": [], "ESP32_BIN_02": [], "ESP32_BIN_03": [] };
        snapshot.forEach(doc => {
            const d = doc.data();
            if(binData[d.device_id]) binData[d.device_id].push(d.fill_percentage);
        });
        const averages = Object.keys(binData).map(id => {
            const vals = binData[id];
            return vals.length ? (vals.reduce((a, b) => a + b, 0) / vals.length) : 0;
        });
        if(fleetChart) {
            fleetChart.data.labels = ["BIN_01", "BIN_02", "BIN_03"];
            fleetChart.data.datasets[0].data = averages;
            fleetChart.data.datasets[0].backgroundColor = ["#055C43", "#BDA9D1", "#64748b"];
            fleetChart.update();
        }
    } catch (e) { console.error("Filter error:", e); }
}

async function updateLocationUI(lat, lon, id) {
    if (!lat || !lon) return;
    const geocoder = new google.maps.Geocoder();
    geocoder.geocode({ location: { lat, lng: lon } }, (results, status) => {
        if (status === "OK" && results[0]) {
            const loc = results[0].formatted_address.split(",").slice(0, 2).join(", ");
            const el = document.getElementById(id);
            if(el) el.innerText = loc + " · Live";
            if(id === "fleet-location-01") {
                const det = document.getElementById("detail-location");
                if(det) det.innerText = loc + " · Live";
            }
        }
    });
}

function updateRiskUI(index) {
    if(!riskGauge) return;
    riskGauge.data.datasets[0].data = [index, 100 - index];
    const color = index < 40 ? '#055C43' : (index < 75 ? '#ca8a04' : '#be123c');
    riskGauge.data.datasets[0].backgroundColor = [color, "#e2e8f0"];
    riskGauge.update();
    const lbl = document.getElementById("riskLevel");
    if(lbl) { lbl.innerText = index < 40 ? "Low Risk" : (index < 75 ? "Medium Risk" : "High Risk"); lbl.style.color = color; }
}

function calculateETA(docs) {
    const el = document.getElementById("etaFull");
    if(!el || docs.length < 5) return;
    
    const first = docs[0].data();
    const last = docs[docs.length - 1].data();
    
    if (!first.timestamp || !last.timestamp) return;

    const timeDiff = (last.timestamp.seconds - first.timestamp.seconds) / 60; 
    const fillDiff = (first.trash_level_cm - last.trash_level_cm); 

    if (fillDiff > 0 && timeDiff > 0) {
        const rate = fillDiff / timeDiff;
        const remaining = last.trash_level_cm - FULL_THRESHOLD;
        const minutes = Math.abs(Math.round(remaining / rate));
        el.innerText = `Full in ~${minutes} mins`;
    } else { 
        el.innerText = "Usage Stable"; 
    }
}

async function serviceBin() {
    const btn = document.getElementById("serviceBtn");
    btn.innerText = "Sending...";
    try {
        await db.collection("commands").doc("ESP32_BIN_01").set({
            action: "RESET",
            timestamp: firebase.firestore.FieldValue.serverTimestamp()
        });
        alert("Reset command sent.");
    } catch(e) { alert("Failed to send command."); } 
    finally { btn.innerText = "🧹 Reset & Unlock Lid"; }
}

async function updateMap(latestDoc) {
    if (!latestDoc.lat || !latestDoc.lon) return;
    const binPos = { lat: latestDoc.lat, lng: latestDoc.lon };
    const mapEl = document.getElementById("map");
    if (!map) {
        try {
            const { Map } = await google.maps.importLibrary("maps");
            const { AdvancedMarkerElement } = await google.maps.importLibrary("marker");
            map = new Map(mapEl, { center: binPos, zoom: 17, mapId: "DEMO_MAP_ID", disableDefaultUI: true });
            marker = new AdvancedMarkerElement({ map: map, position: binPos });
        } catch(e) {}
    } else { if (marker) marker.position = binPos; map.panTo(binPos); }
}