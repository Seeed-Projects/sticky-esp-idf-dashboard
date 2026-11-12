#include "app_event.h"

#include "freertos/queue.h"

namespace {

constexpr UBaseType_t kQueueLength = 8;
QueueHandle_t s_event_queue = nullptr;
portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
bool s_sleep_pending = false;
bool s_orientation_pending = false;
bool s_clock_pending = false;
bool s_navigation_pending = false;

}  // namespace

esp_err_t app_event_init()
{
    if (s_event_queue != nullptr) {
        return ESP_OK;
    }

    s_event_queue = xQueueCreate(kQueueLength, sizeof(AppEvent));
    return s_event_queue != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
}

bool app_event_post(AppEvent event)
{
    if (s_event_queue == nullptr) {
        return false;
    }
    taskENTER_CRITICAL(&s_lock);
    bool *pending = event == AppEvent::OrientationChanged ? &s_orientation_pending
                    : event == AppEvent::ClockMinuteTick ? &s_clock_pending
                    : (event == AppEvent::NextPage || event == AppEvent::PreviousPage)
                        ? &s_navigation_pending : nullptr;
    bool posted = true;
    if (event == AppEvent::EnterDeepSleep) {
        // Retain the request even if the queue is full. Any queued event wakes
        // the consumer, which checks this flag before processing that event.
        s_sleep_pending = true;
        xQueueSend(s_event_queue, &event, 0);
    } else if (!s_sleep_pending && (pending == nullptr || !*pending)) {
        posted = xQueueSend(s_event_queue, &event, 0) == pdTRUE;
        if (posted && pending != nullptr) {
            *pending = true;
        }
    }
    taskEXIT_CRITICAL(&s_lock);
    return posted;
}

bool app_event_wait(AppEvent &event, TickType_t timeout)
{
    if (s_event_queue == nullptr || xQueueReceive(s_event_queue, &event, timeout) != pdTRUE) {
        return false;
    }
    taskENTER_CRITICAL(&s_lock);
    if (event == AppEvent::OrientationChanged) {
        s_orientation_pending = false;
    }
    if (event == AppEvent::ClockMinuteTick) {
        s_clock_pending = false;
    }
    if (s_sleep_pending) {
        event = AppEvent::EnterDeepSleep;
    }
    taskEXIT_CRITICAL(&s_lock);
    return true;
}

void app_event_cancel_sleep()
{
    taskENTER_CRITICAL(&s_lock);
    s_sleep_pending = false;
    s_orientation_pending = false;
    s_clock_pending = false;
    s_navigation_pending = false;
    xQueueReset(s_event_queue);
    taskEXIT_CRITICAL(&s_lock);
}

void app_event_complete(AppEvent event)
{
    if (event == AppEvent::NextPage || event == AppEvent::PreviousPage) {
        taskENTER_CRITICAL(&s_lock);
        s_navigation_pending = false;
        taskEXIT_CRITICAL(&s_lock);
    }
}
