#include "Terrain.h"

Terrain::
Terrain(string path) :
        IndexedMesh(),
        COMPONENTS(3),
        SMOOTH_PAS_COUNT(5),
        _vertices(nullptr),
        _normals(nullptr),
        _indices(nullptr),
        _width(0),
        _depth(0),
        _maxHeight(0),
        _terrainWidth(0),
        _terrainDepth(0),
        _heightMap(nullptr) {
    FILE* fp;
    errno_t fopenCode = fopen_s(&fp, path.c_str(), "rb");
    if (fopenCode != 0) {
        char buffer[256];
        errno_t strerrorCode = strerror_s(buffer, fopenCode);
        if (strerrorCode != 0) {
            throw runtime_error(buffer);
        } else {
            throw runtime_error("Could not load heightmap at " + path);
        }
    } else {
        int width = 0;
        int height = 0;
        int channelsInFile = 0;
        int desiredChannels = 1;
        _heightMap = stbi_load_from_file(
            fp,
            &width,
            &height,
            &channelsInFile,
            desiredChannels
        );
        if (_heightMap != NULL) {
            _width = static_cast<unsigned>(width);
            _depth = static_cast<unsigned>(height);
            construct();
        } else {
            const char* errorMessage = stbi_failure_reason();
            throw runtime_error(errorMessage);
        }
    }
}

Terrain::
Terrain(const Terrain &rhs) :
        IndexedMesh(),
        COMPONENTS(3),
        SMOOTH_PAS_COUNT(5),
        _vertices(NULL),
        _normals(NULL),
        _indices(NULL),
        _width(rhs._width),
        _depth(rhs._depth),
        _maxHeight(0),
        _terrainWidth(0),
        _terrainDepth(0),
        _heightMap(rhs._heightMap) {
    construct();
}

const Terrain &Terrain::
operator=(Terrain &rhs) {
    if (this != &rhs) {
        delete[] _vertices;
        delete[] _normals;
        delete[] _indices;

/* TODO(jan): This is unsafe. */
        this->_heightMap = rhs._heightMap;
        construct();
    }

    return *this;
}

float Terrain::
getHeightAt(unsigned x, unsigned z) {
    return getCoord(x, z)[Y];
}

void Terrain::
setHeight(unsigned x, unsigned z, float height) {
    getCoord(x, z)[Y] = height;
}

float Terrain::
getMaxHeight() const {
    return _maxHeight;
}

unsigned Terrain::
getWidth() const {
    return _width;
}

unsigned Terrain::
getDepth() const {
    return _depth;
}

float *Terrain::
getHeightArray() {
    float *result = new float[_vertexCount];
    float *v = getCoord(0, 0);

    for (unsigned i = 0; i < (unsigned) _vertexCount; i++) {
        result[i] = v[Y];
        v += COMPONENTS;
    }

    return result;
}

const float *Terrain::
getPositions() const {
    return _vertices;
}

const float *Terrain::
getNormals() const {
    return _normals;
}

const unsigned *Terrain::
getIndices() const {
    return _indices;
}

void Terrain::
construct() {
    _vertexCount = _width * _depth;

    const unsigned SIZE = (unsigned) _vertexCount * COMPONENTS;
    _vertices = new float[SIZE];
    _normals = new float[SIZE];

    _indexCount = _vertexCount * 6;
    _indices = new unsigned[_indexCount];

    generateVertices();
    for (unsigned i = 0; i < SMOOTH_PAS_COUNT; i++) {
        smoothVertices();
    }
    generateNormals();
    generateIndices();
}

float *Terrain::
getCoord(unsigned x, unsigned z) {
    return _vertices + (z * _width * COMPONENTS) + (x * COMPONENTS);
}

float Terrain::
getPixel(unsigned x, unsigned z) {
    const unsigned index = z * _width + x;
    const unsigned char rawPixel = _heightMap[index];
    const float normalizedPixel = rawPixel / 255.f;
    return normalizedPixel;
}

void Terrain::
generateVertices() {
    const float Z_DELTA = 1.0f;
    const float X_DELTA = 1.0f;

    unsigned index = 0;
    float Zcoord = 0.0f;
    float Xcoord = 0.0f;
    for (unsigned z = 0; z < _depth; z++) {
        Xcoord = 0.0f;
        for (unsigned x = 0; x < _width; x++) {
            float height = getPixel(x, z) * 64.f;
            if (height > _maxHeight) _maxHeight = height;
            _vertices[index++] = Xcoord;
            _vertices[index++] = height;
            _vertices[index++] = Zcoord;
            Xcoord += X_DELTA;
        }
        Zcoord += Z_DELTA;
    }
    _terrainDepth = Zcoord;
    _terrainWidth = Xcoord;
}

void Terrain::
generateNormals() {
    for (unsigned z = 1; z < _depth - 1; z++) {
        unsigned index = (_width * z + 1) * COMPONENTS;
        float *mid = _vertices + index;
        float *top = mid + _width * COMPONENTS;
        float *left = mid - COMPONENTS;
        float *right = mid + COMPONENTS;
        float *bot = mid - _width * COMPONENTS;

        for (unsigned x = 1; x < _width - 1; x++) {
            vec3 m(mid[X], mid[Y], mid[Z]);
            vec3 t(top[X], top[Y], top[Z]);
            vec3 r(right[X], right[Y], right[Z]);
            vec3 l(left[X], left[Y], left[Z]);
            vec3 b(bot[X], bot[Y], bot[Z]);

            vec3 v0 = t - m;
            vec3 v1 = r - m;
            vec3 n = normalize(cross(v0, v1));
            vec3 v2 = l - m;
            n = n + normalize(cross(v2, v0));
            vec3 v3 = b - m;
            n = n + normalize(cross(v3, v2));
            n = n + normalize(cross(v1, v3));
            n /= 4;

            _normals[index++] = n.x;
            _normals[index++] = n.y;
            _normals[index++] = n.z;

            mid += COMPONENTS;
            top += COMPONENTS;
            right += COMPONENTS;
            left += COMPONENTS;
            bot += COMPONENTS;
        }
    }
}

void Terrain::
generateIndices() {
    unsigned index = 0;
    for (unsigned z = 0; z < _depth - 1; z++) {
        for (unsigned x = 0; x < _width - 1; x++) {
            unsigned i = z * _width + x;

            /* Top left square. */
            _indices[index++] = i;
            _indices[index++] = i + _width;
            _indices[index++] = i + 1;

            /* Bottom right square. */
            _indices[index++] = i + 1;
            _indices[index++] = i + _width;
            _indices[index++] = i + 1 + _width;
        }
    }
}

void Terrain::
smoothVertices() {
    const unsigned SAMPLE = 4;
    for (unsigned z = SAMPLE; z < _depth - SAMPLE; z++) {
        float *t = getCoord(SAMPLE, z + 1);
        float *m = getCoord(SAMPLE, z);
        float *b = getCoord(SAMPLE, z - 1);

        for (unsigned x = SAMPLE; x < _width - SAMPLE; x++) {
            float acc = m[Y] + (m - COMPONENTS)[Y] + (m + COMPONENTS)[Y];
            acc += t[Y] + (t - COMPONENTS)[Y] + (t + COMPONENTS)[Y];
            acc += b[Y] + (b - COMPONENTS)[Y] + (b + COMPONENTS)[Y];

            m[Y] = acc / 9;

            t += COMPONENTS;
            m += COMPONENTS;
            b += COMPONENTS;
        }
    }
}
