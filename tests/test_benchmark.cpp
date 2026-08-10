#include "switch_wifi/core/metrics.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace swifi;

int main() {
    assert(percentile({}, 0.5) == 0.0);
    assert(percentile({10.0}, 0.95) == 10.0);
    assert(std::abs(percentile({10.0, 20.0}, 0.5) - 15.0) < 1e-9);
    assert(percentile({1.0, 2.0, 3.0, 4.0, 5.0}, 0.95) > 4.0);

    const std::vector<double> probes{10.0, 11.0, 9.0, 10.0, 12.0};
    const LatencyStats stats = summarizeLatencySamples(probes);
    assert(stats.samples == probes.size());
    assert(stats.minimumMs == 9.0);
    assert(std::abs(stats.averageMs - 10.4) < 1e-9);
    assert(stats.medianMs == 10.0);
    assert(stats.p95Ms >= 11.0 && stats.p95Ms <= 12.0);
    assert(std::abs(stats.jitterMs - 1.5) < 1e-9);

    const LatencyStats empty = summarizeLatencySamples({});
    assert(empty.samples == 0);
    assert(empty.medianMs == 0.0);

    std::cout << "benchmark tests passed\n";
    return 0;
}
