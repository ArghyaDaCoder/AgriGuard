#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// --- Wi-Fi & Server Settings ---
const char* ssid = "ssid";
const char* password = "pass";
String serverPath = "http://<IP ADD>/api/data";
String commandPath = "http://<IP_ADD/api/irrigation";

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
    
    // --- 1. READ SENSORS ---
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int rawSoil = analogRead(SOIL_PIN);
    
    // Convert raw soil value to a 0-100 percentage
    // map(value, fromLow, fromHigh, toLow, toHigh)
    int soilPercent = map(rawSoil, DRY_VALUE, WET_VALUE, 0, 100);
    
    // Constrain ensures it doesn't go below 0% or above 100% if readings fluctuate
    soilPercent = constrain(soilPercent, 0, 100);

    // --- 2. SEND DATA TO FLASK ---
    HTTPClient http;
    http.begin(serverPath);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    // Fallback to 0 if DHT fails, otherwise use real data
    doc["temperature"] = isnan(temp) ? 0.0 : temp;
    doc["humidity"] = isnan(hum) ? 0.0 : hum;
    doc["soil"] = soilPercent; // Sending the calculated percentage!

    String requestBody;
    serializeJson(doc, requestBody);
    int httpResponseCode = http.POST(requestBody);
    
    if (httpResponseCode > 0) {
      Serial.print("Data sent! Soil: "); Serial.print(soilPercent); Serial.println("%");
    } else {
      Serial.print("Error sending data: "); Serial.println(httpResponseCode);
    }
    http.end();

    // --- 3. FETCH IRRIGATION COMMAND ---
    http.begin(commandPath);
    int getCode = http.GET();
    
    if (getCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<100> commandDoc;
      deserializeJson(commandDoc, payload);
      bool pumpState = commandDoc["state"];

      if (pumpState) {
        digitalWrite(RELAY_PIN, LOW); // Turn Relay ON
        Serial.println("🌊 Pump commanded ON");
      } else {
        digitalWrite(RELAY_PIN, HIGH); // Turn Relay OFF
        Serial.println("🚫 Pump commanded OFF");
      }
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected!");
  }
  
  delay(5000); // Run cycle every 5 seconds
}
