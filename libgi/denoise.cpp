#include "denoise.h"

void denoise(size_t width, size_t height, glm::vec4 *color) {
    denoise(width, height, color, nullptr, nullptr, false);
}

void denoise(size_t width, size_t height, glm::vec4 *color, glm::vec4 *albedo, glm::vec4 *normal, bool prefilter_auxiliary) {
#ifdef HAVE_LIBOPENIMAGEDENOISE
    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color", color, oidn::Format::Float3, width, height, 0, 16, 0);
    if (albedo)
        filter.setImage("albedo", albedo, oidn::Format::Float3, width, height, 0, 16, 0);
    if (normal)
        filter.setImage("normal", normal, oidn::Format::Float3, width, height, 0, 16, 0);
    filter.setImage("output", color, oidn::Format::Float3, width, height, 0, 16, 0);
    filter.set("hdr", true);
    if (prefilter_auxiliary)
        filter.set("cleanAux", true);
    filter.commit();

    if (prefilter_auxiliary) {
        if (albedo) {
            oidn::FilterRef albedo_filter = device.newFilter("RT");
            albedo_filter.setImage("albedo", albedo, oidn::Format::Float3, width, height, 0, 16, 0);
            albedo_filter.setImage("output", albedo, oidn::Format::Float3, width, height, 0, 16, 0);
            albedo_filter.commit();
            albedo_filter.execute();
        }
        if (normal) {
            oidn::FilterRef normal_filter = device.newFilter("RT");
            normal_filter.setImage("normal", normal, oidn::Format::Float3, width, height, 0, 16, 0);
            normal_filter.setImage("output", normal, oidn::Format::Float3, width, height, 0, 16, 0);
            normal_filter.commit();
            normal_filter.execute();
        }
    }

    filter.execute();

    const char *error_message;
    if (device.getError(error_message) != oidn::Error::None)
        std::cout << "OIDN: Error: " << error_message << "\n";
#endif
}
