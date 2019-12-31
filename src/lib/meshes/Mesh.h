#pragma once

class Mesh {
public:
    /**
      * Destructor. Deallocates memory.
      */
    virtual ~Mesh();

protected:
    /**
      * Constructor. Protected to prevent instantiation except via subclass.
      */
    Mesh();

    /**
      * Amount of vertices in this mesh.
      */
    size_t _vertexCount;
};
