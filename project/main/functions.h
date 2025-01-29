#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_

// Functions
void initial_wifi_setup();
void firebase_setup();
void components_setup();
void configureTime();
void check_calibration_mode(int plant_id);
bool is_plant_ready(int plant_id);
void water_plant(int plant_id, int normalized_moisture_val);
int calculatePercentage(int rawValue, int min_value, int max_value);
void measure_DHT_values();
void measure_water_level_value();
int measure_light_value();
void upload_handshake();
void handle_lid_auto(int light_measurement);
void update_lid_status(int lid_mode);
void turn_leds_on(bool state);
void plant_manual_pump(int plant_id);
void upload_irrigation_time(int plant_id);
int measure_current_moisture(int plant_id);

//getter & setter functions
String getCurrentDateTime();
bool is_manual_mode();
int get_normalized_water_level();
int get_plant_pump_pin_by_id(int plant_id);
int get_moisture_sensor_pin_by_id(int plant_id);
bool get_needs_direct_sun();
bool get_leds_status();
bool get_lid_status();
int get_soil_moisture_level(int plant_id);
bool get_plant_pump_water(int plant_id);
int get_dry_soil_measurement(int plant_id);
int get_wet_soil_measurement(int plant_id);
void setColor(int redValue, int greenValue,  int blueValue);

#endif