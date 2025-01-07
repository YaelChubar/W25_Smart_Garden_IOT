#include <WiFiManager.h>

void setup() {
  Serial.begin(115200);
  
  // Create WiFiManager object
  WiFiManager wifiManager;

  // Reset Wi-Fi credentials (remove any saved Wi-Fi credentials from ESP32)
  wifiManager.resetSettings();

  // Start the Wi-Fi configuration portal in Access Point (AP) mode
  Serial.println("Starting Wi-Fi configuration portal...");
  if (!wifiManager.startConfigPortal("SmartGarden")) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();  // Restart ESP32 if it fails to connect
  }

  // Once connected, print the assigned IP address
  Serial.println("Wi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Your main code here (e.g., smart garden sensor code)
}
