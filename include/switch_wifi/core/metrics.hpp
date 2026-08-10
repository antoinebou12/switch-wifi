#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <numeric>
#include <string>
#include <vector>

namespace swifi {

enum class LinkType { None, WiFi, Ethernet };
enum class SignalQuality { Unknown, Poor, Fair, Good, Excellent };
enum class WifiBand { Unknown, Ghz2_4, Ghz5 };

struct IpConfig {
    std::string address{"-"};
    std::string subnet{"-"};
    std::string gateway{"-"};
    std::string primaryDns{"-"};
    std::string secondaryDns{"-"};
    std::uint16_t mtu{0};
};

struct ConnectionSnapshot {
    bool wirelessEnabled{false};
    bool internetConnected{false};
    LinkType linkType{LinkType::None};
    std::string ssid{"-"};
    int wifiBars{-1};
    int rssiDbm{0};
    bool hasRssi{false};
    IpConfig ip;
    std::string diagnostic{"Not sampled"};
};

struct SavedNetwork {
    std::string name;
    std::string ssid;
    std::string authentication;
    std::string encryption;
};

struct NearbyWifiNetwork {
    std::string ssid;
    std::string bssid;
    int channel{0};
    int rssiDbm{0};
    bool hasRssi{false};
    std::string security{"Unknown"};
};

struct LatencyStats {
    double minimumMs{0.0};
    double averageMs{0.0};
    double medianMs{0.0};
    double p95Ms{0.0};
    double jitterMs{0.0};
    std::size_t samples{0};
};

struct SpeedSample {
    bool completed{false};
    bool cancelled{false};
    double latencyMs{0.0};
    double latencyMinMs{0.0};
    double latencyAverageMs{0.0};
    double latencyP95Ms{0.0};
    double jitterMs{0.0};
    std::uint32_t latencyProbeCount{0};
    double downloadMbps{0.0};
    double uploadMbps{0.0};
    std::uint64_t downloadedBytes{0};
    std::uint64_t uploadedBytes{0};
};

struct BluetoothDevice {
    std::string address;
    std::string name;
};

struct BluetoothSnapshot {
    bool serviceAvailable{false};
    int state{-1};
    int connectedDevices{-1};
    int knownDevices{-1};
    std::string diagnostic{"Not sampled"};
};

struct BluetoothScanResult {
    bool serviceAvailable{false};
    bool scanSupported{false};
    std::vector<BluetoothDevice> devices;
    std::string diagnostic{"Not scanned"};
};

inline double bitsPerSecondToMbps(double bps) { return bps / 1'000'000.0; }

inline double transferMbps(std::uint64_t bytes, double elapsedSeconds) {
    if (elapsedSeconds <= 0.0) return 0.0;
    return bitsPerSecondToMbps((static_cast<double>(bytes) * 8.0) / elapsedSeconds);
}

inline SignalQuality classifyRssi(int dbm) {
    if (dbm >= -50) return SignalQuality::Excellent;
    if (dbm >= -60) return SignalQuality::Good;
    if (dbm >= -70) return SignalQuality::Fair;
    return SignalQuality::Poor;
}

inline SignalQuality classifyBars(int bars) {
    switch (bars) {
        case 3: return SignalQuality::Excellent;
        case 2: return SignalQuality::Good;
        case 1: return SignalQuality::Fair;
        case 0: return SignalQuality::Poor;
        default: return SignalQuality::Unknown;
    }
}

inline const char* toString(SignalQuality quality) {
    switch (quality) {
        case SignalQuality::Poor: return "Poor";
        case SignalQuality::Fair: return "Fair";
        case SignalQuality::Good: return "Good";
        case SignalQuality::Excellent: return "Excellent";
        default: return "Unknown";
    }
}

inline const char* toString(LinkType type) {
    switch (type) {
        case LinkType::WiFi: return "Wi-Fi";
        case LinkType::Ethernet: return "Ethernet";
        default: return "Disconnected";
    }
}

inline const char* toString(WifiBand band) {
    switch (band) {
        case WifiBand::Ghz2_4: return "2.4 GHz";
        case WifiBand::Ghz5: return "5 GHz";
        default: return "Unknown";
    }
}

inline WifiBand wifiBandForChannel(int channel) {
    if (channel >= 1 && channel <= 14) return WifiBand::Ghz2_4;
    if (channel >= 32 && channel <= 177) return WifiBand::Ghz5;
    return WifiBand::Unknown;
}

inline int wifiChannelFrequencyMhz(int channel) {
    if (channel >= 1 && channel <= 13) return 2407 + channel * 5;
    if (channel == 14) return 2484;
    if (channel >= 32 && channel <= 177) return 5000 + channel * 5;
    return 0;
}

inline double normalizedSignalWeight(int rssiDbm) {
    const double value = (static_cast<double>(rssiDbm) + 90.0) / 60.0;
    return std::max(0.0, std::min(1.0, value));
}

inline double channelOverlapWeight(int referenceChannel, int otherChannel) {
    const WifiBand a = wifiBandForChannel(referenceChannel);
    const WifiBand b = wifiBandForChannel(otherChannel);
    if (a == WifiBand::Unknown || a != b) return 0.0;
    if (a == WifiBand::Ghz2_4) {
        const int frequencyDelta = std::abs(wifiChannelFrequencyMhz(referenceChannel) - wifiChannelFrequencyMhz(otherChannel));
        return std::max(0.0, 1.0 - static_cast<double>(frequencyDelta) / 22.0);
    }
    return referenceChannel == otherChannel ? 1.0 : 0.0;
}

inline double contentionProxy(int referenceChannel, const std::vector<NearbyWifiNetwork>& networks) {
    double score = 0.0;
    for (const auto& network : networks) {
        if (!network.hasRssi || network.channel <= 0) continue;
        score += channelOverlapWeight(referenceChannel, network.channel) * normalizedSignalWeight(network.rssiDbm);
    }
    return score;
}

inline double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double clamped = std::max(0.0, std::min(1.0, p));
    const double index = clamped * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(index));
    const auto upper = static_cast<std::size_t>(std::ceil(index));
    if (lower == upper) return values[lower];
    const double fraction = index - static_cast<double>(lower);
    return values[lower] + (values[upper] - values[lower]) * fraction;
}

inline LatencyStats summarizeLatencySamples(const std::vector<double>& samples) {
    LatencyStats stats;
    if (samples.empty()) return stats;
    stats.samples = samples.size();
    stats.minimumMs = *std::min_element(samples.begin(), samples.end());
    stats.averageMs = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());
    stats.medianMs = percentile(samples, 0.5);
    stats.p95Ms = percentile(samples, 0.95);
    if (samples.size() > 1) {
        double deltaSum = 0.0;
        for (std::size_t i = 1; i < samples.size(); ++i) deltaSum += std::abs(samples[i] - samples[i - 1]);
        stats.jitterMs = deltaSum / static_cast<double>(samples.size() - 1);
    }
    return stats;
}

template <typename T>
class RingHistory {
  public:
    explicit RingHistory(std::size_t capacity = 120) : capacity_(std::max<std::size_t>(1, capacity)) {}
    void push(const T& value) { if (values_.size() == capacity_) values_.pop_front(); values_.push_back(value); }
    void clear() { values_.clear(); }
    std::size_t size() const { return values_.size(); }
    std::size_t capacity() const { return capacity_; }
    bool empty() const { return values_.empty(); }
    std::vector<T> values() const { return {values_.begin(), values_.end()}; }
  private:
    std::size_t capacity_;
    std::deque<T> values_;
};

} // namespace swifi
