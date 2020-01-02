#include "IndexedMesh.h"

IndexedMesh::
IndexedMesh(): _indexCount(0) {}

unsigned IndexedMesh::
getIndexCount() const {
    return _indexCount;
}
