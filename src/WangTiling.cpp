#include "WangTiling.h"

WangTiling::
WangTiling(int width, int depth):
        _depth(depth),
        _width(width),
        _tilePool(),
        _tiles(NULL) {
   _tiles = new TileType*[getDepth()];

   for (int z = 0; z < getDepth(); z++) {
      _tiles[z] = new TileType[getDepth()];
   }

   initialisePool();
   layTiles();
}

WangTiling::
~WangTiling() {
   for (int z = 0; z < getDepth(); z++) {
      delete[] _tiles[z];
   }

   delete[] _tiles;
   _tiles = NULL;
}

WangTiling::
WangTiling(const WangTiling& rhs):
        _depth(rhs._depth),
        _width(rhs._width),
        _tilePool(rhs._tilePool),
        _tiles(NULL) {
   _tiles = new TileType*[getWidth()];

   for (int z = 0; z < getDepth(); z++) {
      _tiles[z] = new TileType[getWidth()];

      for (int x = 0; x < getWidth(); x++) {
         _tiles[z][x] = rhs._tiles[z][x];
      }
   }
}

const WangTiling& WangTiling::
operator=(const WangTiling& rhs) {
   if (this == &rhs) {
      return *this;
   }

   for (int z = 0; z < getDepth(); z++) {
      delete[] _tiles[z];
   }

   delete[] _tiles;

   _depth = rhs._depth;
   _width = rhs._width;
   _tilePool = rhs._tilePool;

   _tiles = new TileType*[getDepth()];

   for (int z = 0; z < getDepth(); z++) {
      _tiles[z] = new TileType[getWidth()];

      for (int x = 0; x < getWidth(); x++) {
         _tiles[z][x] = rhs._tiles[z][x];
      }
   }

   return *this;
}

int WangTiling::
getDepth() {
   return _depth;
}

WangTile& WangTiling::
getTile(int x, int z) {
   TileType t = _tiles[z][x];
   return getTile(t);
}

WangTile& WangTiling::
getTile(TileType t) {
   return _tilePool[t];
}

const TileType** WangTiling::
getTileArray() {
   return (const TileType**)_tiles;
}

TileType WangTiling::
getTileType(int x, int z) {
   return _tiles[z][x];
}

int WangTiling::
getWidth() {
   return _width;
}

TileType WangTiling::
getRandomType(vec3* left, vec3* top) {
   vector<TileType> options;
   
   for (int a = 0; a < COUNT; a++) {
      TileType t = (TileType)a;
      WangTile w = getTile(t);
      
      bool accept = true;
      
      if ((left != NULL) && (w.getLeft() != *left)) {
         accept = false;
      }
      
      if ((accept) && (top != NULL) && (w.getTop() != *top)) {
         accept = false;
      }
      
      if (accept) {
         options.push_back(t);
      }
   }

   if (options.size() == 0) {
      throw runtime_error("Ran out of Wang tiles.");
   }

   std::random_device rng;
   std::mt19937 urng(rng());
   shuffle(options.begin(), options.end(), urng);
   return options[0];
}

void WangTiling::
setTile(int x, int z, TileType t) {
   _tiles[z][x] = t;
}

void WangTiling::
initialisePool() {
   TileType type[COUNT] = {A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P};
   WangTile tile[COUNT] = {
      WangTile(0, RED, BLUE, RED, BLUE),
      WangTile(1, RED, BLUE, RED, YELLOW),
      WangTile(2, RED, BLUE, GREEN, BLUE),
      WangTile(3, RED, BLUE, GREEN, YELLOW),
      WangTile(4, RED, YELLOW, RED, BLUE),
      WangTile(5, RED, YELLOW, RED, YELLOW),
      WangTile(6, RED, YELLOW, GREEN, BLUE),
      WangTile(7, RED, YELLOW, GREEN, YELLOW),
      WangTile(8, GREEN, BLUE, RED, BLUE),
      WangTile(9, GREEN, BLUE, RED, YELLOW),
      WangTile(10, GREEN, BLUE, GREEN, BLUE),
      WangTile(11, GREEN, BLUE, GREEN, YELLOW),
      WangTile(12, GREEN, YELLOW, RED, BLUE),
      WangTile(13, GREEN, YELLOW, RED, YELLOW),
      WangTile(14, GREEN, YELLOW, GREEN, BLUE),
      WangTile(15, GREEN, YELLOW, GREEN, YELLOW)
   };

   for (int a = 0; a < COUNT; a++) {
      _tilePool[type[a]] = tile[a];
   }
}

void WangTiling::
layTiles() {
   vec3 leftConstraint, topConstraint;

   TileType start = getRandomType(NULL, NULL);
   setTile(0, 0, start);

   for (int x = 1; x < getWidth(); x++) {
      leftConstraint = getTile(x - 1, 0).getRight();
      TileType tile = getRandomType(&leftConstraint, NULL);
      setTile(x, 0, tile);
   }
   
   for (int z = 1; z < getDepth(); z++) {
      topConstraint = getTile(0, z - 1).getBottom();
      TileType tile = getRandomType(NULL, &topConstraint);
      setTile(0, z, tile);

      for (int x = 1; x < getWidth(); x++) {
         topConstraint = getTile(x, z - 1).getBottom();
         leftConstraint = getTile(x - 1, z).getRight();

         tile = getRandomType(&leftConstraint, &topConstraint);
         setTile(x, z, tile);
      }
   }
}
