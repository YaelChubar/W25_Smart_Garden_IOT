// ESP hard coded definitions
#define ESP_ID 1001

// ESP digital inputs
#define MOISTURE_SENSOR_PIN_1 33 

// Firebase relevant paths
extern String garden_path = "/gardens/garden_1001";
extern String garden_global_info_path = "/gardens/garden_1001/global_info";
extern String plant1_path = "/gardens/garden_" + String(ESP_ID) + "/plants/plant1";
extern String plant2_path = "/gardens/garden_1001/plants/plant2";
extern String plant3_path = "/gardens/garden_1001/plants/plant3";
extern String plant4_path = "/gardens/garden_1001/plants/plant4";
