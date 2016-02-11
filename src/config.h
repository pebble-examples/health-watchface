#pragma once

#include <pebble.h>

// Used to turn off logging
#define DEBUG false

// Number of days the averaging mechanism takes into account
#define PAST_DAYS_CONSIDERED 7

// Delay after launch before querying the Health API
#define LOAD_DATA_DELAY 500
