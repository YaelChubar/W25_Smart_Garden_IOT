/* Connection layout:
 * yellow --> GRD
 * red --> D4 (Touch 0) */
#define WATER_LEVEL_PIN T3
#define MIN_VALUE 7
#define MAX_VALUE 33


// Function to calculate the percentage based on min and max values
int calculatePercentage(int rawValue) {
  if (rawValue < MIN_VALUE) rawValue = MIN_VALUE;
  if (rawValue > MAX_VALUE) rawValue = MAX_VALUE;

  int percentage = 100 - ((rawValue - MIN_VALUE) * 100 / (MAX_VALUE - MIN_VALUE));
  return percentage;
}

void setup() {
  Serial.begin(115200);
}

// The water level sensor is built like a wire capacitore and act like a capacitive sensor
void loop() {
  int waterTankReadRaw = touchRead(WATER_LEVEL_PIN);

  Serial.print("Water tank level at: ");
  Serial.print(waterTankReadRaw);
  Serial.print("\n");

  delay(1000);
}
