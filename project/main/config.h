#ifndef CONFIG_H_
#define CONFIG_H_

#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <DFRobot_DHT11.h>
// ESP hard coded definitions
#define ESP_ID 1021

// ESP digital inputs
/* Moisture sensor unit test 
 * Connection layout:
 * GRD --> GRD
 * VCC --> 5V
 * AUOT --> D4 */
#define MOISTURE_SENSOR_PIN_1 35        // Analog pin connected to the first moisture sensor
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
#define SERVO_LID_CLOSED 0
#define SERVO_LID_OPEN 90

/* Connection layout:
 * yellow --> GRD
 * red --> D15 (Touch 3) */
#define WATER_LEVEL_PIN T3
#define WATER_LEVEL_MIN_VALUE 9
#define WATER_LEVEL_MAX_VALUE 26

// Leds Connection layout:
// RGD --> GRD
// 5V --> Vin
// Din --> D12
#define LEDS_PIN        12 
#define NUMPIXELS 30 // Popular NeoPixel ring size

// RGB light Connection layout:
// GRD --> GRD
// R --> D14
// G --> D17
// B --> D26
// each input requires 220 0hm resistor
#define RGB_RED_PIN 14
#define RGB_GREEN_PIN 27
#define RGB_BLUE_PIN  26

#define CALIBRATION_TIMEOUT 300000
#define LID_LIGHT_THRESHOLD 75
#define WATER_LEVEL_THRESHOLD  20
#define PUMP_WATER_TIME 2500

#define MOISTURE_LEVEL_LOW  10
#define MOISTURE_LEVEL_MID  30
#define MOISTURE_LEVEL_HIGH 50

//Offline default values
int existing_plants[4] = {0};
int dry_soil_default[4] = {0};
int wet_soil_default[4] = {0};
int lid_open = 0;
int leds_on = 0;
#define NEEDS_DIRECT_SUN_DEFAULT  0
#define MANUAL_MODE_DEFAULT 0
#define SOIL_MOISTURE_LEVEL_DEFAULT  2


// Firebase relevant paths
extern String garden_path = "/gardens/" + String(ESP_ID);
extern String garden_global_info_path = "/gardens/" + String(ESP_ID) + "/global_info";
extern String plant1_path = "/gardens/" + String(ESP_ID) + "/plants/plant1";
extern String plant2_path = "/gardens/" + String(ESP_ID) + "/plants/plant2";
extern String plant3_path = "/gardens/" + String(ESP_ID) + "/plants/plant3";
extern String plant4_path = "/gardens/" + String(ESP_ID) + "/plants/plant4";

#endif
