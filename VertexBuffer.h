#pragma once

struct Vertex {
    glm::vec3 pos;
};

class VertexBuffer {
public:
	VertexBuffer(const std::vector<Vertex>& vertices) {
		auto size = vector_size(vertices);

	}
};
