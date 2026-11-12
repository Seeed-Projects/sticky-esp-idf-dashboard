#include "app.h"
#include "app_clock.h"
#include "app_data.h"
#include "board.h"
#include "canvas.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "sticky_buzzer.h"
#include "sticky_battery.h"
#include "sticky_buttons.h"
#include "sticky_charger.h"
#include "sticky_display.h"
#include "sticky_imu.h"
#include "sticky_microphone.h"
#include "sticky_power.h"
#include "sticky_rtc.h"
#include "sticky_sdcard.h"
#include "sticky_sht40.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "sticky_dashboard";
// Demo scheduling policy, not a panel timing requirement. Keep consecutive
// operations active and enter standby after one second without app events.
constexpr TickType_t kDisplayIdleWait = pdMS_TO_TICKS(1000);

void init_optional(const char *name, esp_err_t result)
{
    if (result != ESP_OK) {
        ESP_LOGW(kTag, "%s unavailable: %s", name, esp_err_to_name(result));
    }
}

void show_initial_page(Canvas &canvas, const AppState &state)
{
    render_current_page(canvas, state);
    const int64_t start_us = esp_timer_get_time();
    const esp_err_t result = refresh_current_page(state);
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Initial paint failed: %s; press AI to retry", esp_err_to_name(result));
    } else {
        ESP_LOGI(kTag, "Initial page displayed: %s, elapsed=%lld ms",
                 sticky_display_last_refresh_mode(),
                 static_cast<long long>((esp_timer_get_time() - start_us) / 1000));
    }
}

}  // namespace

extern "C" void app_main()
{
    // GPIO configuration dumps are useful for bring-up, not normal Demo use.
    // Keep warnings/errors and the page, driver, and peripheral logs visible.
    esp_log_level_set("gpio", ESP_LOG_WARN);
    esp_log_level_set("sdspi_transaction", ESP_LOG_WARN);
    ESP_LOGI(kTag, "Starting reTerminal Sticky Dashboard Demo");
    sticky_power_log_wakeup_reason();

    ESP_ERROR_CHECK(board_init());
    init_optional("SHT40", sticky_sht40_init(board_sensor_i2c_bus()));
    ESP_ERROR_CHECK(sticky_display_init());
    init_optional("MicroSD", sticky_sdcard_init());
    ESP_ERROR_CHECK(app_event_init());
    ESP_ERROR_CHECK(sticky_buttons_init());
    init_optional("Touch", sticky_touch_init());
    const esp_err_t buzzer_result = sticky_buzzer_init();
    init_optional("Buzzer", buzzer_result);
    if (buzzer_result == ESP_OK &&
        esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
        init_optional("Wake feedback", sticky_buzzer_beep());
    }
    init_optional("Power detection", sticky_charger_init());
    init_optional("Battery", sticky_battery_init(board_sensor_i2c_bus()));
    init_optional("RTC", sticky_rtc_init(board_sensor_i2c_bus()));
    init_optional("RTC time", sticky_rtc_initialize_time());
    init_optional("IMU", sticky_imu_init(board_sensor_i2c_bus()));
    init_optional("Microphone", sticky_microphone_init());

    Canvas *canvas = sticky_display_canvas();
    ESP_ERROR_CHECK(canvas != nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);

    AppState state;
    restore_app_state_after_deep_sleep(state);
    update_date_time(state);
    if (state.date_time.valid) {
        ESP_LOGI(kTag, "RTC time: %04d-%02d-%02d %02d:%02d:%02d",
                 state.date_time.year, state.date_time.month, state.date_time.day,
                 state.date_time.hour, state.date_time.minute, state.date_time.second);
    }
    show_initial_page(*canvas, state);
    update_date_time(state);  // Align the timer after the slow initial paint.
    ESP_ERROR_CHECK(
        app_clock_start(state.date_time.valid ? state.date_time.second : -1));
    init_optional("IMU monitor", sticky_imu_start_monitoring());

    while (true) {
        AppEvent event;
        if (!app_event_wait(event, kDisplayIdleWait)) {
            const esp_err_t idle_result = sticky_display_standby();
            if (idle_result != ESP_OK) {
                ESP_LOGW(kTag, "Display standby failed: %s", esp_err_to_name(idle_result));
            }
            continue;
        }

        const esp_err_t result = handle_app_event(*canvas, state, event);
        app_event_complete(event);
        if (result != ESP_OK) {
            if (event == AppEvent::EnterDeepSleep) {
                app_event_cancel_sleep();
                init_optional("IMU resume", sticky_imu_start_monitoring());
                init_optional("Touch resume", sticky_touch_init());
                update_date_time(state);
                init_optional("Clock resume", app_clock_start(
                    state.date_time.valid ? state.date_time.second : -1));
                render_current_page(*canvas, state);
                const esp_err_t restore_result = refresh_current_page(state);
                if (restore_result != ESP_OK) {
                    ESP_LOGE(kTag, "Sleep recovery paint failed: %s; press AI to retry",
                             esp_err_to_name(restore_result));
                }
            }
            ESP_LOGE(kTag, "App event failed: %s", esp_err_to_name(result));
        }
    }
}
