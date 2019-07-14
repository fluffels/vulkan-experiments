#include "WangTile.h"

WangTile::
WangTile():
        _id(0),
        _bottom(RED),
        _left(RED),
        _right(RED),
        _top(RED) { }

WangTile::
WangTile(uint8_t id, vec3 top, vec3 left, vec3 bottom, vec3 right):
        _id(id),
        _bottom(bottom),
        _left(left),
        _right(right),
        _top(top) { }

uint8_t WangTile::
getID() {
    return _id;
};

vec3 WangTile::
getBottom() {
   return _bottom;
}

vec3 WangTile::
getLeft() {
   return _left;
}

vec3 WangTile::
getRight() {
   return _right;
}

vec3 WangTile::
getTop() {
   return _top;
}

void WangTile::
setBottom(vec3 bottom) {
   _bottom = bottom;
}

void WangTile::
setLeft(vec3 left) {
   _left = left;
}

void WangTile::
setRight(vec3 right) {
   _right = right;
}

void WangTile::
setTop(vec3 top) {
   _top = top;
}
