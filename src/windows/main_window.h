#pragma once

#include <pebble.h>

#include "../modules/data.h"
#include "../modules/graphics.h"
#include "../modules/util.h"

void main_window_push();

void main_window_update_time(struct tm* tick_time);

void main_window_redraw();
