#include <cstdlib>

#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

int
main (int argc, char** argv) {
    START_EASYLOGGINGPP(argc, argv);
    LOG(INFO) << "Hello, world!";
    return 0;
}

