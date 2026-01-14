#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <cmath>

#include "hjorth_settings.h" 
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

float history_buffer[NUM_RAW_INPUTS][HJORTH_WINDOW_SIZE];
int buffer_idx = 0;
bool buffer_full = false;

// Helper: Calculate Variance
float calc_variance(const std::vector<float>& data) {
    if (data.size() < 2) return 0.0f;
    float mean = 0.0f;
    for (float v : data) mean += v;
    mean /= data.size();
    float var = 0.0f;
    for (float v : data) var += (v - mean) * (v - mean);
    return var / (data.size() - 1);
}

// ================= FEATURE EXTRACTION (HJORTH) =================
void extract_hjorth_features(float* raw, float* out) {
    for(int i=0; i<NUM_RAW_INPUTS; i++) {
        history_buffer[i][buffer_idx] = raw[i];
    }
    buffer_idx = (buffer_idx + 1) % HJORTH_WINDOW_SIZE;
    if (buffer_idx == 0) buffer_full = true;

    int f_idx = 0;
    int count = buffer_full ? HJORTH_WINDOW_SIZE : buffer_idx;

    for (int s = 0; s < NUM_RAW_INPUTS; s++) {
        std::vector<float> x;
        x.reserve(count);
        
        if (buffer_full) {
            for(int i=buffer_idx; i<HJORTH_WINDOW_SIZE; i++) x.push_back(history_buffer[s][i]);
            for(int i=0; i<buffer_idx; i++) x.push_back(history_buffer[s][i]);
        } else {
            for(int i=0; i<buffer_idx; i++) x.push_back(history_buffer[s][i]);
        }

        if (x.size() < 3) {
            out[f_idx++] = 0.0f; out[f_idx++] = 0.0f; out[f_idx++] = 0.0f;
            continue;
        }

        std::vector<float> dx;
        for(size_t i=1; i<x.size(); i++) dx.push_back(x[i] - x[i-1]);

        std::vector<float> ddx;
        for(size_t i=1; i<dx.size(); i++) ddx.push_back(dx[i] - dx[i-1]);

        float var_x = calc_variance(x);
        float activity = var_x;

        float var_dx = calc_variance(dx);
        float mobility = 0.0f;
        if (var_x > 1e-9f) mobility = sqrt(var_dx / var_x);

        float var_ddx = calc_variance(ddx);
        float mob_dx = 0.0f;
        if (var_dx > 1e-9f) mob_dx = sqrt(var_ddx / var_dx);

        float complexity = 0.0f;
        if (mobility > 1e-9f) complexity = mob_dx / mobility;

        out[f_idx++] = activity;
        out[f_idx++] = mobility;
        out[f_idx++] = complexity;
    }
}

void wifiConnect() {
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void mqttConnect() {
  client.setServer(MQTT_HOST, MQTT_PORT);
  while (!client.connected()) {
    if (client.connect("esp32_hjorth_rf", MQTT_USER, MQTT_PASSWD)) {
        client.subscribe(MQTT_TOPIC_DATA);
        // READY signal
        client.publish(MQTT_TOPIC_OUT, "{\"status\": \"READY\"}");
        Serial.println("MQTT Connected. Sent READY.");
    } else delay(2000);
  }
}

void onMqtt(char *topic, byte *payload, unsigned int length) {
    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, payload, length);
    if(error) return;

    // Check Reset
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

    static float features[HJORTH_N_FEATURES];
    unsigned long t0 = micros();
    extract_hjorth_features(raw, features);
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
    for(int i=0; i<NUM_RAW_INPUTS; i++) {
        for(int j=0; j<HJORTH_WINDOW_SIZE; j++) history_buffer[i][j] = 0.0f;
    }
    wifiConnect();
    client.setCallback(onMqtt);
    mqttConnect();
}

void loop() {
    if (!client.connected()) mqttConnect();
    client.loop();
}