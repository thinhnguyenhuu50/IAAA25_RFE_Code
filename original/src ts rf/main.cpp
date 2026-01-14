#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <cmath>

#include "tsassure_settings.h" 
#include "infer.h"

#define SERIAL_BAUD 9600

const char *WIFI_SSID = "393B_Home_1";
const char *WIFI_PASS = "1234567890";
const char *MQTT_HOST = "mqtt.abcsolutions.com.vn";
const int MQTT_PORT = 1883;
const char *MQTT_USER = "abcsolution";
const char *MQTT_PASSWD = "CseLAbC5c6";
const char *MQTT_TOPIC_DATA = "duy/sensorFault";
const char *MQTT_TOPIC_OUT = "duy/sensorDetection";

WiFiClient espClient;
PubSubClient client(espClient);

float prev_raw[NUM_RAW_INPUTS];
bool first_run = true;

// ================= FEATURE EXTRACTION =================
void extract_tsassure_features(float* raw, float* out) {
    int f_idx = 0;
    
    // 1. Raw & DiffMain
    out[f_idx++] = raw[IDX_MAIN_COL];
    for(int i=0; i<NUM_RAW_INPUTS; i++) {
        if(i == IDX_MAIN_COL) continue;
        out[f_idx++] = raw[i];
        out[f_idx++] = raw[IDX_MAIN_COL] - raw[i]; 
    }

    // 2. Speed
    for(int i=0; i<NUM_RAW_INPUTS; i++) {
        float diff = first_run ? 0.0f : (raw[i] - prev_raw[i]);
        out[f_idx++] = diff;
    }

    // 3. PRD
    float prd = 0.0f;
    if (!first_run) {
        float curr = raw[IDX_MAIN_COL];
        float prev = prev_raw[IDX_MAIN_COL];
        float denom = (curr + prev) * 0.5f;
        if(std::abs(denom) > 1e-9f) prd = std::abs(curr - prev) / denom;
    }
    out[f_idx++] = prd;

    // 4. Pairs
    #if NUM_PAIRS > 0
    for(int i=0; i<NUM_PAIRS; i++) {
        int c1 = CORR_PAIRS[i][0];
        int c2 = CORR_PAIRS[i][1];
        out[f_idx++] = raw[c1] - raw[c2];
    }
    #endif
    
    for(int i=0; i<NUM_RAW_INPUTS; i++) prev_raw[i] = raw[i];
    first_run = false;
}

void wifiConnect() {
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void mqttConnect() {
  client.setServer(MQTT_HOST, MQTT_PORT);
  while (!client.connected()) {
    if (client.connect("esp32_tsassure_rf", MQTT_USER, MQTT_PASSWD)) {
        client.subscribe(MQTT_TOPIC_DATA);
        client.publish(MQTT_TOPIC_OUT, "{\"status\": \"READY\"}");
        Serial.println("MQTT Connected. Sent READY.");
    }
    else delay(2000);
  }
}

void onMqtt(char *topic, byte *payload, unsigned int length) {
    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, payload, length);
    if(error) return;

    if (doc.containsKey("reset") && doc["reset"] == true) {
        Serial.println("RESET CMD. REBOOTING...");
        delay(100);
        ESP.restart();
        return;
    }

    String timeStr = doc["Time"] | "";
    float raw[NUM_RAW_INPUTS];
    raw[IDX_TEMPERATURE] = doc["Temperature"];
    raw[IDX_HUMIDITY] = doc["Humidity"];
    raw[IDX_HUMIDITY_WEATHERSTATION] = doc["Humidity_WeatherStation"];
    raw[IDX_TEMPERATURE_WEATHERSTATION] = doc["Temperature_WeatherStation"];
    
    if(isnan(raw[0])) return;

    static float features[TS_N_FEATURES];
    unsigned long t0 = micros();
    extract_tsassure_features(raw, features);
    float t_feat = (micros() - t0) / 1000.0f;
    
    float score = 0;
    unsigned long t1 = micros();
    int label = predict_rf(features, &score);
    float t_infer = (micros() - t1) / 1000.0f;

    String msg = timeStr + ",";
    msg += String(raw[IDX_TEMPERATURE],2)+","+String(raw[IDX_HUMIDITY],2)+","+
           String(raw[IDX_HUMIDITY_WEATHERSTATION],2)+","+String(raw[IDX_TEMPERATURE_WEATHERSTATION],2)+",";
    msg += String(label) + "," + String(t_feat, 3) + "," + String(t_infer, 3) + "," + String(score, 4);
    
    client.publish(MQTT_TOPIC_OUT, msg.c_str());
    Serial.println(msg);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    for(int i=0; i<NUM_RAW_INPUTS; i++) prev_raw[i] = 0.0f;
    wifiConnect();
    client.setCallback(onMqtt);
    mqttConnect();
}

void loop() {
    if (!client.connected()) mqttConnect();
    client.loop();
}