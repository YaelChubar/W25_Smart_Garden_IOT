/* Moisture sensor unit test 
 * Connection layout:
 * GRD --> GRD
 * VCC --> 5V
 * AUOT --> D4 */
#define MOISTURE_SENSOR_PIN_1 4        

int sensor_reading = 0;

void setup(void) {
  Serial.begin(9600);
}

void loop(void) {
  sensor_reading = analogRead(MOISTURE_SENSOR_PIN_1);
  Serial.print("moisture sensor #1: ");
  Serial.println(sensor_reading);
  delay(500);
}