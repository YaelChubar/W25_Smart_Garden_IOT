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

void setup() {
  pinMode(PUMP_PIN_NO_1, OUTPUT);
    digitalWrite(PUMP_PIN_NO_1, HIGH);
}

void loop() {
  //testing one pump at a time
    digitalWrite(PUMP_PIN_NO_1, LOW);
    delay(1000);
    digitalWrite(PUMP_PIN_NO_1, HIGH);
    delay(1000);
  }
}
