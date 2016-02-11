#include "util.h"

struct tm* util_get_tm() {
  time_t temp = time(NULL); 
  return localtime(&temp);
}
