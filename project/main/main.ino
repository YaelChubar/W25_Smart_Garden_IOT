#include "secrets.h"
#include "config.h"
#include "calibration.h"


// Global variables


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
void get_existing_plants();

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
  Serial.println("Starting Wi-Fi configuration portal...");
  if (!wifiManager.startConfigPortal("SmartGarden-Setup")) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();  // Restart ESP32 if it fails to connect
  }

  // Once connected, print the assigned IP address
  Serial.println("Wi-Fi connected!");
  Serial.print("IP Address: ");
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
      for (int i = 0; i < 4; i++) {
        //for each plant, check:
        // Calibration mode
        // Is exists
        // Moisture sensor
        check_calibration(i);
      }
      
      // Garden exists in firebase
      // Check existing plants
      get_existing_plants();


      
      
    } else {
      //TODO: add indication that garden was not found in firebase
      // OPTIONAL: red light indicator on esp OR error indication in firebase

    }



  }

}

void get_existing_plants() {
  for (int i = 0; i < 4; i++) {
    String plant_path = garden_path + "/plants/plant" + String(i+1);
    if (Firebase.RTDB.getJSON(&fbdo, plant_path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData is_plant_exists;
      
      if (json.get(is_plant_exists, "is_plant_exists")) { 
        existing_plants[i] = is_plant_exists.intValue;
      }
    } else {
      // TODO: add error indication in firebase
      // error should be presented to user from application
    }
  }

  // Print the array
  Serial.print("Existing plants array: ");
  for (int i = 0; i < 4; i++) {
    Serial.print(existing_plants[i]);
    if (i < 3) {
      Serial.print(", ");  // Add a comma between elements except the last one
    }
  }
  Serial.println();  // End with a newline
}
