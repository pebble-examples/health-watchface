#include <pebble.h>

#define DISP_ROWS PBL_IF_RECT_ELSE(144, 180)
#define DISP_COLS PBL_IF_RECT_ELSE(168, 180)
#define MAX(a, b) ((a) > (b) ? a : b)
#define MIN(a, b) ((a) < (b) ? a : b)
#define XDIV(a, b) (1000 * a / b)
#define DIVX(a) (a / 1000)
#define RECT_PERIMETER ((DISP_ROWS + DISP_COLS) * 2)

#define PAST_DAYS_CONSIDERED 7

static Window *s_main_window;
static Layer *s_canvas_layer, *s_text_layer;
static GBitmap *s_blue_shoe, *s_green_shoe;
static GFont s_font_small, s_font_big, s_font_med;

static char s_current_steps_buffer[] = "99,999";
static char s_current_time_buffer[8];
static uint32_t *s_curr_day_id;
static int32_t s_current_steps;
static uint16_t s_daily_average;
static uint16_t s_current_average;

static const uint32_t TOP_RIGHT = 72;
static const uint32_t BOT_RIGHT = 240;
static const uint32_t BOT_LEFT = 384;
static const uint32_t TOP_LEFT = 552;


/****************************** Render Functions ******************************/

static GPoint prv_steps_to_point(uint32_t current_steps, uint16_t day_average_steps, GRect frame) {
#if defined(PBL_RECT)
    /* e       a       b
     *   -------------
     *   |           |
     *   |           |
     *   |           |
     *   |           |
     *   |           |
     *   |           |
     *   |           |
     *   -------------
     * d               c
     */

     // Limits calculated from length along perimeter starting from 'a'
     const uint32_t limit_b = day_average_steps * TOP_RIGHT / RECT_PERIMETER;
     const uint32_t limit_c = day_average_steps * BOT_RIGHT / RECT_PERIMETER; 
     const uint32_t limit_d = day_average_steps * BOT_LEFT / RECT_PERIMETER;
     const uint32_t limit_e = day_average_steps * TOP_LEFT / RECT_PERIMETER;

    if (current_steps <= limit_b) {
      // Zone a <-> b
      return GPoint(frame.origin.x + DIVX(frame.size.w * (500 + (500 * current_steps / limit_b))), 
                    frame.origin.y);
    } else if (current_steps <= limit_c) {
      // Zone b <-> c
      return GPoint(frame.origin.x + frame.size.w,
                    frame.origin.y + DIVX(frame.size.h * XDIV((current_steps - limit_b), (limit_c - limit_b))));
    } else if (current_steps <= limit_d) {
      // Zone c <-> d
      return GPoint(frame.origin.x + DIVX(frame.size.w * (1000 - XDIV((current_steps - limit_c), (limit_d - limit_c)))),
                    frame.origin.y + frame.size.h);
    } else if (current_steps <= limit_e) {
      // Zone d <-> e
      return GPoint(frame.origin.x,
                    frame.origin.y + DIVX(frame.size.h * (1000 - XDIV((current_steps - limit_d), (limit_e - limit_d)))));
    } else {
      // Zone e <-> 0
      return GPoint(frame.origin.x + DIVX(frame.size.w / 2 * XDIV((current_steps - limit_e), (day_average_steps - limit_e))),
                    frame.origin.y);
    }
#elif defined(PBL_ROUND)
    return gpoint_from_polar(frame, GOvalScaleModeFitCircle,
                             DEG_TO_TRIGANGLE(DIVX(360 * XDIV(current_steps, day_average_steps))));
#endif
}

#if defined(PBL_RECT)
static GPoint prv_inset_point(GPoint outer_point, int inset_amount) {
  // Insets the given point by the specified amount
  return (GPoint) {
    .x = MAX(inset_amount - 1, MIN(outer_point.x, DISP_ROWS - inset_amount)),
    .y = MAX(inset_amount - 1, MIN(outer_point.y, DISP_COLS - inset_amount))
  };
}
#endif

static void prv_fill_outer_ring(GContext *ctx, int32_t current_steps, int32_t day_average_steps,
                                int fill_thickness, GRect frame, GColor color) {
  graphics_context_set_fill_color(ctx, color);
#if defined(PBL_RECT)
    GRect outer_bounds = frame;

    GPoint start_outer_point = prv_steps_to_point(0, day_average_steps, outer_bounds);
    GPoint start_inner_point = prv_inset_point(start_outer_point, fill_thickness);
    GPoint end_outer_point = prv_steps_to_point(current_steps, day_average_steps, outer_bounds);
    GPoint end_inner_point = prv_inset_point(end_outer_point, fill_thickness);

    GPath path = (GPath) {
      .points = (GPoint *) malloc(sizeof(GPoint) * 20), // TODO: change to actual max points needed
      .num_points = 0
    };

    const int32_t corners[6] = {0,
                          day_average_steps * TOP_RIGHT / RECT_PERIMETER,
                          day_average_steps * BOT_RIGHT / RECT_PERIMETER,
                          day_average_steps * BOT_LEFT / RECT_PERIMETER,
                          day_average_steps * TOP_LEFT / RECT_PERIMETER,
                          day_average_steps};

    // start the path with start_outer_point
    path.points[path.num_points++] = start_outer_point;
    // loop through and add all the corners b/w start and end
    for (uint16_t i = 0; i < ARRAY_LENGTH(corners); i++) {
      if (corners[i] > 0 && corners[i] < current_steps) {
        path.points[path.num_points++] = prv_steps_to_point(corners[i], day_average_steps, 
                                                            outer_bounds);
      }
    }
    // add end_outer_point
    path.points[path.num_points++] = end_outer_point;
    // add end_inner_point
    path.points[path.num_points++] = end_inner_point;
    // loop though backwards and add all the corners b/w end and start
    for (int i = ARRAY_LENGTH(corners) - 1; i >= 0; i--) {
      if (corners[i] > 0 && corners[i] < current_steps) {
        path.points[path.num_points++] = prv_inset_point(prv_steps_to_point(corners[i], 
                                                                            day_average_steps, 
                                                                            outer_bounds),
                                                         fill_thickness);
      }
    }
    // add start_inner_point
    path.points[path.num_points++] = start_inner_point;

    gpath_draw_filled(ctx, &path);

    graphics_context_set_stroke_color(ctx, color);
    gpath_draw_outline(ctx, &path);

    free(path.points);
#elif defined(PBL_ROUND)
    graphics_fill_radial(ctx, frame, GOvalScaleModeFitCircle, fill_thickness,
                         DEG_TO_TRIGANGLE(0),
                         DEG_TO_TRIGANGLE(360 * current_steps / day_average_steps));
#endif
}

static void prv_fill_goal_line(GContext *ctx, int32_t current_average, int32_t day_average_steps,
                                int line_length, int line_width, GRect frame, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  GPoint line_outer_point = prv_steps_to_point(current_average, day_average_steps, frame);

#if defined(PBL_RECT)
    GPoint line_inner_point = prv_inset_point(line_outer_point, line_length);
#elif defined(PBL_ROUND)
    GRect inner_bounds = grect_inset(frame, GEdgeInsets(line_length));
    GPoint line_inner_point = prv_steps_to_point(current_average, day_average_steps, inner_bounds);
#endif

  graphics_context_set_stroke_width(ctx, line_width);
  graphics_draw_line(ctx, line_inner_point, line_outer_point);
}

static void draw_steps_value(GRect bounds, GContext *ctx, GColor color, GBitmap *bitmap) {
  GRect steps_text_box = bounds;
  GRect shoe_bitmap_box = bounds;

  shoe_bitmap_box.size = gbitmap_get_bounds(s_green_shoe).size;

  int16_t text_width = graphics_text_layout_get_content_size(s_current_steps_buffer, 
                                                              s_font_small, 
                                                              steps_text_box, 
                                                              GTextOverflowModeTrailingEllipsis, 
                                                              GTextAlignmentCenter).w;
  const int16_t FONT_HEIGHT = 14;
  steps_text_box.size = GSize(text_width, FONT_HEIGHT);

  const uint16_t PADDING = 5;
  uint16_t combined_width = shoe_bitmap_box.size.w + PADDING + text_width;

  steps_text_box.origin.x = (bounds.size.w / 2) - (combined_width / 2);
  shoe_bitmap_box.origin.x = (bounds.size.w / 2) + (combined_width / 2) - shoe_bitmap_box.size.w;

  steps_text_box.origin.y = PBL_IF_RECT_ELSE(56, 60);
  shoe_bitmap_box.origin.y = PBL_IF_RECT_ELSE(60, 65);

  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, s_current_steps_buffer, s_font_small, 
      steps_text_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  graphics_draw_bitmap_in_rect(ctx, bitmap, shoe_bitmap_box);
}

static void draw_outer_dots(GRect bounds, GContext *ctx) {
  GRect frame = grect_inset(bounds, GEdgeInsets(6));
#if defined(PBL_RECT)
    const uint16_t quarter_perimeter = RECT_PERIMETER / 4;
    // Puts middle dot on each side of screen
    for (int i = quarter_perimeter; i <= RECT_PERIMETER; i += quarter_perimeter) {
      GPoint middle = prv_steps_to_point(i, RECT_PERIMETER, frame);
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      if (i != RECT_PERIMETER) {
        graphics_fill_circle(ctx, middle, 2);
      }
      // Puts two dots around each middle dot
      for (int j = -36; j <= 36; j += 72) {
        GPoint sides = middle;
        if (i == quarter_perimeter || i == (quarter_perimeter * 3)) {
          sides.y = middle.y + j;
        } else {
          sides.x = middle.x + j;
        }
        graphics_fill_circle(ctx, sides, 2);
      }
    }
#elif defined(PBL_ROUND)
    // Outer dots placed along inside circumference
    const int NUM_DOTS = 12;
    for (int i = 1; i < NUM_DOTS; i++) {
      GPoint pos = gpoint_from_polar(frame, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(i * 360 / NUM_DOTS));
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_circle(ctx, pos, 2);
    }
#endif
}


/************************************ UI **************************************/

static void update_steps_buffer() {
  int thousands = s_current_steps / 1000;
  int hundreds = s_current_steps % 1000;
  if (thousands > 0) {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer), "%d,%03d", thousands, hundreds);
  } else {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer), "%d", hundreds);
  }
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *time_now = localtime(&temp);

  int hours = time_now->tm_hour;
  if(!clock_is_24h_style()) {
    hours -= (hours > 12) ? 12 : 0;
  }

  // Write the current hours and minutes into a buffer
  char *fmt_str = (time_now->tm_min < 10) ? "%d:0%d" : "%d:%d";
  snprintf(s_current_time_buffer, sizeof(s_current_time_buffer), 
    fmt_str, hours, time_now->tm_min);

  // Remove 0 from start of time
  if ('0' == s_current_time_buffer[0]) {
    memmove(s_current_time_buffer, &s_current_time_buffer[1], sizeof(s_current_time_buffer)-1);
  }

  layer_mark_dirty(s_text_layer);
}

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

static void update_average(bool daily) {
  // Average of steps taken so far today over the last PAST_DAYS_CONSIDERED days
  int *data = (int*)malloc(PAST_DAYS_CONSIDERED * sizeof(int));

  for(int day = 0; day < PAST_DAYS_CONSIDERED; day++) {
    time_t start = time_start_of_today() - (day * (24 * SECONDS_PER_HOUR));

    time_t end;
    if(daily) {
      end = start + (24 * SECONDS_PER_HOUR);
    } else {
      end = start + (time(NULL) - time_start_of_today());
    } 

    HealthServiceAccessibilityMask mask = health_service_metric_accessible(HealthMetricStepCount, start, end);
    if(mask == HealthServiceAccessibilityMaskAvailable) {
      // Data is available, read day's sum
      data[day] = (int)health_service_sum(HealthMetricStepCount, start, end);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "%d steps for %d days ago", data[day], day);
    } else {
      data[day] = 0;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "No data available for %d days ago", day);
    }
  }

  if(daily) {
    s_daily_average = calculate_average(data, PAST_DAYS_CONSIDERED);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Daily average: %d", s_daily_average);
  } else {
    s_current_average = calculate_average(data, PAST_DAYS_CONSIDERED);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Current average: %d", s_current_average);
  }
  free(data);
}

static void update_steps_averages(struct tm *curr_time) {
  static bool done_init = false;

  // Set up new day's total average steps
  if ((curr_time->tm_hour == 0 && curr_time->tm_min == 0) || !done_init) {
    update_average(true);
    update_average(false);

    if (done_init) {
      s_current_steps = 0;
      update_steps_buffer();
    }
  } else if (curr_time->tm_min % 15 == 0 || !done_init) {
    // Update current average throughout day (15 min steps as per api)
    update_average(false);
  }
  done_init = true;
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  update_time();
  update_steps_averages(tick_time);

  // Redraw
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void activity_steps_handler(HealthEventType event, void *context) {
  s_current_steps = health_service_sum_today(HealthMetricStepCount);
  update_steps_buffer();
}

static void progress_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  const int fill_thickness = PBL_IF_RECT_ELSE(12, (180 - grect_inset(bounds, 
                                                                     GEdgeInsets(12)).size.h) / 2);

  draw_outer_dots(bounds, ctx);

  if (s_current_steps > s_daily_average) {
    s_daily_average = s_current_steps;
  }

  GColor scheme;
  GBitmap *bitmap;
  if (s_current_steps >= (int)s_current_average) {
    scheme  = GColorJaegerGreen;
    bitmap = s_green_shoe;
  } else {
    scheme = GColorPictonBlue;
    bitmap = s_blue_shoe;
  }

  graphics_context_set_antialiased(ctx, true);

  prv_fill_outer_ring(ctx, s_current_steps, s_daily_average, fill_thickness, bounds, scheme);
  prv_fill_goal_line(ctx, s_current_average, s_daily_average, 17, 4, bounds, GColorYellow);

  draw_steps_value(bounds, ctx, scheme, bitmap);
}

static void text_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Get total width
  int total_width = 0;
  GSize time_size = graphics_text_layout_get_content_size(
    s_current_time_buffer, s_font_big, bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft);
  total_width += time_size.w;
  total_width += graphics_text_layout_get_content_size(
    "AM", s_font_med, bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft).w;

  graphics_context_set_text_color(ctx, GColorWhite);
  const int x_margin = (bounds.size.w - total_width) / 2;
  int y_margin = PBL_IF_RECT_ELSE(8, 2);
  const GRect time_rect = grect_inset(bounds, GEdgeInsets(-y_margin, 0, 0, x_margin));
  graphics_draw_text(ctx, s_current_time_buffer, s_font_big, time_rect, 
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  const time_t now = time(NULL);
  const struct tm *time_now = localtime(&now);
  const bool am = time_now->tm_hour < 12;
  const int spacing = 2;
  const GRect period_rect = grect_inset(bounds, 
    GEdgeInsets(PBL_IF_RECT_ELSE(-2, 4), 0, 0, time_size.w + x_margin + spacing));
  graphics_draw_text(ctx, am ? "AM" : "PM", s_font_med, period_rect, 
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_green_shoe = gbitmap_create_with_resource(RESOURCE_ID_GREEN_SHOE_LOGO);
  s_blue_shoe = gbitmap_create_with_resource(RESOURCE_ID_BLUE_SHOE_LOGO);

  s_font_small = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_font_med = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#if defined(PBL_RECT)
  s_font_big = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
#else
  s_font_big = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
#endif

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, progress_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  const GEdgeInsets time_insets = GEdgeInsets(80, 0, 0, 0);
  s_text_layer = layer_create(grect_inset(window_bounds, time_insets));
  layer_set_update_proc(s_text_layer, text_update_proc);
  layer_add_child(window_layer, s_text_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  layer_destroy(s_text_layer);
  gbitmap_destroy(s_green_shoe);
  gbitmap_destroy(s_blue_shoe);
}

/*********************************** App **************************************/


static void init() {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);
  window_set_background_color(s_main_window, GColorBlack);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  update_time();

  health_service_events_subscribe(activity_steps_handler, NULL);

  // initialize current step values
  s_current_steps = health_service_sum_today(HealthMetricStepCount);
  update_steps_buffer();

  // initialize average steps values
  const time_t now = time(NULL);
  struct tm *time_now = localtime(&now);
  update_steps_averages(time_now);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}