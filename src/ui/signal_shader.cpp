#include "switch_wifi/ui/signal_shader.hpp"

#include <algorithm>

namespace swifi {

SignalShaderView::SignalShaderView() {
    setHeight(118);
}

void SignalShaderView::setSignal(float normalizedStrength, bool connected) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        strength_ = std::max(0.0f, std::min(1.0f, normalizedStrength));
        connected_ = connected;
    }
    invalidate();
}

void SignalShaderView::draw(NVGcontext* vg, float x, float y, float width, float height,
                            brls::Style style, brls::FrameContext* ctx) {
    (void)style;
    (void)ctx;

    float strength = 0.0f;
    bool connected = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        strength = strength_;
        connected = connected_;
    }

    nvgSave(vg);
    const float radius = 14.0f;

    NVGpaint panel = nvgLinearGradient(vg, x, y, x + width, y + height,
        nvgRGBA(18, 24, 36, 245), nvgRGBA(8, 12, 20, 245));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, radius);
    nvgFillPaint(vg, panel);
    nvgFill(vg);

    const float bloomX = x + width * (0.16f + 0.64f * strength);
    const float bloomY = y + height * 0.50f;
    const float bloomRadius = std::max(80.0f, width * (0.18f + 0.22f * strength));
    const NVGcolor inner = connected ? nvgRGBA(46, 213, 173, 150) : nvgRGBA(118, 128, 150, 90);
    const NVGcolor outer = nvgRGBA(0, 0, 0, 0);
    NVGpaint bloom = nvgRadialGradient(vg, bloomX, bloomY, 4.0f, bloomRadius, inner, outer);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, radius);
    nvgFillPaint(vg, bloom);
    nvgFill(vg);

    const float originX = x + 45.0f;
    const float originY = y + height * 0.72f;
    const int activeRings = connected ? 1 + static_cast<int>(strength * 3.99f) : 0;
    for (int i = 0; i < 4; ++i) {
        const float ringRadius = 20.0f + static_cast<float>(i) * 18.0f;
        nvgBeginPath(vg);
        nvgArc(vg, originX, originY, ringRadius, -1.15f, -0.02f, NVG_CW);
        nvgStrokeWidth(vg, 4.0f);
        nvgStrokeColor(vg, i < activeRings ? nvgRGBA(89, 224, 194, 235)
                                           : nvgRGBA(122, 137, 158, 65));
        nvgStroke(vg);
    }

    for (float sy = y + 12.0f; sy < y + height - 8.0f; sy += 10.0f) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, x + 12.0f, sy);
        nvgLineTo(vg, x + width - 12.0f, sy);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 10));
        nvgStroke(vg);
    }

    nvgRestore(vg);
}

} // namespace swifi
