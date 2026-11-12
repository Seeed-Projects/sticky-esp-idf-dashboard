#pragma once

#include "esp_err.h"

// Powers and initializes GT911, then starts the lightweight polling task.
esp_err_t sticky_touch_init();

// Pauses polling and waits for acknowledgement before deep sleep. The task is
// retained; sticky_touch_init() resumes it if sleep preparation fails.
esp_err_t sticky_touch_stop();
