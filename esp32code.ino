#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// --- Wi-Fi & Server Settings ---
const char* ssid = "ssid";
const char* password = "pass";
String serverPath = "http://IP_ADD/api/data";
String commandPath = "http://IP_ADD/api/irrigation";

// --- Hardware Pins & Settings ---
#define DHTPIN 4
#define DHTTYPE DHT22
#define SOIL_PIN 33     // Using the one that read 206
#define RELAY_PIN 26

// Calibration values from your test!
const int DRY_VALUE = 3312;  // Air reading
const int WET_VALUE = 1500;   // Water reading

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  
  // Initialize Hardware
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF initially (Active Low)

  // Initialize Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\nConnected to WiFi!");
}
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n====================================");
    Serial.println("📡 INITIATING DATA TRANSMISSION...");
    
    // Check Wi-Fi Health
    Serial.print("📶 Wi-Fi Signal Strength (RSSI): ");
    Serial.println(WiFi.RSSI());
    Serial.print("🎯 Target Server URL: ");
    Serial.println(serverPath);
    
    // --- 1. READ SENSORS ---
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int rawSoil = analogRead(SOIL_PIN);
    
    int soilPercent = map(rawSoil, DRY_VALUE, WET_VALUE, 0, 100);
    soilPercent = constrain(soilPercent, 0, 100);

    // --- 2. SEND DATA TO FLASK ---
    HTTPClient http;
    // Set a slightly longer timeout just in case the network is slow (default is usually 5000ms)
    http.setTimeout(10000); 
    
    http.begin(serverPath);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["temperature"] = isnan(temp) ? 0.0 : temp;
    doc["humidity"] = isnan(hum) ? 0.0 : hum;
    doc["soil"] = soilPercent;

    String requestBody;
    serializeJson(doc, requestBody);
    
    Serial.print("📦 Payload generating: ");
    Serial.println(requestBody);
    
    Serial.println("⏳ Sending POST request...");
    int httpResponseCode = http.POST(requestBody);
    
    if (httpResponseCode > 0) {
      Serial.print("✅ SUCCESS! HTTP Status Code: "); 
      Serial.println(httpResponseCode);
      Serial.print("📩 Server Response: ");
      Serial.println(http.getString());
    } else {
      Serial.print("❌ FAILED! HTTP Error Code: "); 
      Serial.println(httpResponseCode);
      Serial.print("🔍 Error Details: ");
      // This is the magic line that translates the -1 error!
      Serial.println(http.errorToString(httpResponseCode).c_str());
    }
    http.end();

    // --- 3. FETCH IRRIGATION COMMAND ---
    // (Keeping this brief for now to focus on the POST error)
    http.begin(commandPath);
    int getCode = http.GET();
    if (getCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<100> commandDoc;
      deserializeJson(commandDoc, payload);
      bool pumpState = commandDoc["state"];
      if (pumpState) {
        digitalWrite(RELAY_PIN, LOW);
      } else {
        digitalWrite(RELAY_PIN, HIGH);
      }
    }
    http.end();
    
    Serial.println("====================================\n");
  } else {
    Serial.println("⚠️ WiFi Disconnected! Attempting to reconnect...");
    WiFi.reconnect();
  }
  
  delay(5000);
}
