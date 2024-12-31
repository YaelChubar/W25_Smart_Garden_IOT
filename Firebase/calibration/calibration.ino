#include <WiFi.h>
#include "secrets.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define MOISTURE_SENSOR_PIN_1 33 

unsigned long readDataPrevMillis = 0;
int moisture_sensor_reading = 0;

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
    String path = "/gardens/garden1/plants/plant1/calibration";

    Serial.println("Reading data from Firebase...");

    // Read the data from Firebase
    if (Firebase.RTDB.getJSON(&fbdo, path))
    {
      // Successfully retrieved the data
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData calibration_state;
      FirebaseJsonData moisture_calibration_dry;
      FirebaseJsonData moisture_calibration_wet;
      
      // calibration main loop
      if (json.get(calibration_state, "moisture_calibration_mode")) {
        Serial.print("moisture_calibration_mode: ");
        Serial.println(calibration_state.intValue);
        if (calibration_state.intValue == 1) { 
          // dry soil calibration
          Serial.print("entered dry calibration process");
          if (json.get(moisture_calibration_dry, "moisture_calibration_dry")) {
            Serial.print("moisture_calibration_dry: ");
            Serial.println(moisture_calibration_dry.intValue);
            if (moisture_calibration_dry.intValue == 1) {
              // measure dry soil
              moisture_sensor_reading = analogRead(MOISTURE_SENSOR_PIN_1);
              Serial.print("moisture sensor in dry soil: ");
              Serial.println(moisture_sensor_reading);
              json.add("dry_soil_measurment", moisture_sensor_reading);
              json.add("moisture_calibration_dry", 0);
              // upload data to RTDB
              Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, path, &json) ? "ok" : fbdo.errorReason().c_str());
            }
          }

          // wet soil calibration
          if (json.get(moisture_calibration_wet, "moisture_calibration_wet")) {
            Serial.print("moisture_calibration_wet: ");
            Serial.println(moisture_calibration_wet.intValue);
            if (moisture_calibration_wet.intValue == 1) {
              
              Serial.print("entered wet calibration process");
              // measure dry soil
              moisture_sensor_reading = analogRead(MOISTURE_SENSOR_PIN_1);
              Serial.print("moisture sensor in wet soil: ");
              Serial.println(moisture_sensor_reading);
              json.add("wet_soil_measurment", moisture_sensor_reading);
              json.add("moisture_calibration_wet", 0);
              json.add("moisture_calibration_mode", 0);
              // upload data to RTDB
              Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, path, &json) ? "ok" : fbdo.errorReason().c_str());
            }
          }        
        }
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
