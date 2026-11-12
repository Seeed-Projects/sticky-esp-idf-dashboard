#include "sticky_buttons.h"

#include <cstdint>
#include <atomic>

#include "app_event.h"
#include "button_gpio.h"
#include "iot_button.h"
#include "pin_config.h"
#include "driver/gpio.h"

namespace {

constexpr uint16_t kLongPressTimeMs = 2000;
constexpr uint16_t kShortPressTimeMs = 180;
button_handle_t s_up_button = nullptr;
button_handle_t s_down_button = nullptr;
button_handle_t s_refresh_button = nullptr;
bool s_initialized = false;
std::atomic<bool> s_ai_armed{false};
std::atomic<bool> s_ai_press_allowed{false};

void ai_press_callback(void *, void *)
{
    s_ai_press_allowed.store(s_ai_armed.load());
}

void ai_release_callback(void *, void *)
{
    // Arm future presses, but keep the just-released wake press suppressed,
    // including its delayed SINGLE_CLICK callback.
    s_ai_armed.store(true);
}

void button_click_callback(void *button_handle, void *user_data)
{
    if (button_handle == s_refresh_button && !s_ai_press_allowed.load()) {
        return;
    }
    const auto event = static_cast<AppEvent>(
        reinterpret_cast<uintptr_t>(user_data));
    app_event_post(event);
}

esp_err_t create_button(int gpio,
                        AppEvent event,
                        button_handle_t *button,
                        bool register_long_press = false,
                        AppEvent long_press_event = AppEvent::EnterDeepSleep)
{
    button_config_t button_config = {};
    button_config.long_press_time = kLongPressTimeMs;
    button_config.short_press_time = kShortPressTimeMs;

    button_gpio_config_t gpio_config = {};
    gpio_config.gpio_num = gpio;
    gpio_config.active_level = 0;
    gpio_config.enable_power_save = false;
    gpio_config.disable_pull = false;

    esp_err_t result =
        iot_button_new_gpio_device(&button_config, &gpio_config, button);
    if (result != ESP_OK) {
        return result;
    }

    if (register_long_press) {
        const bool released = gpio_get_level(static_cast<gpio_num_t>(gpio)) != 0;
        s_ai_armed.store(released);
        s_ai_press_allowed.store(released);
        result = iot_button_register_cb(*button, BUTTON_PRESS_DOWN, nullptr,
                                        ai_press_callback, nullptr);
        if (result != ESP_OK) return result;
        result = iot_button_register_cb(*button, BUTTON_PRESS_UP, nullptr,
                                        ai_release_callback, nullptr);
        if (result != ESP_OK) return result;
    }

    result = iot_button_register_cb(
        *button,
        BUTTON_SINGLE_CLICK,
        nullptr,
        button_click_callback,
        reinterpret_cast<void *>(static_cast<uintptr_t>(event)));
    if (result != ESP_OK || !register_long_press) {
        return result;
    }

    return iot_button_register_cb(
        *button,
        BUTTON_LONG_PRESS_START,
        nullptr,
        button_click_callback,
        reinterpret_cast<void *>(
            static_cast<uintptr_t>(long_press_event)));
}

}  // namespace

esp_err_t sticky_buttons_init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    // Also retry cleanup if a previous delete failed. Never overwrite a live
    // handle or report success after only part of the initialization completed.
    auto cleanup = []() -> esp_err_t {
        esp_err_t first_error = ESP_OK;
        button_handle_t *handles[] = {
            &s_refresh_button, &s_down_button, &s_up_button};
        for (button_handle_t *handle : handles) {
            if (*handle == nullptr) continue;
            const esp_err_t result = iot_button_delete(*handle);
            if (result == ESP_OK) {
                *handle = nullptr;
            } else if (first_error == ESP_OK) {
                first_error = result;
            }
        }
        return first_error;
    };
    esp_err_t result = cleanup();
    if (result != ESP_OK) return result;

    result = create_button(
        PIN_BTN_UP, AppEvent::PreviousPage, &s_up_button);
    if (result != ESP_OK) {
        cleanup();
        return result;
    }
    result = create_button(
        PIN_BTN_DOWN, AppEvent::NextPage, &s_down_button);
    if (result != ESP_OK) {
        cleanup();
        return result;
    }
    result = create_button(PIN_BTN_OK,
                         AppEvent::RefreshPage,
                         &s_refresh_button,
                         true,
                         AppEvent::EnterDeepSleep);
    if (result != ESP_OK) {
        cleanup();
        return result;
    }
    s_initialized = true;
    return ESP_OK;
}
