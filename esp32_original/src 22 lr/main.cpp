#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <cmath>
#include <algorithm>

#include "catch22_settings.h" 
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

float history_buffer[NUM_RAW_INPUTS][C22_WINDOW_SIZE];
int buffer_idx = 0;
bool buffer_full = false;

// ================= CATCH22 ALGORITHMS =================
float c22_histogram_mode(const std::vector<float>& x, int bins) {
    if(x.empty()) return 0.0f;
    float min_v = x[0], max_v = x[0];
    for(float v : x) {
        if(v < min_v) min_v = v;
        if(v > max_v) max_v = v;
    }
    if (abs(max_v - min_v) < 1e-9) return 0.0f;

    std::vector<int> counts(bins, 0);
    float step = (max_v - min_v) / bins;
    
    for(float v : x) {
        int idx = (int)((v - min_v) / step);
        if(idx >= bins) idx = bins - 1;
        counts[idx]++;
    }
    
    int max_count = -1;
    int max_idx = 0;
    for(int i=0; i<bins; i++) {
        if(counts[i] > max_count) {
            max_count = counts[i];
            max_idx = i;
        }
    }
    return min_v + (max_idx + 0.5f) * step;
}

float c22_co_f1ecac(const std::vector<float>& x) {
    size_t N = x.size();
    if(N < 2) return 0.0f;
    float mean = 0.0f;
    for(float v : x) mean += v;
    mean /= N;
    float var = 0.0f;
    for(float v : x) var += (v - mean) * (v - mean);
    if(var < 1e-9) return (float)N;
    float thresh = 0.367879f; 
    for(size_t tau=1; tau < N; tau++) {
        float cov = 0.0f;
        for(size_t i=0; i < N - tau; i++) cov += (x[i] - mean) * (x[i+tau] - mean);
        if(cov / var < thresh) return (float)tau;
    }
    return (float)N;
}

float c22_co_first_min_ac(const std::vector<float>& x) {
    size_t N = x.size();
    if(N < 2) return 0.0f;
    float mean = 0.0f;
    for(float v : x) mean += v;
    mean /= N;
    float var = 0.0f;
    for(float v : x) var += (v - mean) * (v - mean);
    if(var < 1e-9) return 0.0f;
    float prev_ac = 1.0f;
    for(size_t tau=1; tau < N; tau++) {
        float cov = 0.0f;
        for(size_t i=0; i < N - tau; i++) cov += (x[i] - mean) * (x[i+tau] - mean);
        float ac = cov / var;
        if (ac > prev_ac) return (float)(tau - 1);
        prev_ac = ac;
    }
    return (float)N;
}

float c22_co_trev_1_num(const std::vector<float>& x) {
    size_t N = x.size();
    if(N < 2) return 0.0f;
    float sum_val = 0.0f;
    for(size_t i=0; i < N - 1; i++) {
        float diff = x[i+1] - x[i];
        sum_val += (diff * diff * diff);
    }
    return sum_val / (N - 1);
}

float c22_md_hrv_pnn40(const std::vector<float>& x) {
    size_t N = x.size();
    if(N < 2) return 0.0f;
    int count = 0;
    for(size_t i=0; i < N - 1; i++) {
        if(abs(x[i+1] - x[i]) > 0.04f) count++;
    }
    return (float)count / (N - 1);
}

void extract_catch22_features(float* raw, float* out) {
    for(int i=0; i<NUM_RAW_INPUTS; i++) {
        history_buffer[i][buffer_idx] = raw[i];
    }
    buffer_idx = (buffer_idx + 1) % C22_WINDOW_SIZE;
    if (buffer_idx == 0) buffer_full = true;

    int f_idx = 0;
    int count = buffer_full ? C22_WINDOW_SIZE : buffer_idx;

    for (int s = 0; s < NUM_RAW_INPUTS; s++) {
        std::vector<float> x;
        x.reserve(count);
        if (buffer_full) {
            for(int i=buffer_idx; i<C22_WINDOW_SIZE; i++) x.push_back(history_buffer[s][i]);
            for(int i=0; i<buffer_idx; i++) x.push_back(history_buffer[s][i]);
        } else {
            for(int i=0; i<buffer_idx; i++) x.push_back(history_buffer[s][i]);
        }
        
        if (x.size() < 5) {
            for(int k=0; k<6; k++) out[f_idx++] = 0.0f;
            continue;
        }

        out[f_idx++] = c22_histogram_mode(x, 5);
        out[f_idx++] = c22_histogram_mode(x, 10);
        out[f_idx++] = c22_co_f1ecac(x);
        out[f_idx++] = c22_co_first_min_ac(x);
        out[f_idx++] = c22_co_trev_1_num(x);
        out[f_idx++] = c22_md_hrv_pnn40(x);
    }
}

void wifiConnect() {
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void mqttConnect() {
  client.setServer(MQTT_HOST, MQTT_PORT);
  while (!client.connected()) {
    if (client.connect("esp32_catch22_lr", MQTT_USER, MQTT_PASSWD)) {
        client.subscribe(MQTT_TOPIC_DATA);
        // READY signal
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

    static float features[C22_N_FEATURES];
    unsigned long t0 = micros();
    extract_catch22_features(raw, features);
    float t_feat = (micros() - t0) / 1000.0f;
    
    float score = 0;
    unsigned long t1 = micros();
    int label = predict_lr(features, &score); 
    float t_infer = (micros() - t1) / 1000.0f;

    // Strict CSV Format
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
        for(int j=0; j<C22_WINDOW_SIZE; j++) history_buffer[i][j] = 0.0f;
    }
    wifiConnect();
    client.setCallback(onMqtt);
    mqttConnect();
}

void loop() {
    if (!client.connected()) mqttConnect();
    client.loop();
}