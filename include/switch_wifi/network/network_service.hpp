#pragma once

#include "switch_wifi/core/metrics.hpp"
#include <vector>

namespace swifi {

class NetworkService {
  public:
    bool initialize();
    void shutdown();

    ConnectionSnapshot snapshot();
    std::vector<SavedNetwork> savedNetworks();
    std::vector<int> allowedWifiChannels();

    bool legacyRssiSupported() const;

  private:
    bool initialized_{false};
    bool wlanInfInitialized_{false};
};

} // namespace swifi
