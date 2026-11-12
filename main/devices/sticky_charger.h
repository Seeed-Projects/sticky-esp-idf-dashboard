#pragma once

#include "esp_err.h"

// Enables the onboard charger and configures external-power detection.
esp_err_t sticky_charger_init();

// Returns true while USB/external power is present. The legacy parameter name
// does not mean that battery charging current or full-charge status is measured.
esp_err_t sticky_charger_read(bool &charging);
