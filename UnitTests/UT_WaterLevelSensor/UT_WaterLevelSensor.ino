#define PUMP_PIN_NO_1 16
#define PUMP_PIN_NO_2 17
#define PUMP_PIN_NO_3 18
#define PUMP_PIN_NO_4 19

void setup() {
  pinMode(PUMP_PIN_NO_1, OUTPUT);
  pinMode(PUMP_PIN_NO_2, OUTPUT);
  pinMode(PUMP_PIN_NO_3, OUTPUT);
  pinMode(PUMP_PIN_NO_4, OUTPUT);
    digitalWrite(PUMP_PIN_NO_1, HIGH);
    digitalWrite(PUMP_PIN_NO_2, HIGH);
    digitalWrite(PUMP_PIN_NO_3, HIGH);
    digitalWrite(PUMP_PIN_NO_4, HIGH);
}

void loop() {
  //testing one pump at a time
  for(int i = 0; i < 4; i++){
    digitalWrite(PUMP_PIN_NO_1 + (i%4), LOW);
    digitalWrite(PUMP_PIN_NO_1 + ((i + 1)%4), HIGH);
    digitalWrite(PUMP_PIN_NO_1 + ((i + 2)%4), HIGH);
    digitalWrite(PUMP_PIN_NO_1 + ((i + 3)%4), HIGH);
    delay(1000);
  }
  //testing functionality of two pumps together
  for(int i = 0; i < 4; i++){
    digitalWrite(PUMP_PIN_NO_1 + (i%4), LOW);
    digitalWrite(PUMP_PIN_NO_1 + ((i + 1)%4), HIGH);
    digitalWrite(PUMP_PIN_NO_1 + ((i + 2)%4), LOW);
    digitalWrite(PUMP_PIN_NO_1 + ((i + 3)%4), HIGH);
    delay(1000);
  }
  //activating all pumps at the same time to check over power consumption
  for(int i = 0; i < 4; i++){
    if(i%2 == 0){
      digitalWrite(PUMP_PIN_NO_1 + (i%4), LOW);
      digitalWrite(PUMP_PIN_NO_1 + ((i + 1)%4), LOW);
      digitalWrite(PUMP_PIN_NO_1 + ((i + 2)%4), LOW);
      digitalWrite(PUMP_PIN_NO_1 + ((i + 3)%4), LOW);
    }
    else{
      digitalWrite(PUMP_PIN_NO_1 + (i%4), HIGH);
      digitalWrite(PUMP_PIN_NO_1 + ((i + 1)%4), HIGH);
      digitalWrite(PUMP_PIN_NO_1 + ((i + 2)%4), HIGH);
      digitalWrite(PUMP_PIN_NO_1 + ((i + 3)%4), HIGH);
    }
    delay(1000);
  }

  
}