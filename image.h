#pragma once

#include <cstddef>
#include "stb_image.h"

struct Image {
    int width;
    int height;
    int depth;
    size_t length;
    stbi_uc* data;
};


Image
image_load (const char* fname);
