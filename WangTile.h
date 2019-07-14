#pragma once

#include <glm/glm.hpp>

#include "Constants.h"

using glm::vec3;

/**
  * Stores the four sides of a Wang tile.
  */
class WangTile {
   public:
      /**
        * Default constructor. Required by STL containers. Sets every colour to
        * red.
        */
      WangTile();

      /**
        * Constructor.
        *
        * @param id A unique 8-bit integer identifier for this wang tile.
        * @param top The top colour.
        * @param left The left colour.
        * @param bottom The bottom colour;
        * @param right The right colour.
        */
      WangTile(uint8_t id, vec3 top, vec3 left, vec3 bottom, vec3 right);

      /**
        * @return id.
        */
      uint8_t getID();

      /**
        * @return bottom colour.
        */
      vec3 getBottom();

      /**
        * @return Left colour.
        */
      vec3 getLeft();

      /**
        * @return Right colour.
        */
      vec3 getRight();

      /**
        * @return top colour.
        */
      vec3 getTop();

      /**
        * @param bottom The new bottom colour.
        */
      void setBottom(vec3 bottom);

      /**
        * @param left The new left colour.
        */
      void setLeft(vec3 left);

      /**
        * @param right The new right colour.
        */
      void setRight(vec3 right);

      /**
        * @param top The new top colour.
        */
      void setTop(vec3 top);

   private:
      /** Integer identifier. */
      uint8_t _id;
      /** Bottom colour. */
      vec3 _bottom;
      /** Left colour. */
      vec3 _left;
      /** Right colour. */
      vec3 _right;
      /** Top colour. */
      vec3 _top;
};
