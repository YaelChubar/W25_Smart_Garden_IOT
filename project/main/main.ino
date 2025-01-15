#include "secrets.h"
#include "config.h"

// Global variables
WiFiManager wifiManager;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
int plants_num;
unsigned long readDataPrevMillis = 0;
Servo myServo;
DFRobot_DHT11 DHT;
// int existing_plants[4] = {0};

Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);

// Functions
void initial_wifi_setup();
void firebase_setup();
void components_setup();
void configureTime();
String getCurrentDateTime();
int get_moisture_sensor_pin_by_id(int plant_id);
int get_plant_pump_pin_by_id(int plant_id);
void check_calibration_mode(int plant_id);
bool is_plant_ready(int plant_id);
void water_plant(int plant_id);
int calculatePercentage(int rawValue, int min_value, int max_value);
void measure_DHT_values();
void measure_water_level_value();
//void measure_light_value_handle_lid();
//void get_existing_plants();

void setup() {
  Serial.begin(115200);

  initial_wifi_setup();

  firebase_setup();

  components_setup();

  // Configure NTP for time synchronization
  configureTime();
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

//setup pumps, leds and servo
void components_setup() {
  pinMode(PUMP_PIN_NO_1, OUTPUT);
  digitalWrite(PUMP_PIN_NO_1, LOW);
  pinMode(PUMP_PIN_NO_2, OUTPUT);
  digitalWrite(PUMP_PIN_NO_2, LOW);
  pinMode(PUMP_PIN_NO_3, OUTPUT);
  digitalWrite(PUMP_PIN_NO_3, LOW);
  pinMode(PUMP_PIN_NO_4, OUTPUT);
  digitalWrite(PUMP_PIN_NO_4, LOW);
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

void loop() {
  if (Firebase.ready() && (millis() - readDataPrevMillis > 15000 || readDataPrevMillis == 0))
  {
    readDataPrevMillis = millis();

    if (Firebase.RTDB.getJSON(&fbdo, garden_path)) {
      // per plant logic
      for (int i = 1; i < 5; i++) {
        // check if plant is in calibration mode
        check_calibration_mode(i);
        // if not in calibration mode, check if already calibrated
        if (!is_plant_ready(i)) {
          Serial.printf("Skipping plant %d as it is not ready.\n", i);
          continue;
        }
        // check moisture and water if needed
        water_plant(i);
      }
      
      // garden global info logic
      // TODO: DHT - measure and upload data (temp & humi)
      //       water level - measure and upload
      //       light sensor - measure, upload data and close lid if garden needs_direct_sun is 0
      //       timestamp handshake - upload (overwrite previous value)
      measure_DHT_values();
      measure_water_level_value();
      //measure_light_value_handle_lid();


      
      
    } else {
      //TODO: add indication that garden was not found in firebase
      // OPTIONAL: red light indicator on esp OR error indication in firebase
      Serial.println("Error: Garden data not found in Firebase.");
    }



  }
  // Delay main loop for 1 minute
  // delay(60000);

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

int get_plant_pump_pin_by_id(int plant_id) {
  switch (plant_id) {
    case 1:
      return PUMP_PIN_NO_1;
    case 2:
      return PUMP_PIN_NO_2;
    case 3:
      return PUMP_PIN_NO_3;
    case 4:
      return PUMP_PIN_NO_4;
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
    FirebaseJsonData plant_calibrated;
    
    if (json.get(plant_calibrated, "plant_calibrated")) { 
      if(plant_calibrated.intValue == 1) {
        Serial.printf("plant %d is ready\n", plant_id);
        return true;
      } else {
        Serial.printf("plant %d is NOT ready\n", plant_id);
        return false;
      }
    } else {
      Serial.printf("Error getting plant_calibrated field for plant %d\n", plant_id);
    }
  } else {
    // TODO: add error indication in firebase
    Serial.printf("Error getting plant %d directory: %s\n", plant_id, fbdo.errorReason().c_str());
  }
  Serial.println("Returning false, end of is_plant_ready func\n");
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
  String plant_moisture_sesnor_path = garden_path + "/plants/plant" + String(plant_id)+ "/moisture_sensor";
  String plant_irrigation_time_path = garden_path + "/plants/plant" + String(plant_id)+ "/irrigation_time";
  int moisture_pin = get_moisture_sensor_pin_by_id(plant_id);
  int pump_pin = get_plant_pump_pin_by_id(plant_id);
  int dry_soil_val = 0;
  int wet_soil_val = 0;
  int soil_moisture_level_val = 0;
  int moisture_sensor_reading = 0;
  int normalized_moisture_val = 0;
  

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
          break;
        case 2:
          soil_moisture_level_val = 30;
          break;
        case 3:
          soil_moisture_level_val = 50;
          break;
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

  // preform sensor reading
  moisture_sensor_reading = analogRead(moisture_pin);
  Serial.print("moisture sensor in dry soil: ");
  Serial.println(moisture_sensor_reading);
  // calculat and print normalized value
  normalized_moisture_val = calculatePercentage(moisture_sensor_reading, wet_soil_val, dry_soil_val);
  if (normalized_moisture_val == -1) {
    Serial.printf("Invalid normalized val: wet soil val == dry soil val! %s\n", plant_id, fbdo.errorReason().c_str());
    return;
  }
  Serial.print("normalized moisture_sensor_reading: ");
  Serial.println(normalized_moisture_val);
  
  //Upload measured moisture val to Firebase
  FirebaseJson json_normalized_moisture;
  json_normalized_moisture.add(getCurrentDateTime(), normalized_moisture_val);
  if (Firebase.RTDB.updateNode(&fbdo, plant_moisture_sesnor_path, &json_normalized_moisture)) {
    Serial.println("Normalized_moisture_val pushed successfully:");
    Serial.println(fbdo.pushName()); // The unique key for this entry
  } else {
    Serial.printf("Failed to push Normalized_moisture_val: %s\n", fbdo.errorReason().c_str());
  }

  // If value is lower than the one set in soil moisture level, pump water for 5 seconds
  if (normalized_moisture_val <= soil_moisture_level_val) {
    digitalWrite(pump_pin, HIGH);
    delay(5000);
    digitalWrite(pump_pin, LOW);

    //Upload irrigation time to Firebase
    FirebaseJson json_irrigation_time;
    json_irrigation_time.add(getCurrentDateTime(), 5000);
    if (Firebase.RTDB.updateNode(&fbdo, plant_irrigation_time_path, &json_irrigation_time)) {
      Serial.println("plant_irrigation_time pushed successfully:");
      Serial.println(fbdo.pushName()); // The unique key for this entry
    } else {
      Serial.printf("Failed to push plant_irrigation_time_path: %s\n", fbdo.errorReason().c_str());
    }
  }
}


int calculatePercentage(int rawValue, int min_value, int max_value) {
  if (min_value == max_value) {
    return -1;
  }
  if (rawValue < min_value) rawValue = min_value;
  if (rawValue > max_value) rawValue = max_value;
  return (((rawValue - min_value) * 100) / (max_value - min_value));
}

void measure_DHT_values() {
  String garden_temprature_path = garden_global_info_path + "/garden_temprature";
  String garden_humidity_path = garden_global_info_path + "/garden_humidity";
  FirebaseJson json_DHT;
  
  // read temp & humi values
  DHT.read(DHT11_PIN);
  Serial.print("temp:");
  Serial.print(DHT.temperature);
  Serial.print("  humi:");
  Serial.println(DHT.humidity);

  // upload data to firebase
  json_DHT.add(getCurrentDateTime(), DHT.temperature);
  if (Firebase.RTDB.updateNode(&fbdo, garden_temprature_path, &json_DHT)) {
    Serial.println("garden_temprature_val pushed successfully:");
    Serial.println(fbdo.pushName()); // The unique key for this entry
  } else {
    Serial.printf("Failed to push garden_temprature_val: %s\n", fbdo.errorReason().c_str());
  }

  json_DHT.add(getCurrentDateTime(), DHT.humidity);
  if (Firebase.RTDB.updateNode(&fbdo, garden_humidity_path, &json_DHT)) {
    Serial.println("garden_humidity_val pushed successfully:");
    Serial.println(fbdo.pushName()); // The unique key for this entry
  } else {
    Serial.printf("Failed to push garden_humidity_val: %s\n", fbdo.errorReason().c_str());
  }
}

void measure_water_level_value() {
  // measure water level
  int water_tank_read_raw = touchRead(WATER_LEVEL_PIN);
  int normalized_water_tank_read = calculatePercentage(water_tank_read_raw, WATER_LEVEL_MIN_VALUE, WATER_LEVEL_MAX_VALUE);

  //upload data
  String garden_water_level_path = garden_global_info_path + "/water_level";
  FirebaseJson json_water_level;
  json_water_level.add(getCurrentDateTime(), normalized_water_tank_read);
  if (Firebase.RTDB.updateNode(&fbdo, garden_water_level_path, &json_water_level)) {
    Serial.println("normalized_water_tank_read pushed successfully:");
    Serial.println(fbdo.pushName()); // The unique key for this entry
  } else {
    Serial.printf("Failed to push normalized_water_tank_read: %s\n", fbdo.errorReason().c_str());
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
