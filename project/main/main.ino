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
int get_moisture_sensor_pin_by_id(int plant_id);
void check_calibration_mode(int plant_id);
bool is_plant_ready(int plant_id);
void water_plant(int plant_id);
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
        // 1. Calibration mode
        // 2. Is ready
        // 3. Moisture sensor
        check_calibration_mode(i);
        // if not in calibration mode, check if ready
        if (!is_plant_ready(i)) {
          continue;
        }
        // check moisture and water if needed
        water_plant(i);
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

int get_moisture_sensor_pin_by_id(int plant_id) {
  switch (plant_id) {
    case 1:
      return MOISTURE_SENSOR_PIN_1;
    case 2:
      return MOISTURE_SENSOR_PIN_2;
    case 3:
      return MOISTURE_SENSOR_PIN_3;
    case 4:
      return MOISTURE_SENSOR_PIN_4;
    default:
      Serial.print("Invalid plant_id! \n");
      return -1;
  }
  return -1;
}

void check_calibration_mode(int plant_id) {
  String plant_calibration_path = garden_path + "/plants/plant" + String(plant_id)+ "/calibration";
  int moisture_pin = get_moisture_sensor_pin_by_id(plant_id);
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

bool is_plant_ready(int plant_id) {
  String plant_path = garden_path + "/plants/plant" + String(plant_id);
  if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    FirebaseJsonData is_plant_ready;
    
    if (json.get(is_plant_ready, "is_plant_ready")) { 
      if(is_plant_ready.intValue == 1) {
        Serial.printf("plant %d is ready\n", plant_id);
        return true;
      } else {
        Serial.printf("plant %d is NOT ready\n", plant_id);
        return false;
      }
    }
  } else {
    // TODO: add error indication in firebase
    Serial.printf("Error getting plant %d directory: %s\n", plant_id, fbdo.errorReason().c_str());
  }
  return false;
}


/* 
Flow: 
1. Read moisture calibration dry and wet values
2. Read soil moisture level (1/2/3)
3. get moisture pin according to plant id
4. Preform sensor reading
5. normalize reading to precent
6. If value is lower than the one set in soil moisture level, pump water for 5 seconds
*/
void water_plant(int plant_id) {
  String plant_path = garden_path + "/plants/plant" + String(plant_id);
  String plant_calibration_path = garden_path + "/plants/plant" + String(plant_id)+ "/calibration";
  int moisture_pin = get_moisture_sensor_pin_by_id(plant_id);
  int dry_soil_val = 0;
  int wet_soil_val = 0;
  int soil_moisture_level_val = 0;
  int moisture_sensor_reading = 0;

  // Read moisture calibration dry and wet values
  if (Firebase.RTDB.getJSON(&fbdo, plant_calibration_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    FirebaseJsonData dry_soil_measurment;
    FirebaseJsonData wet_soil_measurment;

    if (json.get(dry_soil_measurment, "dry_soil_measurment")) {
      Serial.print("dry_soil_measurment: ");
      Serial.println(dry_soil_measurment.intValue);
      dry_soil_val = dry_soil_measurment.intValue;
    } else {
      Serial.printf("Error getting plant %d dry_soil_measurment: %s\n", plant_id, fbdo.errorReason().c_str());
      return;
    }

    if (json.get(wet_soil_measurment, "wet_soil_measurment")) {
      Serial.print("wet_soil_measurment: ");
      Serial.println(wet_soil_measurment.intValue);
      wet_soil_val = wet_soil_measurment.intValue;
    } else {
      Serial.printf("Error getting plant %d wet_soil_measurment: %s\n", plant_id, fbdo.errorReason().c_str());
      return;
    }
  } else {
    Serial.printf("Error getting plant %d plant_calibration_path directory: %s\n", plant_id, fbdo.errorReason().c_str());
    return;
  }

  //Read soil moisture level (1/2/3)
  if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    FirebaseJsonData soil_moisture_level;

    if (json.get(soil_moisture_level, "soil_moisture_level")) {
      Serial.print("soil_moisture_level: ");
      Serial.println(soil_moisture_level.intValue);
  
      switch (soil_moisture_level.intValue) {
        case 1:
          soil_moisture_level_val = 10;
        case 2:
          soil_moisture_level_val = 30;
        case 3:
          soil_moisture_level_val = 50;
        default:
          Serial.printf("Invalid soil moisture val for plant %d: %s\n", plant_id, fbdo.errorReason().c_str());
          return;
      }

    } else {
      Serial.printf("Error getting plant %d soil_moisture_level: %s\n", plant_id, fbdo.errorReason().c_str());
      return;
    }
  } else {
    Serial.printf("Error getting plant %d plant_path directory: %s\n", plant_id, fbdo.errorReason().c_str());
    return;
  }


}








// void get_existing_plants() {
//   for (int i = 0; i < 4; i++) {
//     String plant_path = garden_path + "/plants/plant" + String(i+1);
//     if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
//       FirebaseJson &json = fbdo.jsonObject();
//       FirebaseJsonData is_plant_ready;
      
//       if (json.get(is_plant_ready, "is_plant_ready")) { 
//         existing_plants[i] = is_plant_ready.intValue;
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
