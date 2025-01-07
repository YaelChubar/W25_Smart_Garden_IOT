#ifndef CCONFIG_H_
#define CCONFIG_H_

#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <Adafruit_NeoPixel.h>
// ESP hard coded definitions
#define ESP_ID 1001

// ESP digital inputs
/* Moisture sensor unit test 
 * Connection layout:
 * GRD --> GRD
 * VCC --> 5V
 * AUOT --> D4 */
#define MOISTURE_SENSOR_PIN_1 34        // Analog pin connected to the first moisture sensor
#define MOISTURE_SENSOR_PIN_2 33        // Analog pin connected to the second moisture sensor
#define MOISTURE_SENSOR_PIN_3 32        // Analog pin connected to the third moisture sensor
#define MOISTURE_SENSOR_PIN_4 35        // Analog pin connected to the fourth moisture sensor

 /* Light Sensor UT.
 * Analog (S) --> D39 UN (needs to be ADC pin)
 * GRD (-) --> GRD
 * V (Mid) --> 3.3 V */
#define LIGHT_SENSOR_PIN 39
#define LIGHT_SENSOR_MIN_VALUE 0
#define LIGHT_SENSOR_MAX_VALUE 1700

/* Temprature & Humidity sensor unit test 
 * Connection layout:
 * S --> D34
 * Mid (+) --> 3.3V
 * (-) --> GRD */
#include <DFRobot_DHT11.h>
DFRobot_DHT11 DHT;
#define DHT11_PIN 4

/* Pumps unit test 
 * Connection layout:
 * YYNMOS-4 :
 * DC+ --> 5V+ (power supply)
 * DC- --> GRD- (power supply)
 * PWM1 --> D18 (esp32)
 * GRD1 --> GRD (esp32)
 * OUT1+ --> + (pump)
 * OUT1- --> - (pump)
 */
#define PUMP_PIN_NO_1 16
#define PUMP_PIN_NO_2 17
#define PUMP_PIN_NO_3 18
#define PUMP_PIN_NO_4 19

// Connection layout:
// Brown --> GRD
// Red --> Vin
// Orange --> D21
#include <ESP32Servo.h>
#define SERVO_PIN 21

/* Connection layout:
 * yellow --> GRD
 * red --> D15 (Touch 3) */
#define WATER_LEVEL_PIN T3
#define MIN_VALUE 30
#define MAX_VALUE 40

// Leds Connection layout:
// RGD --> GRD
// 5V --> Vin
// Din --> D12
#define LEDS_PIN        12 
#define NUMPIXELS 30 // Popular NeoPixel ring size

extern WiFiManager wifiManager;
extern FirebaseData fbdo;
extern FirebaseAuth auth;
extern FirebaseConfig config;


// Firebase relevant paths
extern String garden_path = "/gardens/garden_" + String(ESP_ID);
extern String garden_global_info_path = "/gardens/garden_" + String(ESP_ID) + "/global_info";
extern String plant1_path = "/gardens/garden_" + String(ESP_ID) + "/plants/plant1";
extern String plant2_path = "/gardens/garden_" + String(ESP_ID) + "/plants/plant2";
extern String plant3_path = "/gardens/garden_" + String(ESP_ID) + "/plants/plant3";
extern String plant4_path = "/gardens/garden_" + String(ESP_ID) + "/plants/plant4";

#endif
