#include "image.h"
#include "easylogging++.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Image
image_load (const char* fname) {
    Image img;
    stbi_uc* pixels = stbi_load(
            "texture.jpg", &img.width, &img.height, &img.depth, STBI_rgb_alpha
    );
    if (!pixels) {
        LOG(ERROR) << "Could not load texture.";
    }
    img.length = img.width * img.height * img.depth;
    return img;
}
