#pragma once

#include <cstddef>

#include "esp_err.h"

enum class StickySdStatus {
    Unavailable, NoCard, MountFailed, MissingFile, ReadFailed,
    Empty, Blank, Ready, Truncated, CleanupFailed,
};

struct StickySdReadResult {
    StickySdStatus status = StickySdStatus::Unavailable;
    size_t bytes_read = 0;
    // True when unread bytes remain; Blank describes only the returned prefix.
    bool truncated = false;
    esp_err_t cleanup_error = ESP_OK;
};

// Configures card detection, power, and chip-select GPIOs. SPI2 must already
// be initialized by the display/shared bus owner.
esp_err_t sticky_sdcard_init();

// Mounts the card on the existing shared SPI2 bus, reads one text file, then
// unmounts it. The output is always null-terminated when buffer_size > 0.
esp_err_t sticky_sdcard_read_text(const char *file_name,
                                  char *buffer,
                                  size_t buffer_size,
                                  StickySdReadResult *details = nullptr);
