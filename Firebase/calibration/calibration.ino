#include <WiFi.h>
#include "secrets.h"
#include "config.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


unsigned long readDataPrevMillis = 0;
int moisture_sensor_reading = 0;

// ============ for POC only, needs to be relocated ============
// Function to calculate percentage
int calculatePercentage(int rawValue, int min_value, int max_value) {
  if (rawValue < min_value) rawValue = min_value;
  if (rawValue > max_value) rawValue = max_value;
  return 100 - ((rawValue - min_value) * 100 / (max_value - min_value));
}

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
    String plant1_calibration_path = plant1_path + "/calibration";

    Serial.println("Reading data from Firebase...");

    // Read the data from Firebase
    if (Firebase.RTDB.getJSON(&fbdo, plant1_calibration_path)) {
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
              Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, plant1_calibration_path, &json) ? "ok" : fbdo.errorReason().c_str());
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
              Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, plant1_calibration_path, &json) ? "ok" : fbdo.errorReason().c_str());
            }
          }        
        }
      }

      // ============ print moisture normalized measurments ============
      // ============ for POC only, needs to be relocated ============
      FirebaseJsonData dry_soil_measurment;
      FirebaseJsonData wet_soil_measurment;
      if (json.get(dry_soil_measurment, "dry_soil_measurment")) {
        if (json.get(wet_soil_measurment, "wet_soil_measurment")) {
          if ((dry_soil_measurment.intValue != 0) && (wet_soil_measurment.intValue != 0)) {
            moisture_sensor_reading = analogRead(MOISTURE_SENSOR_PIN_1);
            Serial.print("moisture_sensor_reading: ");
            Serial.println(calculatePercentage(moisture_sensor_reading, wet_soil_measurment.intValue, dry_soil_measurment.intValue));
          }
        }
      }


    }
    else
    {
      // Failed to retrieve data, print error
      Serial.printf("Error: %s\n", fbdo.errorReason().c_str());
    }



  }
}
