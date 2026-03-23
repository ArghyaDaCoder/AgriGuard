import time
from flask import Flask, request, jsonify
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

# In-memory storage
latest_data = {
    "soil": 0,
    "temperature": 0,
    "humidity": 0,
    "connected": False,
    "pump_state": False
}
irrigation_command = {"state": False}
last_seen = 0  # Timestamp to track the ESP32's heartbeat

@app.route('/api/data', methods=['POST', 'GET'])
def handle_data():
    global latest_data, irrigation_command, last_seen
    
    if request.method == 'POST':
        # 1. Reset the stopwatch because ESP32 just checked in
        last_seen = time.time()
        incoming_data = request.json
        
        # 2. Update our memory with fresh data
        latest_data["soil"] = incoming_data.get("soil", 0)
        latest_data["temperature"] = incoming_data.get("temperature", 0)
        latest_data["humidity"] = incoming_data.get("humidity", 0)
        
        # 3. 🔥 THE AUTO SHUT-OFF LOGIC 🔥
        if latest_data["soil"] >= 100 and irrigation_command["state"] == True:
            print("🌊 SAFEGUARD TRIGGERED: Soil reached 100%. Turning pump OFF.")
            irrigation_command["state"] = False
            
        return jsonify({"status": "success"}), 200

    # --- FOR GET REQUESTS (The Dashboard) ---
    
    # Check if the ESP32 has gone silent for more than 15 seconds
    is_connected = (time.time() - last_seen) < 15
    
    # Bundle the connection status and current pump state into the payload
    latest_data["connected"] = is_connected
    latest_data["pump_state"] = irrigation_command["state"]
    
    return jsonify(latest_data)

@app.route('/api/irrigation', methods=['POST', 'GET'])
def handle_irrigation():
    global irrigation_command
    if request.method == 'POST':
        irrigation_command = request.json
        print(f"Irrigation command updated: {irrigation_command}")
        return jsonify({"status": "command_updated"}), 200
    return jsonify(irrigation_command)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)