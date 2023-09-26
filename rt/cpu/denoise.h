#pragma once

#include "platform.h"
#include "wavefront.h"
#include "preprocessing.h"

namespace wf::cpu {

    struct add_hitpoint_albedo_to_framebuffer : public wf::wire::add_hitpoint_albedo_to_framebuffer<raydata> {
        void run() override;
    };

    struct add_hitpoint_normal_to_framebuffer : public wf::wire::add_hitpoint_normal_to_framebuffer<raydata> {
        void run() override;
    };
}
