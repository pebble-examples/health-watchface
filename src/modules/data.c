#include "data.h"

typedef enum {
  AppKeyCurrentAverage = 0,
  AppKeyDailyAverage,
  AppKeyCurrentSteps
} AppKey;

typedef enum {
  AverageTypeCurrent = 0,
  AverageTypeDaily
} AverageType;

static int s_current_steps, s_daily_average, s_current_average;

static int calculate_average(int *data, int num_items) {
  int result = 0;
  int num_valid_items = 0;

  for(int i = 0; i < num_items; i++) {
    if(data[i] > 0) {
      result += data[i];
      num_valid_items++;
    }
  }

  return result / num_valid_items;
}

static void update_average(AverageType type) {
  int data[PAST_DAYS_CONSIDERED];

  for(int day = 0; day < PAST_DAYS_CONSIDERED; day++) {
    // Start time is midnight, minus the number of seconds per day for each day
    const time_t start = time_start_of_today() - (day * (24 * SECONDS_PER_HOUR));

    time_t end = start;
    switch(type) {
      case AverageTypeDaily:
        end = start + (24 * SECONDS_PER_HOUR);
        break;
      case AverageTypeCurrent:
        end = start + (time(NULL) - time_start_of_today());
        break;
      default:
        APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown average type!");
        break;
    } 

    // Gather the data items for the last PAST_DAYS_CONSIDERED days
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(HealthMetricStepCount, start, end);
    if(mask == HealthServiceAccessibilityMaskAvailable) {
      // Data is available, read day's sum
      data[day] = (int)health_service_sum(HealthMetricStepCount, start, end);
      if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "%d steps for %d days ago", data[day], day);
    } else {
      data[day] = 0;
      if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "No data available for %d days ago", day);
    }
  }

  // Store the calculated value
  switch(type) {
    case AverageTypeDaily:
      s_daily_average = calculate_average(data, PAST_DAYS_CONSIDERED);
      persist_write_int(AppKeyDailyAverage, s_daily_average);

      if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Daily average: %d", s_daily_average);
      break;
    case AverageTypeCurrent:
      s_current_average = calculate_average(data, PAST_DAYS_CONSIDERED);
      persist_write_int(AppKeyCurrentAverage, s_current_average);

      if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Current average: %d", s_current_average);
      break;
    default: break;  // Handled by previous switch
  }
}

void data_update_averages() {
  const struct tm *time_now = util_get_tm();

  s_current_steps = health_service_sum_today(HealthMetricStepCount);
  persist_write_int(AppKeyCurrentSteps, s_current_steps);

  // Set up new day's total average steps
  if(time_now->tm_hour == 0 && time_now->tm_min == 0) {
    update_average(AverageTypeCurrent);
    update_average(AverageTypeDaily);
  } else if (time_now->tm_min % 15 == 0) {
    // Update current average throughout day
    update_average(AverageTypeCurrent);
  }

  main_window_update_steps_buffer();
}

static void load_health_data_handler(void *context) {
  data_update_averages();
  main_window_update_steps_buffer();
}

void data_init() {
  // First time persist
  if(!persist_exists(AppKeyCurrentSteps)) {
    s_current_steps = 0;
    s_current_average = 0;
    s_daily_average = 0;

    main_window_redraw();
  }

  s_current_average = persist_read_int(AppKeyCurrentAverage);
  s_daily_average = persist_read_int(AppKeyDailyAverage);
  s_current_steps = persist_read_int(AppKeyCurrentSteps);
  main_window_update_steps_buffer();

  // Avoid half-second delay loading the app by delaying API read
  app_timer_register(LOAD_DATA_DELAY, load_health_data_handler, NULL);
}

int data_get_current_steps() {
  return s_current_steps;
}

int data_get_current_average() {
  return s_current_average;
}

int data_get_daily_average() {
  return s_daily_average;
}

void data_set_current_steps(int value) {
  s_current_steps = value;
}

void data_set_current_average(int value) {
  s_current_average = value;
}

void data_set_daily_average(int value) {
  s_daily_average = value;
}
