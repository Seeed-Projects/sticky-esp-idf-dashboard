#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

enum class AppEvent {
    PreviousPage,
    NextPage,
    RefreshPage,
    OrientationChanged,
    ClockMinuteTick,
    EnterDeepSleep,
};

// Small shared queue used by physical buttons and touch input.
esp_err_t app_event_init();
bool app_event_post(AppEvent event);
// Repeated navigation is coalesced until app_event_complete(). IMU/clock
// events are coalesced while queued; deep-sleep requests take priority.
bool app_event_wait(AppEvent &event, TickType_t timeout);
void app_event_cancel_sleep();
// Release the navigation gate after the accepted page change finishes.
void app_event_complete(AppEvent event);
