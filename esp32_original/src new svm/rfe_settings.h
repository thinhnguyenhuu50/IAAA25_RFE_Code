#pragma once

#define IDX_TEMPERATURE 0
#define IDX_HUMIDITY 1
#define IDX_TEMPERATURE_WEATHERSTATION 2
#define IDX_HUMIDITY_WEATHERSTATION 3
#define NUM_RAW_INPUTS 4
#define WARMUP_PERIOD 15
#define N_FEATURES_COLD 10
#define N_FEATURES_WARM 94

enum FeatureKind { FEAT_RAW, FEAT_DIFF, FEAT_ROLL_RAW, FEAT_ROLL_DIFF, FEAT_LAG, FEAT_EWMA, FEAT_INTER, FEAT_ROLL_TD, FEAT_UNKNOWN };
typedef struct { FeatureKind kind; int stat; int window; int lag; int channel1; int channel2; } FeatureSpec;

static const FeatureSpec FEATURE_SPECS_COLD[] = {
  { FEAT_RAW, -1, 0, 0, 0, -1 }, // Temperature
  { FEAT_RAW, -1, 0, 0, 1, -1 }, // Humidity
  { FEAT_RAW, -1, 0, 0, 2, -1 }, // Temperature_WeatherStation
  { FEAT_RAW, -1, 0, 0, 3, -1 }, // Humidity_WeatherStation
  { FEAT_INTER, -1, 0, 0, 0, 1 }, // inter_Temperature_x_Humidity
  { FEAT_INTER, -1, 0, 0, 0, 2 }, // inter_Temperature_x_Temperature_WeatherStation
  { FEAT_INTER, -1, 0, 0, 0, 3 }, // inter_Temperature_x_Humidity_WeatherStation
  { FEAT_INTER, -1, 0, 0, 1, 2 }, // inter_Humidity_x_Temperature_WeatherStation
  { FEAT_INTER, -1, 0, 0, 1, 3 }, // inter_Humidity_x_Humidity_WeatherStation
  { FEAT_INTER, -1, 0, 0, 2, 3 }, // inter_Temperature_WeatherStation_x_Humidity_WeatherStation
};
static const FeatureSpec FEATURE_SPECS_WARM[] = {
  { FEAT_RAW, -1, 0, 0, 0, -1 }, // Temperature
  { FEAT_RAW, -1, 0, 0, 1, -1 }, // Humidity
  { FEAT_RAW, -1, 0, 0, 2, -1 }, // Temperature_WeatherStation
  { FEAT_RAW, -1, 0, 0, 3, -1 }, // Humidity_WeatherStation
  { FEAT_DIFF, -1, 0, 0, 0, -1 }, // speed_change_Temperature
  { FEAT_DIFF, -1, 0, 0, 1, -1 }, // speed_change_Humidity
  { FEAT_DIFF, -1, 0, 0, 2, -1 }, // speed_change_Temperature_WeatherStation
  { FEAT_DIFF, -1, 0, 0, 3, -1 }, // speed_change_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 0, 5, 0, 0, -1 }, // rolling_mean_5_Temperature
  { FEAT_ROLL_RAW, 1, 5, 0, 0, -1 }, // rolling_median_5_Temperature
  { FEAT_ROLL_RAW, 2, 5, 0, 0, -1 }, // rolling_std_5_Temperature
  { FEAT_ROLL_RAW, 3, 5, 0, 0, -1 }, // rolling_var_5_Temperature
  { FEAT_ROLL_RAW, 4, 5, 0, 0, -1 }, // rolling_min_5_Temperature
  { FEAT_ROLL_RAW, 5, 5, 0, 0, -1 }, // rolling_max_5_Temperature
  { FEAT_ROLL_RAW, 0, 5, 0, 1, -1 }, // rolling_mean_5_Humidity
  { FEAT_ROLL_RAW, 1, 5, 0, 1, -1 }, // rolling_median_5_Humidity
  { FEAT_ROLL_RAW, 2, 5, 0, 1, -1 }, // rolling_std_5_Humidity
  { FEAT_ROLL_RAW, 3, 5, 0, 1, -1 }, // rolling_var_5_Humidity
  { FEAT_ROLL_RAW, 4, 5, 0, 1, -1 }, // rolling_min_5_Humidity
  { FEAT_ROLL_RAW, 5, 5, 0, 1, -1 }, // rolling_max_5_Humidity
  { FEAT_ROLL_RAW, 0, 5, 0, 2, -1 }, // rolling_mean_5_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 1, 5, 0, 2, -1 }, // rolling_median_5_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 2, 5, 0, 2, -1 }, // rolling_std_5_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 3, 5, 0, 2, -1 }, // rolling_var_5_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 4, 5, 0, 2, -1 }, // rolling_min_5_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 5, 5, 0, 2, -1 }, // rolling_max_5_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 0, 5, 0, 3, -1 }, // rolling_mean_5_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 1, 5, 0, 3, -1 }, // rolling_median_5_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 2, 5, 0, 3, -1 }, // rolling_std_5_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 3, 5, 0, 3, -1 }, // rolling_var_5_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 4, 5, 0, 3, -1 }, // rolling_min_5_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 5, 5, 0, 3, -1 }, // rolling_max_5_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 0, 15, 0, 0, -1 }, // rolling_mean_15_Temperature
  { FEAT_ROLL_RAW, 1, 15, 0, 0, -1 }, // rolling_median_15_Temperature
  { FEAT_ROLL_RAW, 2, 15, 0, 0, -1 }, // rolling_std_15_Temperature
  { FEAT_ROLL_RAW, 3, 15, 0, 0, -1 }, // rolling_var_15_Temperature
  { FEAT_ROLL_RAW, 4, 15, 0, 0, -1 }, // rolling_min_15_Temperature
  { FEAT_ROLL_RAW, 5, 15, 0, 0, -1 }, // rolling_max_15_Temperature
  { FEAT_ROLL_RAW, 0, 15, 0, 1, -1 }, // rolling_mean_15_Humidity
  { FEAT_ROLL_RAW, 1, 15, 0, 1, -1 }, // rolling_median_15_Humidity
  { FEAT_ROLL_RAW, 2, 15, 0, 1, -1 }, // rolling_std_15_Humidity
  { FEAT_ROLL_RAW, 3, 15, 0, 1, -1 }, // rolling_var_15_Humidity
  { FEAT_ROLL_RAW, 4, 15, 0, 1, -1 }, // rolling_min_15_Humidity
  { FEAT_ROLL_RAW, 5, 15, 0, 1, -1 }, // rolling_max_15_Humidity
  { FEAT_ROLL_RAW, 0, 15, 0, 2, -1 }, // rolling_mean_15_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 1, 15, 0, 2, -1 }, // rolling_median_15_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 2, 15, 0, 2, -1 }, // rolling_std_15_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 3, 15, 0, 2, -1 }, // rolling_var_15_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 4, 15, 0, 2, -1 }, // rolling_min_15_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 5, 15, 0, 2, -1 }, // rolling_max_15_Temperature_WeatherStation
  { FEAT_ROLL_RAW, 0, 15, 0, 3, -1 }, // rolling_mean_15_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 1, 15, 0, 3, -1 }, // rolling_median_15_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 2, 15, 0, 3, -1 }, // rolling_std_15_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 3, 15, 0, 3, -1 }, // rolling_var_15_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 4, 15, 0, 3, -1 }, // rolling_min_15_Humidity_WeatherStation
  { FEAT_ROLL_RAW, 5, 15, 0, 3, -1 }, // rolling_max_15_Humidity_WeatherStation
  { FEAT_ROLL_DIFF, 0, 5, 0, 0, -1 }, // rolling_mean_5_speed_change_Temperature
  { FEAT_ROLL_DIFF, 2, 5, 0, 0, -1 }, // rolling_std_5_speed_change_Temperature
  { FEAT_ROLL_DIFF, 0, 5, 0, 1, -1 }, // rolling_mean_5_speed_change_Humidity
  { FEAT_ROLL_DIFF, 2, 5, 0, 1, -1 }, // rolling_std_5_speed_change_Humidity
  { FEAT_ROLL_DIFF, 0, 5, 0, 2, -1 }, // rolling_mean_5_speed_change_Temperature_WeatherStation
  { FEAT_ROLL_DIFF, 2, 5, 0, 2, -1 }, // rolling_std_5_speed_change_Temperature_WeatherStation
  { FEAT_ROLL_DIFF, 0, 5, 0, 3, -1 }, // rolling_mean_5_speed_change_Humidity_WeatherStation
  { FEAT_ROLL_DIFF, 2, 5, 0, 3, -1 }, // rolling_std_5_speed_change_Humidity_WeatherStation
  { FEAT_ROLL_DIFF, 0, 15, 0, 0, -1 }, // rolling_mean_15_speed_change_Temperature
  { FEAT_ROLL_DIFF, 2, 15, 0, 0, -1 }, // rolling_std_15_speed_change_Temperature
  { FEAT_ROLL_DIFF, 0, 15, 0, 1, -1 }, // rolling_mean_15_speed_change_Humidity
  { FEAT_ROLL_DIFF, 2, 15, 0, 1, -1 }, // rolling_std_15_speed_change_Humidity
  { FEAT_ROLL_DIFF, 0, 15, 0, 2, -1 }, // rolling_mean_15_speed_change_Temperature_WeatherStation
  { FEAT_ROLL_DIFF, 2, 15, 0, 2, -1 }, // rolling_std_15_speed_change_Temperature_WeatherStation
  { FEAT_ROLL_DIFF, 0, 15, 0, 3, -1 }, // rolling_mean_15_speed_change_Humidity_WeatherStation
  { FEAT_ROLL_DIFF, 2, 15, 0, 3, -1 }, // rolling_std_15_speed_change_Humidity_WeatherStation
  { FEAT_LAG, -1, 0, 1, 0, -1 }, // lag_1_Temperature
  { FEAT_LAG, -1, 0, 1, 1, -1 }, // lag_1_Humidity
  { FEAT_LAG, -1, 0, 1, 2, -1 }, // lag_1_Temperature_WeatherStation
  { FEAT_LAG, -1, 0, 1, 3, -1 }, // lag_1_Humidity_WeatherStation
  { FEAT_LAG, -1, 0, 2, 0, -1 }, // lag_2_Temperature
  { FEAT_LAG, -1, 0, 2, 1, -1 }, // lag_2_Humidity
  { FEAT_LAG, -1, 0, 2, 2, -1 }, // lag_2_Temperature_WeatherStation
  { FEAT_LAG, -1, 0, 2, 3, -1 }, // lag_2_Humidity_WeatherStation
  { FEAT_LAG, -1, 0, 3, 0, -1 }, // lag_3_Temperature
  { FEAT_LAG, -1, 0, 3, 1, -1 }, // lag_3_Humidity
  { FEAT_LAG, -1, 0, 3, 2, -1 }, // lag_3_Temperature_WeatherStation
  { FEAT_LAG, -1, 0, 3, 3, -1 }, // lag_3_Humidity_WeatherStation
  { FEAT_EWMA, -1, 5, 0, 0, -1 }, // ewma_5_Temperature
  { FEAT_EWMA, -1, 5, 0, 1, -1 }, // ewma_5_Humidity
  { FEAT_EWMA, -1, 5, 0, 2, -1 }, // ewma_5_Temperature_WeatherStation
  { FEAT_EWMA, -1, 5, 0, 3, -1 }, // ewma_5_Humidity_WeatherStation
  { FEAT_INTER, -1, 0, 0, 0, 1 }, // inter_Temperature_x_Humidity
  { FEAT_INTER, -1, 0, 0, 0, 2 }, // inter_Temperature_x_Temperature_WeatherStation
  { FEAT_INTER, -1, 0, 0, 0, 3 }, // inter_Temperature_x_Humidity_WeatherStation
  { FEAT_INTER, -1, 0, 0, 1, 2 }, // inter_Humidity_x_Temperature_WeatherStation
  { FEAT_INTER, -1, 0, 0, 1, 3 }, // inter_Humidity_x_Humidity_WeatherStation
  { FEAT_INTER, -1, 0, 0, 2, 3 }, // inter_Temperature_WeatherStation_x_Humidity_WeatherStation
};
