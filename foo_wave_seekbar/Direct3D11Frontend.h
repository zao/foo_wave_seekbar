#pragma once

#include "frontend_sdk/VisualFrontend.h"

namespace wave {
    struct d3d11_frontend : visual_frontend
    {
        void clear() override;
        void draw() override;
        void present() override;
        void on_state_changed(state s) override;
        int get_present_interval() const override;
    };
}
