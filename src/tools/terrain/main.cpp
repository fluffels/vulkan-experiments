#include <iostream>

#include "lib/meshes/Terrain.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using std::cout;
using std::cerr;
using std::endl;
using std::exception;

void
usage() {
    cout << "usage: terrain.exe <heightmapFile>" << endl;
    cout << endl;
    cout << "\theightmapFile\t\tPath to a greyscale image to be used to "
         << "generate the heightmap." << endl;
    cout << endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    try {
        Terrain terrain(argv[1]);
        cout << "width:\t\t" << terrain.getWidth() << endl;
        cout << "depth:\t\t" << terrain.getDepth() << endl;
        cout << "max height:\t" << terrain.getMaxHeight() << endl;
    } catch (exception& e) {
        cerr << e.what() << endl;
    }
    return 0;
}
