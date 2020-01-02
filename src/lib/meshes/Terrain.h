#pragma once

#include <string>

#include <stb_image.h>

#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <glm/glm.hpp>

#include "easylogging++.h"

#include "../Constants.h"
#include "IndexedMesh.h"

using std::max;
using std::min;
using std::string;
using std::runtime_error;

using glm::cross;
using glm::normalize;
using glm::vec3;

/**
  * Encapsulates a terrain mesh generated from a greyscale height map.
  */
class Terrain : public IndexedMesh {
public:
    /**
      * Constructor, creates the mesh.
      *
      * @param path Path to source height map.
      */
    Terrain(string path);

    /**
      * Copy constructor.
      */
    Terrain(const Terrain &rhs);

    /**
      * Assignment operator.
      */
    const Terrain &operator=(Terrain &rhs);

    /**
      * Get the height at the given heightmap coordinates.
      */
    float getHeightAt(unsigned x, unsigned z);

    /**
      * Get the height field as a one dimensional array.
      */
    float *getHeightArray();

    /**
      * Set the height at the given heightmap coordinates.
      */
    void setHeight(unsigned x, unsigned z, float height);

    /**
     * Get the highest height on the terrain.
     */
    float getMaxHeight() const;

    /**
     * Get the width of the heightmap.
     */
    unsigned getWidth() const;

    /**
     * Get the depth of the heightmap.
     */
    unsigned getDepth() const;

    /**
     * Get a pointer to the positions.
     */
    const float* getPositions() const;

    /**
     * Get a pointer to the normals.
     */
    const float* getNormals() const;

    /**
     * Get a pointer to the indices.
     */
    virtual const unsigned* getIndices() const;

private:
    /** How many components each vertex has. Set to 3 for 3d. */
    const unsigned COMPONENTS;
    /** How many times to smooth the terrain. Set to 5. */
    const unsigned SMOOTH_PAS_COUNT;

    /**
      * Helper constructor.
      */
    void construct();

    /**
      * Get the coordinate that corresponds to the given heightmap
      * coordinates.
      */
    float *getCoord(unsigned x, unsigned z);

    /**
     * Get normalized pixel value at the given coordinates.
     */
    float getPixel(unsigned x, unsigned z);

    /**
      * Generate vertices.
      */
    void generateVertices();

    /**
      * Generate normals.
      */
    void generateNormals();

    /**
      * Generate indices.
      */
    void generateIndices();

    /**
      * Smooth out the quantized vertex data.
      */
    void smoothVertices();

    /** Height map image */
    unsigned char* _heightMap;
    /** Width of the height map image. */
    unsigned _width;
    /** Depth of the height map image. */
    unsigned _depth;
    /** Max height of the terrain. */
    float _maxHeight;
    /** Width of the terrain model. */
    float _terrainWidth;
    /** Depth of the terrain model. */
    float _terrainDepth;
    /** The vertices. */
    float *_vertices;
    /** The normals. */
    float *_normals;
    /** The indices. */
    unsigned *_indices;
};
