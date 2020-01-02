#pragma once

#include "Mesh.h"

class IndexedMesh : public Mesh {
public:
    /**
     * Get index count.
     */
    virtual unsigned getIndexCount() const;

    /**
     * Get a pointer to the indices.
     */
    virtual const unsigned* getIndices() const = 0;

protected:
    /**
      * Constructor. Protected to prevent instantiation except via subclass.
      */
    IndexedMesh();

    /** Amount of indices in the index buffer. */
    size_t _indexCount;
};
