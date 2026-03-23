#pragma once

struct DemoTierConfig;

// Validates tier config fields and logs warnings for out-of-range values.
// Returns true if all fields are valid.
bool validateTierConfig(const DemoTierConfig& cfg);
