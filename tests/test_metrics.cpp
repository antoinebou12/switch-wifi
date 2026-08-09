#include "switch_wifi/core/metrics.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace swifi;

int main() {
    assert(std::abs(bitsPerSecondToMbps(8'000'000.0) - 8.0) < 1e-9);
    assert(std::abs(transferMbps(1'000'000, 1.0) - 8.0) < 1e-9);
    assert(transferMbps(10, 0.0) == 0.0);

    assert(classifyRssi(-45) == SignalQuality::Excellent);
    assert(classifyRssi(-55) == SignalQuality::Good);
    assert(classifyRssi(-65) == SignalQuality::Fair);
    assert(classifyRssi(-80) == SignalQuality::Poor);
    assert(classifyBars(3) == SignalQuality::Excellent);
    assert(classifyBars(-1) == SignalQuality::Unknown);

    assert(wifiBandForChannel(1) == WifiBand::Ghz2_4);
    assert(wifiBandForChannel(36) == WifiBand::Ghz5);
    assert(wifiBandForChannel(0) == WifiBand::Unknown);
    assert(wifiChannelFrequencyMhz(1) == 2412);
    assert(wifiChannelFrequencyMhz(6) == 2437);
    assert(wifiChannelFrequencyMhz(11) == 2462);
    assert(wifiChannelFrequencyMhz(14) == 2484);
    assert(wifiChannelFrequencyMhz(36) == 5180);
    assert(wifiChannelFrequencyMhz(0) == 0);

    assert(std::abs(channelOverlapWeight(6, 6) - 1.0) < 1e-9);
    assert(channelOverlapWeight(1, 11) == 0.0);
    assert(channelOverlapWeight(36, 36) == 1.0);
    assert(channelOverlapWeight(36, 40) == 0.0);
    assert(channelOverlapWeight(6, 36) == 0.0);

    std::vector<NearbyWifiNetwork> aps{
        {"same", "", 6, -50, true, ""},
        {"overlap", "", 5, -60, true, ""},
        {"far", "", 11, -40, true, ""},
        {"unknown-rssi", "", 6, 0, false, ""},
    };
    const double contention = contentionProxy(6, aps);
    assert(contention > 1.0 && contention < 2.0);

    std::cout << "metric tests passed\n";
    return 0;
}
