#include "switch_wifi/network/speed_test.hpp"

#include <curl/curl.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace swifi {
namespace {
struct TransferContext { std::atomic<bool>* cancelled{}; std::uint64_t bytes{0}; };
size_t discardWrite(char*, size_t size, size_t nmemb, void* userdata) { auto* ctx = static_cast<TransferContext*>(userdata); const size_t bytes = size * nmemb; ctx->bytes += bytes; return bytes; }
int progressFn(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) { auto* cancelled = static_cast<std::atomic<bool>*>(userdata); return cancelled->load() ? 1 : 0; }
void configureCommon(CURL* curl, const SpeedTestConfig& config, std::atomic<bool>* cancelled) {
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); curl_easy_setopt(curl, CURLOPT_USERAGENT, "switch-wifi/0.3");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, config.connectTimeoutMs); curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config.totalTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L); curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressFn); curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cancelled);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L); curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
}
std::string downUrl(const SpeedTestConfig& cfg, std::uint64_t bytes) { return cfg.downloadEndpoint + "?bytes=" + std::to_string(bytes); }
bool performLatencyProbe(CURL* curl, const SpeedTestConfig& config, std::atomic<bool>* cancelled, double* milliseconds) {
    curl_easy_reset(curl); configureCommon(curl, config, cancelled); TransferContext ctx{cancelled, 0}; const std::string url = downUrl(config, config.latencyProbeBytes);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardWrite); curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    const CURLcode code = curl_easy_perform(curl); if (code != CURLE_OK) return false; double startTransferSeconds = 0.0;
    curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &startTransferSeconds); *milliseconds = startTransferSeconds * 1000.0; return true;
}
} // namespace

SpeedTestEngine::SpeedTestEngine(SpeedTestConfig config) : config_(std::move(config)) {}
SpeedTestEngine::~SpeedTestEngine() { cancel(); }
void SpeedTestEngine::cancel() { cancelled_.store(true); }

SpeedSample SpeedTestEngine::run(const ProgressCallback& callback) {
    if (running_.exchange(true)) throw std::runtime_error("speed test already running");
    cancelled_.store(false);
    struct RunningReset { std::atomic<bool>& running; ~RunningReset() { running.store(false); } } reset{running_};
    SpeedSample sample;
    auto notify = [&](SpeedTestStage stage, double progress, const std::string& message) { if (callback) callback({stage, progress, sample, message}); };
    CURL* curl = curl_easy_init(); if (!curl) { notify(SpeedTestStage::Failed, 0.0, "curl_easy_init failed"); throw std::runtime_error("curl_easy_init failed"); }
    auto cleanup = [&] { curl_easy_cleanup(curl); };
    const std::uint32_t probeCount = std::max<std::uint32_t>(1, std::min<std::uint32_t>(config_.latencyProbeCount, 20));
    std::vector<double> latencySamples; latencySamples.reserve(probeCount);
    for (std::uint32_t i = 0; i < probeCount; ++i) {
        if (cancelled_.load()) { cleanup(); sample.cancelled = true; notify(SpeedTestStage::Cancelled, 0.0, "Cancelled"); return sample; }
        notify(SpeedTestStage::Latency, 0.05 + 0.10 * (static_cast<double>(i) / static_cast<double>(probeCount)), "Latency probe " + std::to_string(i + 1) + "/" + std::to_string(probeCount));
        double milliseconds = 0.0;
        if (!performLatencyProbe(curl, config_, &cancelled_, &milliseconds)) {
            if (cancelled_.load()) { cleanup(); sample.cancelled = true; notify(SpeedTestStage::Cancelled, 0.0, "Cancelled"); return sample; }
            cleanup(); notify(SpeedTestStage::Failed, 0.0, "Latency probe failed"); throw std::runtime_error("latency probe failed");
        }
        latencySamples.push_back(milliseconds);
    }
    const LatencyStats latency = summarizeLatencySamples(latencySamples);
    sample.latencyMs = latency.medianMs; sample.latencyMinMs = latency.minimumMs; sample.latencyAverageMs = latency.averageMs;
    sample.latencyP95Ms = latency.p95Ms; sample.jitterMs = latency.jitterMs; sample.latencyProbeCount = static_cast<std::uint32_t>(latency.samples);
    notify(SpeedTestStage::Download, 0.20, "Measuring download throughput");
    curl_easy_reset(curl); TransferContext downloadCtx{&cancelled_, 0}; configureCommon(curl, config_, &cancelled_);
    const std::string downloadUrl = downUrl(config_, config_.downloadBytes); curl_easy_setopt(curl, CURLOPT_URL, downloadUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardWrite); curl_easy_setopt(curl, CURLOPT_WRITEDATA, &downloadCtx);
    const auto dlStart = std::chrono::steady_clock::now(); CURLcode code = curl_easy_perform(curl); const auto dlEnd = std::chrono::steady_clock::now();
    if (code != CURLE_OK) { cleanup(); if (cancelled_.load()) { sample.cancelled = true; notify(SpeedTestStage::Cancelled, 0.0, "Cancelled"); return sample; } notify(SpeedTestStage::Failed, 0.0, curl_easy_strerror(code)); throw std::runtime_error(curl_easy_strerror(code)); }
    sample.downloadedBytes = downloadCtx.bytes; sample.downloadMbps = transferMbps(downloadCtx.bytes, std::chrono::duration<double>(dlEnd - dlStart).count());
    notify(SpeedTestStage::Upload, 0.65, "Measuring upload throughput"); curl_easy_reset(curl); configureCommon(curl, config_, &cancelled_);
    std::vector<unsigned char> upload(static_cast<std::size_t>(config_.uploadBytes), 0); TransferContext uploadResponse{&cancelled_, 0};
    curl_easy_setopt(curl, CURLOPT_URL, config_.uploadEndpoint.c_str()); curl_easy_setopt(curl, CURLOPT_POST, 1L); curl_easy_setopt(curl, CURLOPT_POSTFIELDS, upload.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(upload.size())); curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardWrite); curl_easy_setopt(curl, CURLOPT_WRITEDATA, &uploadResponse);
    const auto ulStart = std::chrono::steady_clock::now(); code = curl_easy_perform(curl); const auto ulEnd = std::chrono::steady_clock::now();
    if (code != CURLE_OK) { cleanup(); if (cancelled_.load()) { sample.cancelled = true; notify(SpeedTestStage::Cancelled, 0.0, "Cancelled"); return sample; } notify(SpeedTestStage::Failed, 0.0, curl_easy_strerror(code)); throw std::runtime_error(curl_easy_strerror(code)); }
    sample.uploadedBytes = config_.uploadBytes; sample.uploadMbps = transferMbps(config_.uploadBytes, std::chrono::duration<double>(ulEnd - ulStart).count());
    cleanup(); sample.completed = true; notify(SpeedTestStage::Complete, 1.0, "Complete"); return sample;
}

} // namespace swifi
