#include <pebble.h>

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)
#define XDIV(a,b) (1000 * a / b)
#define DIVX(a) (a / 1000)
#define RECT_PERIMETER 624 // 144 * 2 + 168 * 2

static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_time_layer;
static GBitmap *s_blue_shoe, *s_green_shoe;
static GFont s_zonapro_font_19, s_zonapro_font_27, s_zonapro_font_30;

static char s_current_steps_buffer[] = "99,999";
static uint32_t s_current_average;
static uint32_t s_daily_average;
static uint32_t s_current_steps;


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
     const uint32_t limit_b = day_average_steps * 72 / RECT_PERIMETER;
     const uint32_t limit_c = day_average_steps * 240 / RECT_PERIMETER; 
     const uint32_t limit_d = day_average_steps * 384 / RECT_PERIMETER;
     const uint32_t limit_e = day_average_steps * 552 / RECT_PERIMETER;

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
      return GPoint(frame.origin.x + DIVX(frame.size.w / 2 * XDIV((current_steps - limit_e), (RECT_PERIMETER - limit_e))),
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
      .points = (GPoint *) malloc(sizeof(GPoint) * 20), // TODO: change to actual max points needed
      .num_points = 0
    };

    const int32_t corners[6] = {0,
                          day_average_steps * 72 / RECT_PERIMETER,
                          day_average_steps * 240 / RECT_PERIMETER,
                          day_average_steps * 384 / RECT_PERIMETER,
                          day_average_steps * 552 / RECT_PERIMETER,
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

static void draw_steps_value(GRect bounds, GContext *ctx, GColor color, GBitmap *bitmap) {
  GRect steps_text_box = bounds;
  GRect shoe_bitmap_box = bounds;

  shoe_bitmap_box.size = GSize(29, 15);

  int16_t text_width = graphics_text_layout_get_content_size(s_current_steps_buffer, 
                                                              s_zonapro_font_19, 
                                                              steps_text_box, 
                                                              GTextOverflowModeTrailingEllipsis, 
                                                              GTextAlignmentCenter).w;
  steps_text_box.size = GSize(text_width, 14);

  uint16_t combined_width = shoe_bitmap_box.size.w + 5 + text_width;

  steps_text_box.origin.x = (bounds.size.w / 2) - (combined_width / 2);
  shoe_bitmap_box.origin.x = (bounds.size.w / 2) + (combined_width / 2) - shoe_bitmap_box.size.w;

  steps_text_box.origin.y = PBL_IF_RECT_ELSE(56, 60);
  shoe_bitmap_box.origin.y = PBL_IF_RECT_ELSE(60, 65);

  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, s_current_steps_buffer, s_zonapro_font_19, 
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
    // Hours are dots
    for(int i = 1; i < 12; i++) {
      GPoint pos = gpoint_from_polar(frame, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(i * 360 / 12));
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_circle(ctx, pos, 2);
    }
  #endif
}


/************************************ UI **************************************/

static void update_steps(int32_t current_steps) {
  int thousands = current_steps / 1000;
  int hundreds = current_steps % 1000;
  if (thousands > 0) {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer), "%d,%03d", thousands, hundreds);
  } else {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer), "%d", hundreds);
  }
}

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
  update_time();

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void update_proc(Layer *layer, GContext *ctx) {
  update_steps(s_current_steps);

  // For Text Purposes
  s_current_average += 100;
  s_current_steps += 80 + (s_current_steps * 0.01F);
  s_daily_average = 20000;

  if (s_current_average > s_daily_average)
    s_current_average = 0;

  if (s_current_steps > s_daily_average)
    s_current_steps = 0;
  // -----------------

  GRect bounds = layer_get_bounds(layer);

  const int fill_thickness = PBL_IF_RECT_ELSE(12, (180 - grect_inset(bounds, 
                                                                     GEdgeInsets(12)).size.h) / 2);

  draw_outer_dots(bounds, ctx);

  GColor scheme;
  GBitmap *bitmap;
  if (s_current_steps >= s_current_average) {
    scheme  = GColorIslamicGreen;
    bitmap = s_green_shoe;
  } else {
    scheme = GColorPictonBlue;
    bitmap = s_blue_shoe;
  }

  APP_LOG(APP_LOG_LEVEL_WARNING, "STARTED CALL");
  prv_fill_outer_ring(ctx, s_current_steps, s_daily_average, fill_thickness, bounds, scheme);
  prv_fill_goal_line(ctx, s_current_average, s_daily_average, 17, 4, bounds, GColorYellow);

  draw_steps_value(bounds, ctx, scheme, bitmap);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_green_shoe = gbitmap_create_with_resource(RESOURCE_ID_GREEN_SHOE_LOGO);
  s_blue_shoe = gbitmap_create_with_resource(RESOURCE_ID_BLUE_SHOE_LOGO);

  s_zonapro_font_19 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ZONAPRO_BOLD_FONT_19));
  s_zonapro_font_27 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ZONAPRO_BOLD_FONT_27));
  s_zonapro_font_30 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ZONAPRO_BOLD_FONT_30));

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_RECT_ELSE(74, 80), window_bounds.size.w, 30));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, PBL_IF_RECT_ELSE(s_zonapro_font_27, s_zonapro_font_30));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  text_layer_destroy(s_time_layer);
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