// Connection layout:
// Brown --> GRD
// Red --> Vin
// Orange --> D18

#include <ESP32Servo.h>

#define SERVO_PIN 21

Servo myServo;

void setup() {
  // put your setup code here, to run once:
  myServo.attach(SERVO_PIN);
  Serial.begin(115200);

}

void loop() {
  // put your main code here, to run repeatedly:
  myServo.write(30);
  Serial.print("Servo angle: 30\n");
  delay(5000);
  myServo.write(170);
  Serial.print("Servo angle: 170\n");
  delay(8000);

}
