#define LIGHT_SENSOR_PIN 35
#define MOISTURE_SENSOR_PIN 34

#include <WiFi.h>
#include "secrets.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long readDataPrevMillis = 0;

void setup()
{
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  // Assign the API key and database URL
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Set user credentials for authentication
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Set callback function for token status
  config.token_status_callback = tokenStatusCallback;

  // Initialize Firebase
  Firebase.begin(&config, &auth);

  // Automatically reconnect Wi-Fi if disconnected
  Firebase.reconnectWiFi(true);
}

void loop()
{
  if (Firebase.ready() && (millis() - readDataPrevMillis > 15000 || readDataPrevMillis == 0))
  {
    readDataPrevMillis = millis();

    // Path to the desired location in the database
    String path = "/gardens/garden1/sensors";

    Serial.println("Reading data from Firebase...");

    // Read the data from Firebase
    if (Firebase.RTDB.getJSON(&fbdo, path))
    {
      // Successfully retrieved the data
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData jsonData;
      if (json.get(jsonData, "manual_mode"))
      {
        Serial.print("manual_mode: ");
        Serial.println(jsonData.intValue);
      }

      if (json.get(jsonData, "light_sensor_value"))
      {
        Serial.print("light_sensor_value: ");
        Serial.println(jsonData.intValue);
      }

      // print all json data
      // String jsonStr;
      // json.toString(jsonStr, true);
      // Serial.printf("Data: %s\n", jsonStr.c_str());
    
    }
    else
    {
      // Failed to retrieve data, print error
      Serial.printf("Error: %s\n", fbdo.errorReason().c_str());
    }
  }
}
