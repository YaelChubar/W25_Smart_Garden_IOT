// Connection layout:
// Brown --> GRD
// Red --> Vin
// Orange --> D18

#include <ESP32Servo.h>

#define SERVO_PIN 18

Servo myServo;

void setup() {
  // put your setup code here, to run once:
  myServo.attach(SERVO_PIN);
  Serial.begin(115200);

}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) {
    int angle = Serial.parseInt();
    myServo.write(angle);
    double current = myServo.read();
    printf("current angle = %f\n", current);
  }
  delay(20);
}
