#include "main_window.h"

static Window *s_window;
static Layer *s_canvas_layer, *s_text_layer;

static char s_current_time_buffer[8];

static void progress_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int fill_thickness = PBL_IF_RECT_ELSE(12, (180 - grect_inset(bounds, GEdgeInsets(12)).size.h) / 2);
  int current_steps = data_get_current_steps();
  int daily_average = data_get_daily_average();
  int current_average = data_get_current_average();

  // Set new exceeded daily average
  if(current_steps > daily_average) {
    daily_average = current_steps;
    data_set_daily_average(daily_average);
  }

  // Decide color scheme based on progress to/past goal
  GColor scheme_color;
  GBitmap *bitmap;
  if(current_steps >= current_average) {
    scheme_color  = GColorJaegerGreen;
    bitmap = data_get_green_shoe();
  } else {
    scheme_color = GColorPictonBlue;
    bitmap = data_get_blue_shoe();
  }

  // Perform drawing
  graphics_draw_outer_dots(ctx, bounds);
  graphics_fill_outer_ring(ctx, current_steps, fill_thickness, bounds, scheme_color);
  graphics_fill_goal_line(ctx, daily_average, 17, 4, bounds, GColorYellow);
  graphics_draw_steps_value(ctx, bounds, scheme_color, bitmap);
}

static void text_update_proc(Layer *layer, GContext *ctx) {
  const GRect layer_bounds = layer_get_bounds(layer);

  const GFont font_med = data_get_font(FontSizeMedium);
  const GFont font_large = data_get_font(FontSizeLarge);

  // Get total width
  int total_width = 0;
  GSize time_size = graphics_text_layout_get_content_size(
    s_current_time_buffer, font_large, layer_bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft);
  total_width += time_size.w;
  if(!clock_is_24h_style()) {
    total_width += graphics_text_layout_get_content_size(
      "AM", font_med, layer_bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
  }

  const int x_margin = (layer_bounds.size.w - total_width) / 2;
  const int y_margin = PBL_IF_RECT_ELSE(8, 2);
  const GRect time_rect = grect_inset(layer_bounds, GEdgeInsets(-y_margin, 0, 0, x_margin));
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_current_time_buffer, font_large, time_rect, 
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  if(!clock_is_24h_style()) {
    // 12 hour mode
    const struct tm *time_now = util_get_tm();
    const bool am = time_now->tm_hour < 12;
    const int spacing = 2;

    const GRect period_rect = grect_inset(layer_bounds, 
      GEdgeInsets(PBL_IF_RECT_ELSE(-2, 4), 0, 0, time_size.w + x_margin + spacing));
    graphics_draw_text(ctx, am ? "AM" : "PM", font_med, period_rect, 
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  }
}

/*********************************** Window ***********************************/

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, progress_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  GEdgeInsets time_insets = GEdgeInsets(80, 0, 0, 0);
  s_text_layer = layer_create(grect_inset(window_bounds, time_insets));
  layer_set_update_proc(s_text_layer, text_update_proc);
  layer_add_child(window_layer, s_text_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  layer_destroy(s_text_layer);

  window_destroy(s_window);
}

void main_window_push() {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  graphics_set_window(s_window);
}

void main_window_update_time(struct tm* tick_time) {
  strftime(s_current_time_buffer, sizeof(s_current_time_buffer),
    clock_is_24h_style() ? "%H:%M" : "%l:%M", tick_time);
  layer_mark_dirty(s_text_layer);
}

void main_window_redraw() {
  if(s_canvas_layer && s_text_layer) {
    layer_mark_dirty(s_canvas_layer);
    layer_mark_dirty(s_text_layer);
  }
}
