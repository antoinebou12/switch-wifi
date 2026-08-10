#include "switch_wifi/ui/sparkline.hpp"

#include <algorithm>

namespace swifi {

SparklineView::SparklineView() {
    setHeight(154);
}

void SparklineView::setSamples(std::vector<float> samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    samples_ = std::move(samples);
    invalidate();
}

void SparklineView::draw(NVGcontext* vg, float x, float y, float width, float height,
                         brls::Style style, brls::FrameContext* ctx) {
    (void)style;
    (void)ctx;

    std::vector<float> samples;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        samples = samples_;
    }

    nvgSave(vg);
    const float corner = 12.0f;
    NVGpaint panel = nvgLinearGradient(vg, x, y, x, y + height,
        nvgRGBA(20, 27, 40, 238), nvgRGBA(9, 13, 21, 238));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, corner);
    nvgFillPaint(vg, panel);
    nvgFill(vg);

    const float pad = 14.0f;
    const float usableW = std::max(1.0f, width - pad * 2.0f);
    const float usableH = std::max(1.0f, height - pad * 2.0f);

    for (int i = 1; i < 4; ++i) {
        const float gy = y + pad + usableH * static_cast<float>(i) / 4.0f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x + pad, gy);
        nvgLineTo(vg, x + width - pad, gy);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 20));
        nvgStroke(vg);
    }

    if (samples.empty()) {
        nvgRestore(vg);
        return;
    }

    const auto [minIt, maxIt] = std::minmax_element(samples.begin(), samples.end());
    float minValue = *minIt;
    float maxValue = *maxIt;
    if (maxValue - minValue < 0.001f) maxValue = minValue + 1.0f;

    auto point = [&](std::size_t i) {
        const float t = samples.size() == 1 ? 0.0f
            : static_cast<float>(i) / static_cast<float>(samples.size() - 1);
        const float normalized = (samples[i] - minValue) / (maxValue - minValue);
        return std::pair<float, float>{x + pad + t * usableW,
                                      y + pad + (1.0f - normalized) * usableH};
    };

    nvgBeginPath(vg);
    const auto first = point(0);
    nvgMoveTo(vg, first.first, y + height - pad);
    nvgLineTo(vg, first.first, first.second);
    for (std::size_t i = 1; i < samples.size(); ++i) {
        const auto p = point(i);
        nvgLineTo(vg, p.first, p.second);
    }
    const auto last = point(samples.size() - 1);
    nvgLineTo(vg, last.first, y + height - pad);
    nvgClosePath(vg);
    NVGpaint area = nvgLinearGradient(vg, x, y + pad, x, y + height - pad,
        nvgRGBA(59, 196, 177, 115), nvgRGBA(59, 196, 177, 4));
    nvgFillPaint(vg, area);
    nvgFill(vg);

    auto strokeCurve = [&](float widthPx, NVGcolor color) {
        nvgBeginPath(vg);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto p = point(i);
            if (i == 0) nvgMoveTo(vg, p.first, p.second);
            else nvgLineTo(vg, p.first, p.second);
        }
        nvgStrokeWidth(vg, widthPx);
        nvgStrokeColor(vg, color);
        nvgStroke(vg);
    };

    strokeCurve(9.0f, nvgRGBA(57, 219, 192, 30));
    strokeCurve(4.0f, nvgRGBA(76, 224, 199, 235));

    nvgBeginPath(vg);
    nvgCircle(vg, last.first, last.second, 5.0f);
    nvgFillColor(vg, nvgRGBA(226, 255, 249, 255));
    nvgFill(vg);

    nvgRestore(vg);
}

} // namespace swifi
