#include "secrets.h"
#include "config.h"

// Global variables
WiFiManager wifiManager;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int plants_num;
int existing_plants[4] = {0};
unsigned long readDataPrevMillis = 0;
int sensor_reading = 0;
int light_sensor_reading;
int waterTankReadRaw;
Servo myServo;

Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);

// Functions
void initial_wifi_setup();
void firebase_setup();
void check_calibration(int plant_id);
//void get_existing_plants();

void setup() {
  Serial.begin(115200);

  initial_wifi_setup();

  firebase_setup();
  
}

// Initial setup:
void initial_wifi_setup() {
  // Reset Wi-Fi credentials (remove any saved Wi-Fi credentials from ESP32)
  wifiManager.resetSettings();

  // Start the Wi-Fi configuration portal in Access Point (AP) mode
  Serial.println("Starting Wi-Fi configuration portal...\n");
  if (!wifiManager.startConfigPortal("SmartGarden-Setup")) {
    Serial.println("Failed to connect and hit timeout\n");
    ESP.restart();  // Restart ESP32 if it fails to connect
  }

  // Once connected, print the assigned IP address
  Serial.println("Wi-Fi connected!\n");
  Serial.print("IP Address: \n");
  Serial.println(WiFi.localIP());
}

void firebase_setup() {
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

void loop() {
  if (Firebase.ready() && (millis() - readDataPrevMillis > 15000 || readDataPrevMillis == 0))
  {
    readDataPrevMillis = millis();

    if (Firebase.RTDB.getJSON(&fbdo, garden_path)) {
      for (int i = 1; i < 5; i++) {
        //for each plant, check:        
        // Calibration mode
        // Is exists
        // Moisture sensor
        check_calibration(i);
        // if not in calibration mode, check if ready
        // if not ready
        // continue
        // else - check moisture and water if needed
      }
      
      // Garden exists in firebase
      // Check existing plants
      //get_existing_plants();


      
      
    } else {
      //TODO: add indication that garden was not found in firebase
      // OPTIONAL: red light indicator on esp OR error indication in firebase

    }



  }

}

void check_calibration(int plant_id) {
  int moisture_pin = 0;
  switch (plant_id) {
    case 1:
      moisture_pin = MOISTURE_SENSOR_PIN_1;
      break;
    case 2:
      moisture_pin = MOISTURE_SENSOR_PIN_2;
      break;
    case 3:
      moisture_pin = MOISTURE_SENSOR_PIN_3;
      break;
    case 4:
      moisture_pin = MOISTURE_SENSOR_PIN_4;
      break;
    default:
      Serial.print("Invalid plant_id! \n");
      return;
  }
  
  String plant_calibration_path = garden_path + "/plants/plant" + String(plant_id)+ "/calibration";
  int moisture_sensor_reading = 0;

  if (Firebase.RTDB.getJSON(&fbdo, plant_calibration_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    FirebaseJsonData calibration_state;
    FirebaseJsonData moisture_calibration_dry;
    FirebaseJsonData moisture_calibration_wet;
    if (json.get(calibration_state, "moisture_calibration_mode")) {
      Serial.print("moisture_calibration_mode: ");
      Serial.println(calibration_state.intValue);
      
      unsigned long calibrationStartTime = millis(); 
      while (calibration_state.intValue == 1) {
        delay(2000);
        Serial.print("starting calibration\n");

        // dry soil calibration
        Serial.print("entered dry calibration process\n");
        if (json.get(moisture_calibration_dry, "moisture_calibration_dry")) {
          Serial.print("moisture_calibration_dry: ");
          Serial.println(moisture_calibration_dry.intValue);
          if (moisture_calibration_dry.intValue == 1) {
            // measure dry soil
            moisture_sensor_reading = analogRead(moisture_pin);
            Serial.print("moisture sensor in dry soil: ");
            Serial.println(moisture_sensor_reading);
            json.add("dry_soil_measurment", moisture_sensor_reading);
            json.add("moisture_calibration_dry", 0);
            // upload data to RTDB
            Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, plant_calibration_path, &json) ? "ok" : fbdo.errorReason().c_str());
          }
        }

        // wet soil calibration
        if (json.get(moisture_calibration_wet, "moisture_calibration_wet")) {
          Serial.print("moisture_calibration_wet: ");
          Serial.println(moisture_calibration_wet.intValue);
          if (moisture_calibration_wet.intValue == 1) {
            
            Serial.print("entered wet calibration process\n");
            // measure dry soil
            moisture_sensor_reading = analogRead(moisture_pin);
            Serial.print("moisture sensor in wet soil: ");
            Serial.println(moisture_sensor_reading);
            json.add("wet_soil_measurment", moisture_sensor_reading);
            json.add("moisture_calibration_wet", 0);
            json.add("moisture_calibration_mode", 0);
            // upload data to RTDB
            Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, plant_calibration_path, &json) ? "ok" : fbdo.errorReason().c_str());
          }
        }

        Firebase.RTDB.getJSON(&fbdo, plant_calibration_path);
        json = fbdo.jsonObject();
        json.get(calibration_state, "moisture_calibration_mode");

        // Check if the 5 minutes timeout has been exceeded
        if (millis() - calibrationStartTime > 30000) {
            Serial.println("Calibration timed out!\n");
            // change all variables in firebase back to initial value (0)
            json.add("dry_soil_measurment", 0);
            json.add("moisture_calibration_dry", 0);
            json.add("wet_soil_measurment", 0);
            json.add("moisture_calibration_wet", 0);
            json.add("moisture_calibration_mode", 0);
            // upload data to RTDB
            Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, plant_calibration_path, &json) ? "ok" : fbdo.errorReason().c_str());
            break; // Exit the loop after timeout
        }
      }

      Serial.print("existed calibration while loop\n");

    } else {
      // Failed to retrieve data, print error
      Serial.printf("Error getting calibration mode: %s , current plant id = %d\n", fbdo.errorReason().c_str(), plant_id);
      return;
    }

  } else {
      // Failed to retrieve data, print error
      Serial.printf("Error getting calibration directory: %s , current plant id = %d\n", fbdo.errorReason().c_str(), plant_id);
      return;
  }
  
}

// void get_existing_plants() {
//   for (int i = 0; i < 4; i++) {
//     String plant_path = garden_path + "/plants/plant" + String(i+1);
//     if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
//       FirebaseJson &json = fbdo.jsonObject();
//       FirebaseJsonData is_plant_exists;
      
//       if (json.get(is_plant_exists, "is_plant_exists")) { 
//         existing_plants[i] = is_plant_exists.intValue;
//       }
//     } else {
//       // TODO: add error indication in firebase
//       // error should be presented to user from application
//     }
//   }

//   // Print the array
//   Serial.print("Existing plants array: ");
//   for (int i = 0; i < 4; i++) {
//     Serial.print(existing_plants[i]);
//     if (i < 3) {
//       Serial.print(", ");  // Add a comma between elements except the last one
//     }
//   }
//   Serial.println();  // End with a newline
// }
