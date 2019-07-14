#pragma once

#include <glm/glm.hpp>

using glm::vec3;

/**
  * This file contains constants that are global over the whole project.
  */

/** The colour red. */
static const vec3 RED(1.0f, 0.0f, 0.0f);
/** The colour green. */
static const vec3 GREEN(0.0f, 1.0f, 0.0f);
/** The colour blue. */
static const vec3 BLUE(0.0f, 0.0f, 1.0f);
/** The colour yellow. */
static const vec3 YELLOW(0.0f, 1.0f, 1.0f);
/** The colour gray. */
static const vec3 GRAY(0.5f, 0.5f, 0.5f);

/** The value of X, when used as an index. */
static const int X = 0;
/** The value of Y, when used as an index. */
static const int Y = 1;
/** The value of Z, when used as an index. */
static const int Z = 2;
/** The value of W, when used as an index. */
static const int W = 3;

/** Pi. */
static const float PI = 3.141592653589793f;
