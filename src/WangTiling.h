#pragma once

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "lib/Constants.h"
#include "WangTile.h"

using namespace std;
using glm::vec3;

/**
  * Encodes the 16 Wang tiles needed for an aperiodic tiling.
  */
enum TileType {
   A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, COUNT
};

/**
  * Encapsulates a Wang tiling.
  */
class WangTiling {
   public:
      /**
        * Constructor, allocates memory.
        *
        * @param width The width of the grid to tile.
        * @param depth The depth of the gird to tile.
        */
      WangTiling(int width, int depth);

      /**
        * Destructor, frees memory.
        */
      ~WangTiling();

      /**
        * Copy constructor.
        *
        * @param rhs The WangTiling to copy from.
        */
      WangTiling(const WangTiling& rhs);

      /**
        * Assignment operator.
        *
        * @param rhs The WangTiling to copy from.
        * @return This WangTiling.
        */
      const WangTiling& operator=(const WangTiling& rhs);
      
      /**
        * Get the tile at some grid coordinate.
        *
        * @param x The x coordinate.
        * @param z The z coordinate.
        * @return The tile.
        */
      WangTile& getTile(int x, int z);
      
      /**
        * Get the Wang tile associated with the specified tiletype.
        * @param t The type.
        * @return The Wang tile.
        */
      WangTile& getTile(TileType t);

      /**
        * Get the tiling as a one dimensional array.
        */
      const TileType** getTileArray();
      
      /**
        * @return The depth of the grid that's being tiled.
        */
      int getDepth();

      /**
        * @return The width of the grid that's being tiled.
        */
      int getWidth();
      
   private:
      /** Depth of the grid to tile. */
      int _depth;

      /** Width of the grid to tile. */
      int _width;
      
      /** Map between tile types and wang tiles. */
      map<TileType, WangTile> _tilePool;
   
      /** The actual tiles. */
      TileType** _tiles;
      
      /**
        * Get the tile type at a grid coordinate.
        * @param x The x coordinate.
        * @param z The z coordinate.
        * @return The tile type.
        */
      TileType getTileType(int x, int z);
      
      /**
        * Get a random tile type given the constrains imposed by the tile to
        * the left, and the tile to the top.
        *
        * @param left The left tile's colour, or NULL for no constraint.
        * @param top The top tile's colour, or NULL for no constraint.
        * @return The tile type.
        */
      TileType getRandomType(vec3* left, vec3* top);
      
      /**
        * Initializes the tile pool.
        */
      void initialisePool();

      /**
        * Performs the tiling.
        */
      void layTiles();
      
      /**
        * Set a tile at some grid coordinate to a type.
        * @param x The x coordinate.
        * @param z The z coordinate.
        * @param t The type.
        */
      void setTile(int x, int z, TileType t);
};
