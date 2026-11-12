#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

constexpr uint16_t kStickyDisplayWidth = 800;
constexpr uint16_t kStickyDisplayHeight = 480;

// Initializes SPI2, powers and detects SSD1677/SSD2677, and creates the framebuffer.
esp_err_t sticky_display_init();

// Returns the drawing surface after sticky_display_init() succeeds.
Canvas *sticky_display_canvas();

// Mode of the most recent successful refresh, for application diagnostics.
const char *sticky_display_last_refresh_mode();

// Rotates the logical framebuffer 180 degrees and sends it to the panel.
// Canvas coordinates remain a normal 800 x 480, top-left-origin coordinate system.
esp_err_t sticky_display_refresh();

// Refreshes black-and-white changes made to Canvas since the previous refresh.
// Sends a full comparison frame through the detected controller's driver;
// that transfer and the physical 180-degree rotation are handled internally.
// After 20 successful partial refreshes, or after a refresh failure, the
// next partial request is promoted to a monochrome full refresh.
esp_err_t sticky_display_refresh_partial();

// Refreshes the full screen with the panel's black-and-white waveform.
// Intended for pages that contain only Black and White canvas pixels.
esp_err_t sticky_display_refresh_monochrome();

// Clears the physical panel to white and clears the framebuffer.
esp_err_t sticky_display_clear();

// Called by the app after its event queue has been idle. The next refresh
// resumes/reinitializes through the official component's prepare path.
// SSD1677 Gray4 already powers down its analog/clock and skips extra standby.
esp_err_t sticky_display_standby();

// Puts the panel controller to sleep while retaining its supply, matching
// the official display lifecycle. This is separate from turning off EPD_EN.
esp_err_t sticky_display_sleep();
