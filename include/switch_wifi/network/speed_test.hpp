#pragma once

#include "switch_wifi/core/metrics.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace swifi {

struct SpeedTestConfig {
    std::string downloadEndpoint{"https://speed.cloudflare.com/__down"};
    std::string uploadEndpoint{"https://speed.cloudflare.com/__up"};
    std::uint64_t latencyProbeBytes{0};
    std::uint32_t latencyProbeCount{8};
    std::uint64_t downloadBytes{25'000'000};
    std::uint64_t uploadBytes{10'000'000};
    long connectTimeoutMs{5000};
    long totalTimeoutMs{30000};
};

enum class SpeedTestStage { Idle, Latency, Download, Upload, Complete, Failed, Cancelled };

struct SpeedTestProgress {
    SpeedTestStage stage{SpeedTestStage::Idle};
    double progress{0.0};
    SpeedSample result;
    std::string message;
};

class SpeedTestEngine {
  public:
    using ProgressCallback = std::function<void(const SpeedTestProgress&)>;

    explicit SpeedTestEngine(SpeedTestConfig config = {});
    ~SpeedTestEngine();

    SpeedSample run(const ProgressCallback& callback = {});
    void cancel();
    bool isRunning() const { return running_.load(); }

  private:
    SpeedTestConfig config_;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> running_{false};
};

} // namespace swifi
