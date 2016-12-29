#pragma once
#include <vector>

namespace imaging
{
    std::vector<int> slico(
        const unsigned int* img,
        const int width,
        const int height,
        const int no_superpixels);
}
