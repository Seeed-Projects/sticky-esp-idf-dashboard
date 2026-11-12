#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

struct RtcDateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

// Attaches the PCF8563 device to the shared sensor I2C bus.
esp_err_t sticky_rtc_init(i2c_master_bus_handle_t bus);

// Reads and validates the current RTC date and time without changing it.
esp_err_t sticky_rtc_read(RtcDateTime &date_time);

// Startup only: preserve valid time; seed invalid clock data with the documented
// demo epoch (2026-01-01 00:00:00). This is not real-time synchronization.
// An absent device or I2C error is returned without attempting a write.
esp_err_t sticky_rtc_initialize_time();
