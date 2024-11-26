/* Light Sensor UT.
 * Analog (S) --> D2
 * GRD (-) --> GRD
 * V (Mid) --> 3.3 V */
#define LIGHT_SENSOR_PIN 2

int light_sensor_reading;
void setup() {
  Serial.begin(9600);

}

void loop() {
  light_sensor_reading = analogRead(LIGHT_SENSOR_PIN);

  Serial.print("light sensor value (raw): ");
  Serial.println(light_sensor_reading);

  delay(100);
}