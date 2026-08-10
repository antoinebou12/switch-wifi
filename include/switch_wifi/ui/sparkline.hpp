#pragma once

#include <borealis.hpp>
#include <mutex>
#include <vector>

namespace swifi {

class SparklineView : public brls::View {
  public:
    SparklineView();
    void setSamples(std::vector<float> samples);
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    std::mutex mutex_;
    std::vector<float> samples_;
};

} // namespace swifi
