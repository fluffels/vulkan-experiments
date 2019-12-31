#pragma once

#include "Mesh.h"

class IndexedMesh : public Mesh {
protected:
    /**
      * Constructor. Protected to prevent instantiation except via subclass.
      */
    IndexedMesh();

    /** Amount of indices in the index buffer. */
    size_t _indexCount;
};
