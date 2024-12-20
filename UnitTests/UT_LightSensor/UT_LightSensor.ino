/* Light Sensor UT.
 * Analog (S) --> D2
 * GRD (-) --> GRD
 * V (Mid) --> 3.3 V */
#define LIGHT_SENSOR_PIN 35
#define LIGHT_SENSOR_MIN_VALUE 0
#define LIGHT_SENSOR_MAX_VALUE 1700

int light_sensor_reading;

// Function to calculate the percentage based on min and max values
int calculatePercentage(int rawValue) {
  if (rawValue < LIGHT_SENSOR_MIN_VALUE) rawValue = LIGHT_SENSOR_MIN_VALUE;
  if (rawValue > LIGHT_SENSOR_MAX_VALUE) rawValue = LIGHT_SENSOR_MAX_VALUE;

  int percentage = 100 - ((rawValue - LIGHT_SENSOR_MIN_VALUE) * 100 / (LIGHT_SENSOR_MAX_VALUE - LIGHT_SENSOR_MIN_VALUE));
  return percentage;
}

void setup() {
  Serial.begin(115200);

}

void loop() {
  light_sensor_reading = analogRead(LIGHT_SENSOR_PIN);

  Serial.print("light sensor value (%): ");
  Serial.println(calculatePercentage(light_sensor_reading));

  delay(100);
}