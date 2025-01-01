#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"
#include "config.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h> // Include time library for NTP

// Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Pins and Constants
#define LIGHT_SENSOR_PIN 39
#define LIGHT_SENSOR_MIN_VALUE 0
#define LIGHT_SENSOR_MAX_VALUE 1700

unsigned long sendDataPrevMillis = 0;
int light_sensor_reading;

// Function to calculate percentage
int calculatePercentage(int rawValue, int min_value, int max_value) {
  if (rawValue < min_value) rawValue = min_value;
  if (rawValue > max_value) rawValue = max_value;
  return 100 - ((rawValue - min_value) * 100 / (max_value - min_value));
}

// Function to configure NTP and get current time
void configureTime() {
  configTime(7200, 0, "pool.ntp.org", "time.nist.gov"); // UTC+2 for Israel
  setenv("TZ", "IST-2IDT,M3.4.4/2,M10.5.0/2", 1); // Israel timezone (IDT/DST rules)
  struct tm timeInfo;
  const int timeoutMs = 10000; // 10 seconds timeout
  unsigned long startAttemptTime = millis();

  while (!getLocalTime(&timeInfo) && (millis() - startAttemptTime) < timeoutMs) {
    Serial.println("Waiting for NTP sync...");
    delay(500);
  }

  if (!getLocalTime(&timeInfo)) {
    Serial.println("Failed to obtain time");
  } else {
    Serial.println("Time synchronized successfully");
  }
}

// Function to get the current date and time as a string
String getCurrentDateTime() {
    struct tm timeInfo;
  const int timeoutMs = 10000; // 10 seconds timeout
  unsigned long startAttemptTime = millis();

  while (!getLocalTime(&timeInfo) && (millis() - startAttemptTime) < timeoutMs) {
    Serial.println("Waiting for NTP sync...");
    delay(500);
  }

  if (!getLocalTime(&timeInfo)) {
    Serial.println("Failed to obtain time");
  } else {
    Serial.println("Time synchronized successfully");
  }
  
  if (getLocalTime(&timeInfo)) {
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return String(buffer);
  }
  return "1970-01-01 00:00:00"; // Fallback in case of error
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.println("Connected to Wi-Fi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Configure Firebase
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  // Configure NTP for time synchronization
  configureTime();
}

void loop() {
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Read Light Sensor Value
    light_sensor_reading = analogRead(LIGHT_SENSOR_PIN);

    // Create JSON object for the current reading
    FirebaseJson json;
    json.add(getCurrentDateTime(), calculatePercentage(light_sensor_reading, LIGHT_SENSOR_MIN_VALUE, LIGHT_SENSOR_MAX_VALUE)); // Add current date and time

    // Save data to Firebase under a "history" path
    String path = garden_global_info_path + "/garden_light_strength";
    if (Firebase.RTDB.updateNode(&fbdo, path, &json)) {
      Serial.println("Data pushed successfully:");
      Serial.println(fbdo.pushName()); // The unique key for this entry
    } else {
      Serial.printf("Failed to push data: %s\n", fbdo.errorReason().c_str());
    }
  }
}
