#include "FS.h"
#include "LittleFS.h"

void setup() {
  Serial.begin(115200);

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  } else {
    Serial.println("LittleFS Mounted Successfully");
  }

  // Create or open a file to store data
  File file = LittleFS.open("/data.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Write data to the file
  file.println("Sensor data here...");
  file.close();  // Don't forget to close the file after writing
  Serial.println("Data written to file");
  
  // Read from the file
  file = LittleFS.open("/data.txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  // Read the file and print to Serial Monitor
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void loop() {
  // Your loop code here
}
