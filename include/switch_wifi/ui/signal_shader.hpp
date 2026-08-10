#pragma once

#include <borealis.hpp>
#include <mutex>

namespace swifi {

// GPU-backed radio visualization rendered through Borealis/NanoVG. It is a
// visual status shader, not an RF measurement and does not imply a noise floor.
class SignalShaderView : public brls::View {
  public:
    SignalShaderView();
    void setSignal(float normalizedStrength, bool connected);
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    std::mutex mutex_;
    float strength_{0.0f};
    bool connected_{false};
};

} // namespace swifi
