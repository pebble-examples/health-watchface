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

static GBitmap *s_blue_shoe, *s_green_shoe;
static GFont s_font_small, s_font_big, s_font_med;

static int s_current_steps, s_daily_average, s_current_average;
static char s_current_steps_buffer[8];

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
        if(DEBUG) APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown average type!");
        break;
    } 

    // Gather the data items for the last PAST_DAYS_CONSIDERED days
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(HealthMetricStepCount, start, end);
    if(mask & HealthServiceAccessibilityMaskAvailable) {
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

void data_update_steps_buffer() {
  int thousands = s_current_steps / 1000;
  int hundreds = s_current_steps % 1000;
  if(thousands > 0) {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer), "%d,%03d", thousands, hundreds);
  } else {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer), "%d", hundreds);
  }

  main_window_redraw();
}

static void load_health_data_handler(void *context) {
  const struct tm *time_now = util_get_tm();

  s_current_steps = health_service_sum_today(HealthMetricStepCount);
  persist_write_int(AppKeyCurrentSteps, s_current_steps);

  update_average(AverageTypeDaily);
  update_average(AverageTypeCurrent);

  data_update_steps_buffer();
}

void data_reload_averages() {
  app_timer_register(LOAD_DATA_DELAY, load_health_data_handler, NULL);
}

void data_init() {
  // Load resources
  s_green_shoe = gbitmap_create_with_resource(RESOURCE_ID_GREEN_SHOE_LOGO);
  s_blue_shoe = gbitmap_create_with_resource(RESOURCE_ID_BLUE_SHOE_LOGO);
  s_font_small = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_font_med = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  s_font_big = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);

  // First time persist
  if(!persist_exists(AppKeyCurrentSteps)) {
    s_current_steps = 0;
    s_current_average = 0;
    s_daily_average = 0;
  } else {
    s_current_average = persist_read_int(AppKeyCurrentAverage);
    s_daily_average = persist_read_int(AppKeyDailyAverage);
    s_current_steps = persist_read_int(AppKeyCurrentSteps);
  }
  data_update_steps_buffer();

  // Avoid half-second delay loading the app by delaying API read
  data_reload_averages();
}

void data_deinit() {
  gbitmap_destroy(s_green_shoe);
  gbitmap_destroy(s_blue_shoe);
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

GFont data_get_font(FontSize size) {
  switch(size) {
    case FontSizeSmall:  return s_font_small;
    case FontSizeMedium: return s_font_med;
    case FontSizeLarge:  return s_font_big;
    default: return s_font_small;
  }
}

GBitmap* data_get_blue_shoe() {
  return s_blue_shoe;
}

GBitmap* data_get_green_shoe() {
  return s_green_shoe;
}

char* data_get_current_steps_buffer() {
  return s_current_steps_buffer;
}
