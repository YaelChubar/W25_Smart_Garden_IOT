#include "secrets.h"
#include "config.h"
#include "functions.h"

// Global variables
WiFiManager wifiManager;
FirebaseData fbdo;
FirebaseData fbdo_timestamp;
FirebaseAuth auth;
FirebaseConfig config;
int plants_num;
unsigned long readDataPrevMillis = 0;
unsigned long uploadSensorDataPrevMillis = 0;
Servo myServo;
DFRobot_DHT11 DHT;
bool wifi_connected;
Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);


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
    wifi_connected = false;
    Serial.println("Failed to connect and hit timeout\n");
    ESP.restart();  // Restart ESP32 if it fails to connect
    // TODO: add light indication
  }

  // Once connected, print the assigned IP address
  Serial.println("Wi-Fi connected!\n");
  Serial.print("IP Address: \n");
  Serial.println(WiFi.localIP());
  wifi_connected = true;
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
  myServo.attach(SERVO_PIN);
  pinMode(PUMP_PIN_NO_1, OUTPUT);
  digitalWrite(PUMP_PIN_NO_1, LOW);
  pinMode(PUMP_PIN_NO_2, OUTPUT);
  digitalWrite(PUMP_PIN_NO_2, LOW);
  pinMode(PUMP_PIN_NO_3, OUTPUT);
  digitalWrite(PUMP_PIN_NO_3, LOW);
  pinMode(PUMP_PIN_NO_4, OUTPUT);
  digitalWrite(PUMP_PIN_NO_4, LOW);
  pixels.begin(); 

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
  // check wifi connection
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
  } else {
    wifi_connected = false;
  }
  // TODO: set_wifi_light_indicator(bool wifi_connected);

  if (Firebase.ready() && (millis() - readDataPrevMillis > 15000 || readDataPrevMillis == 0))
  {
    readDataPrevMillis = millis();

    if (Firebase.RTDB.getJSON(&fbdo, garden_path)) {
      // per plant logic
      if (is_manual_mode()) {
        Serial.printf("Manual mode on\n");
        handle_manual_mode();
      
      } else { // automatic mode
        for (int i = 1; i < 5; i++) {
          // check if plant is in calibration mode
          check_calibration_mode(i);
          // if not in calibration mode, check if already calibrated
          if (!is_plant_ready(i)) {
            Serial.printf("Skipping plant %d as it is not ready.\n", i);
            continue;
          }
          // check moisture and water if needed
          if (millis() - uploadSensorDataPrevMillis > 60000 && get_normalized_water_level() >= WATER_LEVEL_THRESHOLD) {
            uploadSensorDataPrevMillis = millis();
            water_plant(i);
          }
        }
        handle_lid_auto(measure_light_value());
        measure_water_level_value();
        measure_DHT_values();
      }
      
      // both manual & auto
      upload_handshake();
      
    } else { //get garden path failed
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
            dry_soil_default[plant_id-1] = moisture_sensor_reading;
            json.add("dry_soil_measurement", moisture_sensor_reading);
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
            wet_soil_default[plant_id-1] = moisture_sensor_reading;
            json.add("wet_soil_measurement", moisture_sensor_reading);
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
        if (millis() - calibrationStartTime > CALIBRATION_TIMEOUT) {
            Serial.println("Calibration timed out!\n");
            // change all variables in firebase back to initial value (0)
            dry_soil_default[plant_id-1] = 0;
            wet_soil_default[plant_id-1] = 0;
            
            json.add("dry_soil_measurement", 0);
            json.add("moisture_calibration_dry", 0);
            json.add("wet_soil_measurement", 0);
            json.add("moisture_calibration_wet", 0);
            json.add("moisture_calibration_mode", 0);
            json.add("calibration_timeout", 1);
            // upload data to RTDB
            Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, plant_calibration_path, &json) ? "ok" : fbdo.errorReason().c_str());
            break; // Exit the loop after timeout
        }
        upload_handshake();
      }

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
        existing_plants[plant_id-1] = 1; 
        return true;
      } else {
        Serial.printf("plant %d is NOT ready\n", plant_id);
        existing_plants[plant_id-1] = 0; 
        return false;
      }
    } else {
      Serial.printf("Error getting plant_calibrated field for plant %d\n", plant_id);
    }
  } else {
    // plant directory does not exist
    Serial.printf("Error getting plant %d directory: %s\n", plant_id, fbdo.errorReason().c_str());
    existing_plants[plant_id-1] = 0; 
  }
  Serial.println("Returning false, end of is_plant_ready func\n");
  return false;
}

int get_dry_soil_measurement(int plant_id) {
  if (dry_soil_default[plant_id-1]) {
    return dry_soil_default[plant_id-1];
  }
  //read val from firebase
  if (wifi_connected && Firebase.ready()) {
    String plant_path = garden_path + "/plants/plant" + String(plant_id) + "/calibration";
    if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData dry_measurement;

      if (json.get(dry_measurement, "dry_soil_measurement")) {
        Serial.print("dry_measurement: ");
        Serial.println(dry_measurement.intValue);
        return dry_measurement.intValue;
      } else {
        Serial.printf("Error getting plant %d dry_measurement: %s\n", plant_id, fbdo.errorReason().c_str());
        return 0;
      }
    } else {
      Serial.printf("Error getting plant %d plant_path directory: %s\n", plant_id, fbdo.errorReason().c_str());
      return 0;
    }
  } else {
    Serial.printf("Error : wifi is not connected, no default value %s\n", plant_id, fbdo.errorReason().c_str());
    // TODO: add light inidicator
    return 0;
  }
}

int get_wet_soil_measurement(int plant_id) {
  if (wet_soil_default[plant_id-1]) {
    return wet_soil_default[plant_id-1];
  }
  //read val from firebase
  if (wifi_connected && Firebase.ready()) {
    String plant_path = garden_path + "/plants/plant" + String(plant_id) + "/calibration";
    if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData wet_measurement;

      if (json.get(wet_measurement, "wet_soil_measurement")) {
        Serial.print("wet_measurement: ");
        Serial.println(wet_measurement.intValue);
        return wet_measurement.intValue;
      } else {
        Serial.printf("Error getting plant %d wet_measurement: %s\n", plant_id, fbdo.errorReason().c_str());
        return 0;
      }
    } else {
      Serial.printf("Error getting plant %d plant_path directory: %s\n", plant_id, fbdo.errorReason().c_str());
      return 0;
    }
  } else {
    Serial.printf("Error : wifi is not connected, no default value %s\n", plant_id, fbdo.errorReason().c_str());
    // TODO: add light inidicator
    return 0;
  }
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
  String plant_moisture_sesnor_path = garden_path + "/plants/plant" + String(plant_id)+ "/moisture_sensor";
  int moisture_pin = get_moisture_sensor_pin_by_id(plant_id);
  int pump_pin = get_plant_pump_pin_by_id(plant_id);
  int dry_soil_val = get_dry_soil_measurement(plant_id);
  int wet_soil_val = get_wet_soil_measurement(plant_id);
  int raw_moisture_level = get_soil_moisture_level(plant_id);
  int soil_moisture_level_val = 0;
  int moisture_sensor_reading = 0;
  int normalized_moisture_val = 0;
  
  switch (raw_moisture_level) {
    case 1:
      soil_moisture_level_val = MOISTURE_LEVEL_LOW;
      break;
    case 2:
      soil_moisture_level_val = MOISTURE_LEVEL_MID;
      break;
    case 3:
      soil_moisture_level_val = MOISTURE_LEVEL_HIGH;
      break;
    default:
      Serial.printf("Invalid soil moisture val for plant %d: %s\n", plant_id, fbdo.errorReason().c_str());
      return;
  }

  // preform sensor reading
  moisture_sensor_reading = analogRead(moisture_pin);
  Serial.print("current moisture sensor reading: ");
  Serial.println(moisture_sensor_reading);
  // calculate and print normalized value
  normalized_moisture_val = calculatePercentage(moisture_sensor_reading, wet_soil_val, dry_soil_val);
  if (normalized_moisture_val == -1) {
    Serial.printf("Invalid normalized val: wet soil val == dry soil val!\n", plant_id);
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
    delay(PUMP_WATER_TIME);
    digitalWrite(pump_pin, LOW);
    upload_irrigation_time(plant_id);
  }
}


int calculatePercentage(int rawValue, int minValue, int maxValue) {
  // Ensure the maxValue is greater than the minValue to avoid division by zero
  if (maxValue <= minValue) {
    Serial.println("Error: maxValue should be greater than minValue");
    return -1;
  }

  // Constrain the rawValue to stay within the minValue and maxValue range
  rawValue = constrain(rawValue, minValue, maxValue);
  Serial.printf("rawValue = %d\n", rawValue);

  // Calculate moisture percentage (flip the range)
  int percentage = ((maxValue - rawValue) * 100) / (maxValue - minValue);
  Serial.printf("percentage = %d\n", percentage);

  return percentage;

}

void measure_DHT_values() {
  // read temp & humi values
  DHT.read(DHT11_PIN);
  Serial.print("temp:");
  Serial.print(DHT.temperature);
  Serial.print("  humi:");
  Serial.println(DHT.humidity);

  // check that path exists
  if (Firebase.RTDB.getJSON(&fbdo, garden_global_info_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    //upload data
    json.add("garden_humidity", DHT.humidity);
    json.add("garden_temperature", DHT.temperature);
    Serial.printf("garden_temperature and humi vals pushed... %s\n\n", Firebase.RTDB.setJSON(&fbdo, garden_global_info_path, &json) ? "successfully" : fbdo.errorReason().c_str());
  }
}

int get_normalized_water_level() {
  int water_tank_read_raw = touchRead(WATER_LEVEL_PIN);
  return calculatePercentage(water_tank_read_raw, WATER_LEVEL_MIN_VALUE, WATER_LEVEL_MAX_VALUE);
}

void measure_water_level_value() {
  // measure water level
  int normalized_water_tank_read = get_normalized_water_level();
  Serial.printf("measure_water_level_value func\n");

  // check if path exists
  if (Firebase.RTDB.getJSON(&fbdo, garden_global_info_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    //upload data
    json.add("water_level", normalized_water_tank_read);
    Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, garden_global_info_path, &json) ? "ok" : fbdo.errorReason().c_str());
  } else {
    Serial.printf("Error getting garden_global_info_path: %s\n", fbdo.errorReason().c_str());
    return;
  }
}

int measure_light_value() {
  int light_sensor_reading = analogRead(LIGHT_SENSOR_PIN);
  int normalized_light_sensor_reading = calculatePercentage(light_sensor_reading, LIGHT_SENSOR_MIN_VALUE, LIGHT_SENSOR_MAX_VALUE);

    // check if path exists
  if (Firebase.RTDB.getJSON(&fbdo, garden_global_info_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    //upload data
    json.add("garden_light_strength", normalized_light_sensor_reading);
    Serial.printf("garden_light_strength pushed... %s\n\n", Firebase.RTDB.setJSON(&fbdo, garden_global_info_path, &json) ? "successfully" : fbdo.errorReason().c_str());    
  } else {
    Serial.printf("Error getting garden_global_info_path: %s\n", fbdo.errorReason().c_str());
    return -1;
  }

  return normalized_light_sensor_reading;
}

void upload_handshake() {
  // check if path exists
  if (Firebase.RTDB.getJSON(&fbdo_timestamp, garden_global_info_path)) {
    FirebaseJson &json = fbdo_timestamp.jsonObject();
    //upload data
    json.add("timestamp_handshake", getCurrentDateTime());
    Serial.printf("timestamp_handshake pushed... %s\n\n", Firebase.RTDB.setJSON(&fbdo_timestamp, garden_global_info_path, &json) ? "successfully" : fbdo_timestamp.errorReason().c_str());    
  } else {
    Serial.printf("Error getting garden_global_info_path: %s\n", fbdo_timestamp.errorReason().c_str());
    return;
  }
}

  /* Lid: 
  1.if manual mode
        global_info
          lid_open == 1 --> open lid
                            update lid status
          lid open == 0 --> close lid
                            update lid satus
          leds_on == 1 --> turn on leds
                            update leds satus
          leds_on == 0 --> turn off leds
                            update leds satus
          pump_water == 1 --> for loop for each plant
                              water for 5 sec
                              update irrigation time for each plant
        per plant
          pump_water == 1 --> pump water for 5 sec
                              update irrigation time
  */
void handle_manual_mode() {
  bool leds_on = get_leds_status();
  turn_leds_on(leds_on);

  bool is_lid_open = get_lid_status();
  if (is_lid_open) {
    myServo.write(SERVO_LID_OPEN);
  } else {
    myServo.write(SERVO_LID_CLOSED);
  }

  //per plant
  for (int plant_id = 1; plant_id < 5; plant_id++) {
      plant_manual_pump(plant_id);
    
  }

}

void plant_manual_pump(int plant_id) {
  bool pump_water = get_plant_pump_water(plant_id);
  if (pump_water) {
    int pump_pin = get_plant_pump_pin_by_id(plant_id);
    // check there is enough water in tank
    if (get_normalized_water_level() >= WATER_LEVEL_THRESHOLD) {
      // get time in millies
      digitalWrite(pump_pin, HIGH);

      delay(PUMP_WATER_TIME);
      digitalWrite(pump_pin, LOW);
      upload_irrigation_time(plant_id);
    } else {
      // TODO: add light indication that water level is low.
    }
  }
}

void upload_irrigation_time(int plant_id) {
  //Upload irrigation time to Firebase
  String plant_irrigation_time_path = garden_path + "/plants/plant" + String(plant_id)+ "/irrigation_time";
  FirebaseJson json_irrigation_time;
  FirebaseData fbdo_irrigation;
  // TODO : change time val to water amount in ml
  json_irrigation_time.add(getCurrentDateTime(), 55);
  if (Firebase.RTDB.updateNode(&fbdo_irrigation, plant_irrigation_time_path, &json_irrigation_time)) {
    Serial.println("plant_irrigation_time pushed successfully:");
    Serial.println(fbdo_irrigation.pushName()); // The unique key for this entry
  } else {
    Serial.printf("Failed to push plant_irrigation_time_path: %s\n", fbdo_irrigation.errorReason().c_str());
  }
}

bool get_plant_pump_water(int plant_id) {
  String plant_path = garden_path + "/plants/plant" + String(plant_id);
  if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    FirebaseJsonData plant_pump_water;
    
    if (json.get(plant_pump_water, "pump_water")) { 
      if(plant_pump_water.intValue == 1) {
        // set value back to 0
        json.add("pump_water", 0);
        Serial.printf("Zero pump_water val in firebase... %s\n", Firebase.RTDB.setJSON(&fbdo, garden_global_info_path, &json) ? "successfully" : fbdo.errorReason().c_str());
        return true;
      } else {
        return false;
      }
    } else {
      Serial.printf("Error getting plant_pump_water field for plant %d\n", plant_id);
    }
  } else {
    // plant directory does not exist
    Serial.printf("Error getting plant %d directory: %s\n", plant_id, fbdo.errorReason().c_str());
  }
  Serial.println("Returning false, end of get_plant_pump_water func\n");
  return false;
}

void turn_leds_on(bool state) {
  pixels.clear(); // Set all pixel colors to 'off'
  if (state) {
    for(int i=0; i<NUMPIXELS; i++) { // For each pixel...
      // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    }
  } else { //state = 0
    for(int i=0; i<NUMPIXELS; i++) { // For each pixel...
      // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    }
  }
  pixels.show();   // Send the updated pixel colors to the hardware.
}

/* 
if led_on --> close_lid
   return
else
   automatically according to light strength
   - if light > 70 && needs_direct_sun==0 --> close lid
     update firebase
   - if light <= 70 && needs_direct_sun ==1 --> open lid
     update firebase
*/
void handle_lid_auto(int light_measurement) {
  if (leds_on) {
    myServo.write(SERVO_LID_CLOSED);
    update_lid_status(0);
    return;
  }

  bool needs_direct_sun = get_needs_direct_sun();
  if (light_measurement > LID_LIGHT_THRESHOLD && !needs_direct_sun) {
    myServo.write(SERVO_LID_CLOSED);
    update_lid_status(0);
  } else if (light_measurement <= LID_LIGHT_THRESHOLD && needs_direct_sun) {
    myServo.write(SERVO_LID_OPEN);
    update_lid_status(1);
  }
}

bool get_leds_status() {
  if (wifi_connected && Firebase.ready()) {
    // get_status_from_firebase
    if (Firebase.RTDB.getJSON(&fbdo, garden_global_info_path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData leds_status;
      if (json.get(leds_status, "leds_on")) {
        leds_on = leds_status.intValue;
        return leds_on ? true : false;
      } else {
        Serial.printf("Failed reading leds_status: %s\n", fbdo.errorReason().c_str());
        return leds_on;
      }
    } else {
      Serial.printf("Error getting garden_global_info_path: %s\n", fbdo.errorReason().c_str());
      return leds_on;
    }
  } else { //no wifi connection / firebase disconnected
    return leds_on;
  }
}

bool get_lid_status() {
  if (wifi_connected && Firebase.ready()) {
    // get_status_from_firebase
    if (Firebase.RTDB.getJSON(&fbdo, garden_global_info_path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData lid_status;
      if (json.get(lid_status, "lid_open")) {
        lid_open = lid_status.intValue;
        return lid_open ? true : false;
      } else {
        Serial.printf("Failed reading lid_status: %s\n", fbdo.errorReason().c_str());
        return lid_open;
      }
    } else {
      Serial.printf("Error getting garden_global_info_path: %s\n", fbdo.errorReason().c_str());
      return lid_open;
    }
  } else { //no wifi connection / firebase disconnected
    return lid_open;
  }
}

bool get_needs_direct_sun() {
  if (wifi_connected && Firebase.ready()) {
    // get_status_from_firebase
    if (Firebase.RTDB.getJSON(&fbdo, garden_global_info_path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData needs_direct_sun;
      if (json.get(needs_direct_sun, "needs_direct_sun")) {
        int needs_direct_sun_val = needs_direct_sun.intValue;
        return needs_direct_sun_val ? true : false;
      } else {
        Serial.printf("Failed reading needs_direct_sun_val: %s\n", fbdo.errorReason().c_str());
        return NEEDS_DIRECT_SUN_DEFAULT;
      }
    } else {
      Serial.printf("Error getting garden_global_info_path: %s\n", fbdo.errorReason().c_str());
      return NEEDS_DIRECT_SUN_DEFAULT;
    }
  } else { //no wifi connection / firebase disconnected
    return NEEDS_DIRECT_SUN_DEFAULT;
  }
}

int get_soil_moisture_level(int plant_id) {
  if (wifi_connected && Firebase.ready()) {
    //Read soil moisture level (1/2/3)
    String plant_path = garden_path + "/plants/plant" + String(plant_id);
    if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData soil_moisture_level;

      if (json.get(soil_moisture_level, "soil_moisture_level")) {
        Serial.print("soil_moisture_level: ");
        Serial.println(soil_moisture_level.intValue);
        return soil_moisture_level.intValue;
      } else {
        Serial.printf("Error getting plant %d soil_moisture_level: %s\n", plant_id, fbdo.errorReason().c_str());
        return SOIL_MOISTURE_LEVEL_DEFAULT;
      }
    } else {
      Serial.printf("Error getting plant %d plant_path directory: %s\n", plant_id, fbdo.errorReason().c_str());
      return SOIL_MOISTURE_LEVEL_DEFAULT;
    }
  } else {
    return SOIL_MOISTURE_LEVEL_DEFAULT;
  }
}

bool is_manual_mode() {
  if (wifi_connected && Firebase.ready()) {
    // get_status_from_firebase
    if (Firebase.RTDB.getJSON(&fbdo, garden_global_info_path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData manual_mode;
      if (json.get(manual_mode, "manual_mode")) {
        int manual_mode_val = manual_mode.intValue;
        return manual_mode_val ? true : false;
      } else {
        Serial.printf("Failed reading manual_mode_val: %s\n", fbdo.errorReason().c_str());
        return MANUAL_MODE_DEFAULT;
      }
    } else {
      Serial.printf("Error getting garden_global_info_path: %s\n", fbdo.errorReason().c_str());
      return MANUAL_MODE_DEFAULT;
    }
  } else { //no wifi connection / firebase disconnected
    return MANUAL_MODE_DEFAULT;
  }
}

void update_lid_status(int lid_mode) {
  // check if path exists
  if (Firebase.RTDB.getJSON(&fbdo, garden_global_info_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    //upload data
    json.add("lid_open", lid_mode);
    Serial.printf("Upload lid status to firebase... %s\n\n", Firebase.RTDB.setJSON(&fbdo, garden_global_info_path, &json) ? "successfully" : fbdo.errorReason().c_str());
  } else {
    Serial.printf("Error getting garden_global_info_path: %s\n", fbdo.errorReason().c_str());
  }
  lid_open = lid_mode;
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
