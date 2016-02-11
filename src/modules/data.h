#pragma once

#include <pebble.h>

#include "../config.h"

#include "../modules/util.h"

#include "../windows/main_window.h"

void data_init();

int data_get_current_steps();
void data_set_current_steps(int value);

int data_get_current_average();
void data_set_current_average(int value);

int data_get_daily_average();
void data_set_daily_average(int value);

void data_update_averages();
