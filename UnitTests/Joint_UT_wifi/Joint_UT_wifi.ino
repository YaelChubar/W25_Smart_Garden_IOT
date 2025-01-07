#include <WiFi.h>
#include "secrets.h"
#include "config.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <Adafruit_NeoPixel.h>


// Global variables
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long readDataPrevMillis = 0;
int sensor_reading = 0;
int light_sensor_reading;
int waterTankReadRaw;
Servo myServo;

Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);

// Function to calculate the percentage based on min and max values
int calculatePercentage(int rawValue) {
  if (rawValue < LIGHT_SENSOR_MIN_VALUE) rawValue = LIGHT_SENSOR_MIN_VALUE;
  if (rawValue > LIGHT_SENSOR_MAX_VALUE) rawValue = LIGHT_SENSOR_MAX_VALUE;

  int percentage = 100 - ((rawValue - LIGHT_SENSOR_MIN_VALUE) * 100 / (LIGHT_SENSOR_MAX_VALUE - LIGHT_SENSOR_MIN_VALUE));
  return rawValue;
}

void setup(void) {
  myServo.attach(SERVO_PIN);
  Serial.begin(115200);
  pinMode(PUMP_PIN_NO_1, OUTPUT);
  digitalWrite(PUMP_PIN_NO_1, HIGH);
  pinMode(PUMP_PIN_NO_2, OUTPUT);
  digitalWrite(PUMP_PIN_NO_2, HIGH);
  pinMode(PUMP_PIN_NO_3, OUTPUT);
  digitalWrite(PUMP_PIN_NO_3, HIGH);
  pinMode(PUMP_PIN_NO_4, OUTPUT);
  digitalWrite(PUMP_PIN_NO_4, HIGH);

  // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.begin(); 


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

void loop(void) {
  sensor_reading = analogRead(MOISTURE_SENSOR_PIN_1);
  Serial.print("moisture sensor #1: ");
  Serial.println(sensor_reading);
  sensor_reading = analogRead(MOISTURE_SENSOR_PIN_2);
  Serial.print("moisture sensor #2: ");
  Serial.println(sensor_reading);
  sensor_reading = analogRead(MOISTURE_SENSOR_PIN_3);
  Serial.print("moisture sensor #3: ");
  Serial.println(sensor_reading);
  sensor_reading = analogRead(MOISTURE_SENSOR_PIN_4);
  Serial.print("moisture sensor #4: ");
  Serial.println(sensor_reading);
  
  light_sensor_reading = analogRead(LIGHT_SENSOR_PIN);
  Serial.print("light sensor value (%): ");
  Serial.println(calculatePercentage(light_sensor_reading));
  
  DHT.read(DHT11_PIN);
  Serial.print("temp:");
  Serial.print(DHT.temperature);
  Serial.print("  humi:");
  Serial.println(DHT.humidity);

  waterTankReadRaw = touchRead(WATER_LEVEL_PIN);
  int waterTankRead = calculatePercentage(waterTankReadRaw);

  Serial.print("Water tank level at: ");
  Serial.print(waterTankRead);
  Serial.println("%");

  pixels.clear(); // Set all pixel colors to 'off'
  for(int i=0; i<NUMPIXELS; i++) { // For each pixel...

    // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
    // Here we're using a moderately bright green color:
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));

    pixels.show();   // Send the updated pixel colors to the hardware.
  }

  //if (Serial.available()) {
    //int angle = Serial.parseInt();
    myServo.write(10);
    double current = myServo.read();
    printf("current angle = %f\n", current);
    delay(1000);
    myServo.write(100);
    current = myServo.read();
    printf("current angle = %f\n", current);
    delay(1000);
  //}
  
  //testing one pump at a time
  digitalWrite(PUMP_PIN_NO_1, LOW);
  digitalWrite(PUMP_PIN_NO_2, LOW);
  digitalWrite(PUMP_PIN_NO_3, LOW);
  digitalWrite(PUMP_PIN_NO_4, LOW);
  delay(1000);
  digitalWrite(PUMP_PIN_NO_1, HIGH);
  digitalWrite(PUMP_PIN_NO_2, HIGH);
  digitalWrite(PUMP_PIN_NO_3, HIGH);
  digitalWrite(PUMP_PIN_NO_4, HIGH);
  delay(1000);

  
}