#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <cmath>
#include <algorithm>

#include "rfe_settings.h"
#include "infer.h"

#define SERIAL_BAUD 9600

// ================= CONFIGURATION =================
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

// ================= GLOBAL STATE =================
uint32_t sample_count = 0;
uint32_t last_ts = 0;

// ================= RING BUFFER =================
struct RingBuffer {
  std::vector<float> data;
  int size, head, count;
  RingBuffer(int s) : size(s), head(0), count(0) { data.resize(s); }

  void push(float val) {
    data[head] = val;
    head = (head + 1) % size;
    if (count < size) count++;
  }

  float get_stat(int stat_type) {
    if (count == 0) return 0.0f;
    if (stat_type == 0) { // Mean
      float sum = 0;
      int idx = (head - 1 + size) % size;
      for (int i = 0; i < count; i++) {
        sum += data[idx];
        idx = (idx - 1 + size) % size;
      }
      return sum / count;
    }

    std::vector<float> valid_data;
    valid_data.reserve(count);
    int idx = (head - 1 + size) % size;
    for (int i = 0; i < count; i++) {
      valid_data.push_back(data[idx]);
      idx = (idx - 1 + size) % size;
    }

    if (stat_type == 4) return *std::min_element(valid_data.begin(), valid_data.end());
    if (stat_type == 5) return *std::max_element(valid_data.begin(), valid_data.end());

    if (stat_type == 1) { // Median
      std::sort(valid_data.begin(), valid_data.end());
      if (count % 2 == 0) return (valid_data[count / 2 - 1] + valid_data[count / 2]) / 2.0f;
      else return valid_data[count / 2];
    }

    float sum = 0; for (float x : valid_data) sum += x;
    float mean = sum / count;
    float sq_sum = 0; for (float x : valid_data) sq_sum += (x - mean) * (x - mean);
    float var = (count > 1) ? sq_sum / (count - 1) : 0.0f;

    if (stat_type == 3) return var;
    if (stat_type == 2) return sqrt(var);
    return 0.0f;
  }
};

struct ChannelState {
  float prev_val = NAN;
  float ewma_val = NAN;
  RingBuffer *roll_raw[2];
  RingBuffer *roll_diff[2];
  float lags[3] = {NAN, NAN, NAN};
  ChannelState() {
    roll_raw[0] = new RingBuffer(5); roll_raw[1] = new RingBuffer(15);
    roll_diff[0] = new RingBuffer(5); roll_diff[1] = new RingBuffer(15);
  }
  void update(float x) {
    float diff = isnan(prev_val) ? 0.0f : (x - prev_val);
    for (int i = 0; i < 2; i++) { roll_raw[i]->push(x); roll_diff[i]->push(diff); }
    lags[2] = lags[1]; lags[1] = lags[0]; lags[0] = isnan(prev_val) ? x : prev_val;
    if (isnan(ewma_val)) ewma_val = x;
    else ewma_val = 0.333f * x + 0.667f * ewma_val;
    prev_val = x;
  }
};
ChannelState channels[NUM_RAW_INPUTS];
RingBuffer time_diff_5(5);
RingBuffer time_diff_15(15);

void extract_features_generic(float *raw_inputs, const FeatureSpec *specs, int n_specs, float *out_feat) {
  for (int i = 0; i < n_specs; i++) {
    FeatureSpec s = specs[i];
    float val = 0.0f;
    int win_idx = (s.window == 15) ? 1 : 0;
    switch (s.kind) {
      case FEAT_RAW: val = raw_inputs[s.channel1]; break;
      case FEAT_INTER: val = raw_inputs[s.channel1] * raw_inputs[s.channel2]; break;
      case FEAT_DIFF:
        val = raw_inputs[s.channel1] - channels[s.channel1].prev_val;
        if (isnan(val)) val = 0.0f; break;
      case FEAT_ROLL_RAW: val = channels[s.channel1].roll_raw[win_idx]->get_stat(s.stat); break;
      case FEAT_ROLL_DIFF: val = channels[s.channel1].roll_diff[win_idx]->get_stat(s.stat); break;
      case FEAT_LAG:
        if (s.lag >= 1 && s.lag <= 3) val = channels[s.channel1].lags[s.lag - 1];
        if (isnan(val)) val = raw_inputs[s.channel1]; break;
      case FEAT_EWMA: val = channels[s.channel1].ewma_val; break;
      case FEAT_ROLL_TD: val = (s.window == 5) ? time_diff_5.get_stat(2) : time_diff_15.get_stat(2); break;
      default: val = 0.0f;
    }
    out_feat[i] = val;
  }
}

void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}
void mqttConnect() {
  client.setServer(MQTT_HOST, MQTT_PORT);
  while (!client.connected()) {
    if (client.connect("esp32_dual_rfe_svm", MQTT_USER, MQTT_PASSWD)) {
      client.subscribe(MQTT_TOPIC_DATA);
      
      // === NEW: Send READY signal for data_reset.py ===
      client.publish(MQTT_TOPIC_OUT, "{\"status\": \"READY\"}");
      Serial.println("MQTT Connected. Sent READY signal.");
      
    } else delay(2000);
  }
}

void onMqtt(char *topic, byte *payload, unsigned int length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.print("JSON Error: "); Serial.println(error.c_str());
    return;
  }

  // === NEW: Check for Reset Command ===
  if (doc.containsKey("reset") && doc["reset"] == true) {
    Serial.println("RESET COMMAND RECEIVED. REBOOTING...");
    delay(100);
    ESP.restart();
    return;
  }
  
  // Ensure Time exists
  String timeStr = doc["Time"] | "";
  
  uint32_t ts = millis() / 1000;
  float raw[NUM_RAW_INPUTS];
  raw[IDX_TEMPERATURE] = doc["Temperature"];
  raw[IDX_HUMIDITY] = doc["Humidity"];
  raw[IDX_HUMIDITY_WEATHERSTATION] = doc["Humidity_WeatherStation"];
  raw[IDX_TEMPERATURE_WEATHERSTATION] = doc["Temperature_WeatherStation"];
  if (isnan(raw[0])) return;

  float dt = (last_ts == 0) ? 0.0f : (float)(ts - last_ts);
  last_ts = ts;
  time_diff_5.push(dt); time_diff_15.push(dt);
  for (int i = 0; i < NUM_RAW_INPUTS; i++) channels[i].update(raw[i]);
  sample_count++;

  float score = 0;
  int label = 0;
  unsigned long t0 = micros();
  float t_feat = 0;

  if (sample_count < WARMUP_PERIOD) {
    static float feat_cold[N_FEATURES_COLD];
    unsigned long t_f_start = micros();
    extract_features_generic(raw, FEATURE_SPECS_COLD, N_FEATURES_COLD, feat_cold);
    t_feat = (micros() - t_f_start) / 1000.0f;
    label = predict_cold(feat_cold, &score);
  } else {
    static float feat_warm[N_FEATURES_WARM];
    unsigned long t_f_start = micros();
    extract_features_generic(raw, FEATURE_SPECS_WARM, N_FEATURES_WARM, feat_warm);
    t_feat = (micros() - t_f_start) / 1000.0f;
    label = predict_warm(feat_warm, &score);
  }
  float t_total = (micros() - t0) / 1000.0f;
  float t_infer = t_total - t_feat;

  // Strict CSV Format: Time,Temp,Hum,HumWS,TempWS,Label,FeatTime,TestTime,Score
  String msg = timeStr + ",";
  msg += String(raw[IDX_TEMPERATURE], 2) + ",";
  msg += String(raw[IDX_HUMIDITY], 2) + ",";
  msg += String(raw[IDX_HUMIDITY_WEATHERSTATION], 2) + ",";
  msg += String(raw[IDX_TEMPERATURE_WEATHERSTATION], 2) + ",";
  msg += String(label) + ",";
  msg += String(t_feat, 3) + ",";
  msg += String(t_infer, 3) + ",";
  msg += String(score, 4);

  client.publish(MQTT_TOPIC_OUT, msg.c_str());
  Serial.println(msg);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  wifiConnect();
  client.setCallback(onMqtt);
  mqttConnect();
}
void loop() {
  if (!client.connected()) mqttConnect();
  client.loop();
}