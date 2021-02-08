#include "gui.hpp"

namespace retrojsvice {

void renderUploadModeGUI(vector<uint8_t>& data, size_t width, size_t height) {
    REQUIRE(data.size() >= 4 * width * height);

    for(uint8_t& val : data) {
        val = (val >> 1) | (uint8_t)0x80;
    }
}

}
