#pragma once

#include "switch_wifi/core/metrics.hpp"

#include <cstdint>

namespace swifi {

class BluetoothService {
  public:
    BluetoothSnapshot snapshot();

    // Explicit, bounded, read-only BLE discovery. It never pairs, connects,
    // disconnects, or modifies controller state. Unsupported launch contexts
    // simply return a diagnostic instead of failing the app.
    BluetoothScanResult passiveScan(std::uint32_t durationMs = 1500);
};

} // namespace swifi
