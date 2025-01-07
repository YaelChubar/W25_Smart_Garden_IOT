#include "calibration.h"
#include "config.h"

void check_calibration(int plant_id) {
  String plant_calibration_path = garden_path + "/plants/plant" + String(plant_id+1)+ "/calibration";
  if (Firebase.RTDB.getJSON(&fbdo, plant_calibration_path)) {
    FirebaseJson &json = fbdo.jsonObject();
    FirebaseJsonData calibration_state;
    FirebaseJsonData moisture_calibration_dry;
    FirebaseJsonData moisture_calibration_wet;
    if (json.get(calibration_state, "moisture_calibration_mode")) {
      Serial.print("moisture_calibration_mode: ");
      Serial.println(calibration_state.intValue);
      while (calibration_state.intValue == 1) {
        Serial.print("while calibration mode ");
      }
    }

  }
}