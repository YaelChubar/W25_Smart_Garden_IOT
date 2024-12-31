/* Moisture sensor unit test 
 * Connection layout:
 * GRD --> GRD
 * VCC --> 5V
 * AUOT --> D4 */
#define MOISTURE_SENSOR_PIN_1 25        // Analog pin connected to the first moisture sensor
#define MOISTURE_SENSOR_PIN_2 33        // Analog pin connected to the second moisture sensor
#define MOISTURE_SENSOR_PIN_3 32        // Analog pin connected to the third moisture sensor
#define MOISTURE_SENSOR_PIN_4 35        // Analog pin connected to the fourth moisture sensor

 /* Light Sensor UT.
 * Analog (S) --> D26 (needs to be ADC pin)
 * GRD (-) --> GRD
 * V (Mid) --> 3.3 V */
#define LIGHT_SENSOR_PIN 26
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
#define PUMP_PIN_NO_1 18
#define PUMP_PIN_NO_2 16

// Connection layout:
// Brown --> GRD
// Red --> Vin
// Orange --> D18
#include <ESP32Servo.h>
#define SERVO_PIN 21

int sensor_reading = 0;
int light_sensor_reading;
Servo myServo;


// Function to calculate the percentage based on min and max values
int calculatePercentage(int rawValue) {
  //if (rawValue < LIGHT_SENSOR_MIN_VALUE) rawValue = LIGHT_SENSOR_MIN_VALUE;
  //if (rawValue > LIGHT_SENSOR_MAX_VALUE) rawValue = LIGHT_SENSOR_MAX_VALUE;

  //int percentage = 100 - ((rawValue - LIGHT_SENSOR_MIN_VALUE) * 100 / (LIGHT_SENSOR_MAX_VALUE - LIGHT_SENSOR_MIN_VALUE));
  return rawValue;
}

void setup(void) {
  myServo.attach(SERVO_PIN);
  Serial.begin(115200);
  pinMode(PUMP_PIN_NO_1, OUTPUT);
  digitalWrite(PUMP_PIN_NO_1, HIGH);
  pinMode(PUMP_PIN_NO_2, OUTPUT);
  digitalWrite(PUMP_PIN_NO_2, HIGH);
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
  delay(1000);
  digitalWrite(PUMP_PIN_NO_1, HIGH);
  digitalWrite(PUMP_PIN_NO_2, HIGH);
  delay(1000);
}