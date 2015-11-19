#include <pebble.h>

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;
static Time s_curr_time;
static TextLayer *s_time_layer;

static uint32_t current_average;
static uint32_t daily_average;
static uint32_t current_steps;


/****************************** Render Functions ******************************/

static GPoint prv_steps_to_point(uint32_t current_steps, uint16_t day_average_steps, GRect frame) {
  #if defined(PBL_RECT)
    /* e       0       b
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

    if (current_steps <= (day_average_steps * 1.5 / 12)) {
      // Zone o <-> b
      return GPoint(frame.origin.x + frame.size.w * 
                        (0.5 + 0.5 * (current_steps / (day_average_steps * 1.5 / 12))), 
                    frame.origin.y);
    } else if (current_steps <= (day_average_steps * 4.5 / 12)) {
      // Zone b <-> c
      return GPoint(frame.origin.x + frame.size.w,
                    frame.origin.y + frame.size.h *
                    ((current_steps - (day_average_steps * 1.5 / 12)) / (day_average_steps / 4)));
    } else if (current_steps <= (day_average_steps * 7.5 / 12)) {
      // Zone c <-> d
      return GPoint(frame.origin.x + frame.size.w * 
                        (1 -  ((current_steps - (day_average_steps * 4.5 / 12)) / 
                        (day_average_steps / 4))),
                    frame.origin.y + frame.size.h);
    } else if (current_steps <= (day_average_steps * 10.5 / 12)) {
      // Zone d <-> e
      return GPoint(frame.origin.x,
                    frame.origin.y + frame.size.h * 
                        (1 - ((current_steps - (day_average_steps * 7.5 / 12)) / 
                        (day_average_steps / 4))));
    } else {
      // Zone e <-> 0
      return GPoint(frame.origin.x + frame.size.w * 
                        (0.5 *  ((current_steps - (day_average_steps * 10.5 / 12)) / 
                        (day_average_steps / 8))),
                    frame.origin.y);
    }
  #elif defined(PBL_ROUND)
    return gpoint_from_polar(frame, GOvalScaleModeFitCircle,
                             DEG_TO_TRIGANGLE(360 * current_steps / day_average_steps));
  #endif
}

#if defined(PBL_RECT)
static GPoint prv_inset_point(GPoint outer_point, int inset_amount) {
  // Insets the given point by the specified amount
  return (GPoint) {
    .x = MAX(inset_amount - 1, MIN(outer_point.x, 144 - inset_amount)),
    .y = MAX(inset_amount - 1, MIN(outer_point.y, 168 - inset_amount))
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
      .points = (GPoint *) malloc(sizeof(GPoint) * 20), // change to actual max points needed
      .num_points = 0
    };

    int32_t corners[6] = {0,
                          day_average_steps * 1.5 / 12,
                          day_average_steps * 4.5 / 12,
                          day_average_steps * 7.5 / 12,
                          day_average_steps * 10.5 / 12,
                          day_average_steps};

    // start the path with start_outer_point
    path.points[path.num_points++] = start_outer_point;
    // loop through and add all the corners b/w start and end
    for (int i = 0; i < (int) ARRAY_LENGTH(corners); i++) {
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
    graphics_context_set_stroke_color(ctx, GColorBlack);

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


/************************************ UI **************************************/

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M%p" : "%I:%M%p", tick_time);

  // Remove 0 from start of time
  if('0' == s_buffer[0]) {
    memmove(s_buffer, &s_buffer[1], sizeof(s_buffer)-1);
  }

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_curr_time.hours = tick_time->tm_hour;
  s_curr_time.hours -= (s_curr_time.hours > 12) ? 12 : 0;
  s_curr_time.minutes = tick_time->tm_min;

  update_time();

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void update_proc(Layer *layer, GContext *ctx) {
  // For Text Purposes
  current_average += 8;
  current_steps += 10;
  daily_average = 1000;

  if (current_average > daily_average)
    current_average = 0;

  if (current_steps > daily_average)
    current_steps = 0;
  // -----------------

  GRect bounds = layer_get_bounds(layer);

  // fill the radial
  #if defined(PBL_RECT)
    const int fill_thickness = 12;
  #elif defined(PBL_ROUND)
    const int fill_thickness = (180 - grect_inset(bounds, GEdgeInsets(12)).size.h) / 2;
  #endif

  prv_fill_outer_ring(ctx, current_steps, daily_average, fill_thickness, bounds, GColorIslamicGreen);
  prv_fill_goal_line(ctx, current_average, daily_average, 17, 4, bounds, GColorYellow);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(70, 70), window_bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  text_layer_destroy(s_time_layer);
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

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  update_time();
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}