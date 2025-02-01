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
unsigned long uploadSensorDataPrevMillis[4] = {0};
Servo myServo;
DFRobot_DHT11 DHT;
bool wifi_connected = false;
bool water_tank_empty = false;
Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);

// Task Handles
TaskHandle_t mainTaskHandle = NULL;
TaskHandle_t blinkTaskHandle = NULL;

unsigned long lastWiFiRetryMillis = 0;
const unsigned long WIFI_RETRY_INTERVAL = 6000; 

// Non-blocking blink task
void blinkTask(void *parameter) {
  while (!wifi_connected) {
    setColor(0, 0, 255); // Blue ON
    vTaskDelay(500 / portTICK_PERIOD_MS); // Delay for 500ms (FreeRTOS way)

    setColor(0, 0, 0); // Blue OFF
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }

  vTaskDelete(NULL); // Stop task after Wi-Fi connects
}

// Wi-Fi setup task with timeout
void initial_wifi_setup(void *parameter) {

  wifiManager.setTimeout(10);  // Timeout in seconds for captive portal
  wifiManager.setConnectTimeout(5); // Timeout for connecting to Wi-Fi
  wifiManager.setConfigPortalTimeout(30); // Total timeout for config portal mode

  wifiManager.resetSettings();
  Serial.println("Starting Wi-Fi configuration portal...");

  while (!wifi_connected) {
    if (wifiManager.startConfigPortal("SmartGarden-Setup")) {
      wifi_connected = true;
      Serial.println("Wi-Fi connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      setColor(0, 0, 0); // Turn off light
    }
  }

  // Notify the main task (setup) that Wi-Fi is connected
  if (mainTaskHandle != NULL) {
    xTaskNotifyGive(mainTaskHandle);
  }

  vTaskDelete(NULL); // Stop Wi-Fi setup task
}

void setup() {
  Serial.begin(115200);
  mainTaskHandle = xTaskGetCurrentTaskHandle(); // Store main task handle

  components_setup();

  // Create the Wi-Fi setup task
  xTaskCreatePinnedToCore(initial_wifi_setup, "WiFiTask", 5000, NULL, 1, NULL, 1); 

  // Create the blinking task
  xTaskCreatePinnedToCore(blinkTask, "BlinkTask", 1000, NULL, 1, &blinkTaskHandle, 0);

  // Wait until Wi-Fi is connected before continuing setup
  Serial.println("Waiting for Wi-Fi to connect...");
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Block setup() until Wi-Fi is done

  firebase_setup();

  // Configure NTP for time synchronization
  configureTime();
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
  // Servo setup
  myServo.attach(SERVO_PIN);

  // water pumps setup
  pinMode(PUMP_PIN_NO_1, OUTPUT);
  digitalWrite(PUMP_PIN_NO_1, LOW);
  pinMode(PUMP_PIN_NO_2, OUTPUT);
  digitalWrite(PUMP_PIN_NO_2, LOW);
  pinMode(PUMP_PIN_NO_3, OUTPUT);
  digitalWrite(PUMP_PIN_NO_3, LOW);
  pinMode(PUMP_PIN_NO_4, OUTPUT);
  digitalWrite(PUMP_PIN_NO_4, LOW);
  
  // leds setup
  pixels.begin(); 

  // RBG inidicator setup
  pinMode(RGB_RED_PIN,  OUTPUT);              
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);

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

//handle WiFi reconnection
void handleWiFiConnection() {
  // Check current WiFi status
  if (WiFi.status() == WL_CONNECTED && wifi_connected == false) {
    wifi_connected = true;
    if (!water_tank_empty) {
      setColor(0, 0, 0); // Turn off indicator
    }
  } else if (WiFi.status() != WL_CONNECTED) {
    wifi_connected = false;
    if (!water_tank_empty) {
      setColor(255, 0, 0); // Red indicator
    }

    // Only attempt reconnection if enough time has passed since last attempt
    if (millis() - lastWiFiRetryMillis >= WIFI_RETRY_INTERVAL) {
      Serial.println("Attempting to reconnect WiFi...");
      WiFi.disconnect();
      WiFi.begin(); // Attempt to reconnect with stored credentials
      lastWiFiRetryMillis = millis();
    }
  }
}

void loop() {
  // Check and handle WiFi connection
  handleWiFiConnection();

  if ((millis() - readDataPrevMillis > 1000 || readDataPrevMillis == 0))
  {
    readDataPrevMillis = millis();

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
        if (millis() - uploadSensorDataPrevMillis[i-1] > 10000) {
          uploadSensorDataPrevMillis[i-1] = millis();
          int soil_measurement = measure_current_moisture(i);
          if (soil_measurement != -1 && get_normalized_water_level() >= WATER_LEVEL_THRESHOLD) {
            water_plant(i,soil_measurement);
          }
        }
      }
      handle_lid_auto(measure_light_value());
      measure_DHT_values();
    }
      
    // both manual & auto
    measure_water_level_value();
    upload_handshake();
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
  FirebaseData fbdo_calibration;
  String plant_calibration_path = garden_path + "/plants/plant" + String(plant_id)+ "/calibration";
  int moisture_pin = get_moisture_sensor_pin_by_id(plant_id);
  int moisture_sensor_reading = 0;

  if (wifi_connected && Firebase.RTDB.getJSON(&fbdo_calibration, plant_calibration_path)) {
    FirebaseJson &json = fbdo_calibration.jsonObject();
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
            Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo_calibration, plant_calibration_path, &json) ? "ok" : fbdo_calibration.errorReason().c_str());
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
            Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo_calibration, plant_calibration_path, &json) ? "ok" : fbdo_calibration.errorReason().c_str());
          }
        }

        Firebase.RTDB.getJSON(&fbdo_calibration, plant_calibration_path);
        json = fbdo_calibration.jsonObject();
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
            Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo_calibration, plant_calibration_path, &json) ? "ok" : fbdo_calibration.errorReason().c_str());
            break; // Exit the loop after timeout
        }
        upload_handshake();
      }

    } else {
      // Failed to retrieve data, print error
      Serial.printf("Error getting calibration mode: %s , current plant id = %d\n", fbdo_calibration.errorReason().c_str(), plant_id);
      return;
    }

  } else {
      // Failed to retrieve data, print error
      Serial.printf("Error getting calibration directory: %s , current plant id = %d\n", fbdo_calibration.errorReason().c_str(), plant_id);
      return;
  }
  
}

bool is_plant_ready(int plant_id) {
  String plant_path = garden_path + "/plants/plant" + String(plant_id) + "/plant_calibrated";
  FirebaseData fbdo_plant_calibrated;
  if (Firebase.RTDB.getInt(&fbdo_plant_calibrated, plant_path)) { // Directly get the integer value
    int plant_calibrated = fbdo_plant_calibrated.intData(); // Retrieve the value from the response

    if (plant_calibrated == 1) {
      Serial.printf("Plant %d is ready\n", plant_id);
      existing_plants[plant_id - 1] = 1;
      return true;
    } else {
      Serial.printf("Plant %d is NOT ready\n", plant_id);
      existing_plants[plant_id - 1] = 0;
      return false;
    }
  } else {
    // Handle errors (e.g., path doesn't exist or connection issues)
    Serial.printf("Error getting plant %d data: %s\n", plant_id, "");
    if (existing_plants[plant_id - 1]) {
      return true;
    } else {
      return false;
    }
  }
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
        dry_soil_default[plant_id-1] = dry_measurement.intValue;
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
        wet_soil_default[plant_id-1] = wet_measurement.intValue;
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
    return 0;
  }
}

int measure_current_moisture(int plant_id) {
  String plant_moisture_sensor_path = garden_path + "/plants/plant" + String(plant_id) + "/moisture_sensor";
  int moisture_pin = get_moisture_sensor_pin_by_id(plant_id);
  int moisture_sensor_reading = 0;
  int normalized_moisture_val = 0;
  int dry_soil_val = get_dry_soil_measurement(plant_id);
  int wet_soil_val = get_wet_soil_measurement(plant_id);

  // preform sensor reading
  moisture_sensor_reading = analogRead(moisture_pin);
  Serial.print("current moisture sensor reading: ");
  Serial.println(moisture_sensor_reading);
  // calculate and print normalized value
  normalized_moisture_val = calculatePercentage(moisture_sensor_reading, wet_soil_val, dry_soil_val);
  if (normalized_moisture_val == -1) {
    Serial.printf("Invalid normalized val: wet soil val == dry soil val!\n", plant_id);
    return -1;
  }
  Serial.print("normalized moisture_sensor_reading: ");
  Serial.println(normalized_moisture_val);
  
  // Upload measured moisture value to Firebase (single value)
  if (Firebase.RTDB.setInt(&fbdo, plant_moisture_sensor_path, normalized_moisture_val)) {
    Serial.println("Normalized moisture value updated successfully.");
  } else {
    Serial.printf("Failed to update normalized moisture value: %s\n", fbdo.errorReason().c_str());
  }

  return normalized_moisture_val;
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
void water_plant(int plant_id, int normalized_moisture_val) {
  int pump_pin = get_plant_pump_pin_by_id(plant_id);
  int raw_moisture_level = get_soil_moisture_level(plant_id);
  int soil_moisture_level_val = 0;
  
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
  FirebaseData fbdo_DHT;
  
  // read temp & humi values
  DHT.read(DHT11_PIN);
  Serial.print("temp:");
  Serial.print(DHT.temperature);
  Serial.print("  humi:");
  Serial.println(DHT.humidity);

  // check that path exists
  if (wifi_connected && Firebase.RTDB.getJSON(&fbdo_DHT, garden_global_info_path)) {
    FirebaseJson &json = fbdo_DHT.jsonObject();
    //upload data
    json.add("garden_humidity", DHT.humidity);
    json.add("garden_temperature", DHT.temperature);
    Serial.printf("garden_temperature and humi vals pushed... %s\n\n", Firebase.RTDB.setJSON(&fbdo_DHT, garden_global_info_path, &json) ? "successfully" : fbdo_DHT.errorReason().c_str());
  }
}

int get_normalized_water_level() {
  int water_tank_read_raw = touchRead(WATER_LEVEL_PIN);
  water_tank_read_raw = constrain(water_tank_read_raw, 14, 22);
  int water_level_precentage = 0;
  
  switch (water_tank_read_raw) {
    case 21:
      water_level_precentage = 5;
      break;
    case 20:
      water_level_precentage = 10;
      break;
    case 19:
      water_level_precentage = 20;
      break;
    case 18:
      water_level_precentage = 30;
      break;
    case 17:
      water_level_precentage = 45;
      break;
    case 16:
      water_level_precentage = 60;
      break;
    case 15:
      water_level_precentage = 70;
      break;
    case 14:
      water_level_precentage = 90;
      break;
    default:
      water_level_precentage = 0;
      break;
  }
  
  if (water_level_precentage < WATER_LEVEL_THRESHOLD && !water_tank_empty) {
    // turn indicator yellow light on
    setColor(252,180,0);
    water_tank_empty = true;
  } else if (water_tank_empty && water_level_precentage >= WATER_LEVEL_THRESHOLD){
    water_tank_empty = false;
    if (wifi_connected) {
      // turn indicator light off
      setColor(0,0,0);
    }
  }

  return water_level_precentage;
}

void measure_water_level_value() {
  // measure water level
  FirebaseData fbdo_water_level;
  int normalized_water_tank_read = get_normalized_water_level();
  Serial.printf("measure_water_level_value func\n");

  // check if path exists
  if (wifi_connected && Firebase.RTDB.getJSON(&fbdo_water_level, garden_global_info_path)) {
    FirebaseJson &json = fbdo_water_level.jsonObject();
    //upload data
    json.add("water_level", normalized_water_tank_read);
    Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo_water_level, garden_global_info_path, &json) ? "ok" : fbdo_water_level.errorReason().c_str());
  } else {
    Serial.printf("Error getting garden_global_info_path: %s\n", fbdo_water_level.errorReason().c_str());
    return;
  }
}

int measure_light_value() {
  FirebaseData fbdo_light;
  int light_sensor_reading = analogRead(LIGHT_SENSOR_PIN);
  int normalized_light_sensor_reading = calculatePercentage(light_sensor_reading, LIGHT_SENSOR_MIN_VALUE, LIGHT_SENSOR_MAX_VALUE);

    // check if path exists
  if (wifi_connected && Firebase.RTDB.getJSON(&fbdo_light, garden_global_info_path)) {
    FirebaseJson &json = fbdo_light.jsonObject();
    //upload data
    json.add("garden_light_strength", normalized_light_sensor_reading);
    Serial.printf("garden_light_strength pushed... %s\n\n", Firebase.RTDB.setJSON(&fbdo_light, garden_global_info_path, &json) ? "successfully" : fbdo_light.errorReason().c_str());    
  } else {
    Serial.printf("Error getting garden_global_info_path: %s\n", fbdo_light.errorReason().c_str());
    return -1;
  }

  return normalized_light_sensor_reading;
}

void upload_handshake() {
  // check if path exists
  if (wifi_connected && Firebase.RTDB.getJSON(&fbdo_timestamp, garden_global_info_path)) {
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
    }
  }
}

void upload_irrigation_time(int plant_id) {
  // Upload irrigation time to Firebase (single value)
  String plant_irrigation_time_path = garden_path + "/plants/plant" + String(plant_id) + "/irrigation_time";
  FirebaseData fbdo_irrigation;
  String current_time = getCurrentDateTime();

  if (Firebase.RTDB.setString(&fbdo_irrigation, plant_irrigation_time_path, current_time)) {
    Serial.println("Irrigation time updated successfully.");
  } else {
    Serial.printf("Failed to update irrigation time: %s\n", fbdo_irrigation.errorReason().c_str());
  }
}

bool get_plant_pump_water(int plant_id) {
  FirebaseData fbdo_pump;
  // Direct path to the pump_water value
  String plant_path_pump_water = garden_path + "/plants/plant" + String(plant_id) + "/pump_water";

  // Directly fetch the pump_water value
  if (Firebase.RTDB.getInt(&fbdo_pump, plant_path_pump_water)) {
    int pump_water = fbdo_pump.intData(); // Get the value directly

    if (pump_water == 1) {
      // Set value back to 0 in Firebase after processing
      if (Firebase.RTDB.setInt(&fbdo_pump, plant_path_pump_water, 0)) {
        Serial.println("Successfully set pump_water to 0 in Firebase.");
        return true;
      } else {
        Serial.printf("Error setting pump_water to 0: %s\n", "");
        return false;
      }
    } else {
      return false;  // No water pump action required
    }
  } else {
    // Handle errors during fetching pump_water value
    Serial.printf("Error getting pump_water field for plant %d: %s\n", plant_id, "");
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
    // Directly get integer value from Firebase
    if (Firebase.RTDB.getInt(&fbdo, String(garden_global_info_path) + "/leds_on")) {
      leds_on = fbdo.intData();
      return leds_on;
    } else {
      Serial.printf("Error getting leds_on: %s\n", fbdo.errorReason().c_str());
      return leds_on;
    }
  } else { // No Wi-Fi connection / Firebase disconnected
    return leds_on;
  }
}

bool get_lid_status() {
  if (wifi_connected && Firebase.ready()) {
    // Directly get integer value from Firebase
    if (Firebase.RTDB.getInt(&fbdo, String(garden_global_info_path) + "/lid_open")) {
      lid_open = fbdo.intData();
      return lid_open;
    } else {
      Serial.printf("Error getting lid_open: %s\n", fbdo.errorReason().c_str());
      return lid_open;
    }
  } else { // No Wi-Fi connection / Firebase disconnected
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
  // Ensure Wi-Fi and Firebase connection
  FirebaseData fbdo_moisture;
  if (wifi_connected && Firebase.ready()) {
    // Direct path to the soil_moisture_level value
    String plant_path = garden_path + "/plants/plant" + String(plant_id) + "/soil_moisture_level";

    // Directly fetch the soil_moisture_level value
    if (Firebase.RTDB.getInt(&fbdo_moisture, plant_path)) {
      int soil_moisture_level = fbdo_moisture.intData(); // Get the value directly
      Serial.print("soil_moisture_level: ");
      Serial.println(soil_moisture_level);
      return soil_moisture_level;
    } else {
      // Handle errors during fetching soil_moisture_level value
      Serial.printf("Error getting plant %d soil_moisture_level: %s\n", plant_id, "");
      return SOIL_MOISTURE_LEVEL_DEFAULT;
    }
  } else {
    return SOIL_MOISTURE_LEVEL_DEFAULT; // Default value if Wi-Fi or Firebase is not ready
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
  FirebaseData fbdo_lid;
  if (wifi_connected && Firebase.RTDB.getJSON(&fbdo_lid, garden_global_info_path)) {
    FirebaseJson &json = fbdo_lid.jsonObject();
    //upload data
    json.add("lid_open", lid_mode);
    Serial.printf("Upload lid status to firebase... %s\n\n", Firebase.RTDB.setJSON(&fbdo_lid, garden_global_info_path, &json) ? "successfully" : fbdo_lid.errorReason().c_str());
  } else {
    Serial.printf("Error getting garden_global_info_path: %s\n", fbdo_lid.errorReason().c_str());
  }
  lid_open = lid_mode;
}

void setColor(int redValue, int greenValue,  int blueValue) {
  analogWrite(RGB_RED_PIN, redValue);
  analogWrite(RGB_GREEN_PIN,  greenValue);
  analogWrite(RGB_BLUE_PIN, blueValue);
}
