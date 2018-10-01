#include "WalkMesh.hpp"

#include "read_chunk.hpp"

#include <glm/glm.hpp>
#include <fstream>
#include <iostream>
#include <string>

WalkMesh::WalkMesh(std::string const &wok_filename) {
	std::ifstream file(wok_filename, std::ios::binary);


	//read vertex data
	struct VertexEntry {
		glm::vec3 position;
		glm::vec3 normal;
	};
	static_assert(sizeof(VertexEntry) == 3 * 4 + 3 * 4, "Vertex is packed");

	std::vector<VertexEntry> data;

	std::cerr << "Start reading vec0" << std::endl;
	read_chunk(file, "vex0", &data);
	std::cerr << "Finished reading vec0" << std::endl;

	for (auto vertex : data) {
		this->vertices.emplace_back(vertex.position);
		this->vertex_normals.emplace_back(vertex.normal);
	}

    static_assert(sizeof(glm::uvec3) == 3*4 ,"Triangle is packed");

    read_chunk(file, "tri0", &this->triangles);
	std::cerr << "Finished reading tri0" << std::endl;

    for (auto triangle : triangles) {
        auto a = triangle[0];
		auto b = triangle[1];
		auto c = triangle[2];

		next_vertex[glm::uvec2(a, b)] = c;
		next_vertex[glm::uvec2(b, c)] = a;
		next_vertex[glm::uvec2(c, a)] = b;
    }
}

// Referenced from https://www.gamedev.net/forums/topic/552906-closest-point-on-triangle/
// https://www.geometrictools.com/Documentation/DistancePoint3Triangle3.pdf
glm::vec3 closestPointOnTriangle(const glm::vec3& vertex_a, const glm::vec3& vertex_b, const glm::vec3& vertex_c, const glm::vec3 postion) {

	glm::vec3 edge0 = vertex_b - vertex_a;
	glm::vec3 edge1 = vertex_c - vertex_a;
	glm::vec3 v0 = vertex_a - postion;

	float a = glm::dot(edge0, edge0);
	float b = glm::dot(edge0, edge1);
	float c = glm::dot(edge1, edge1);
	float d = glm::dot(edge0, v0);
	float e = glm::dot(edge1, v0);

	float det = a * c - b * b;
	float s = b * e - c * d;
	float t = b * d - a * e;

	if (s + t < det) {
		if (s < 0.f) {
			if (t < 0.f) {
				if (d < 0.f) {
				    s = glm::clamp(-d/a, 0.f, 1.f);
				    t = 0.f;
				} else {
					s = 0.f;
					t = glm::clamp(-e/c, 0.f, 1.f);
				}
			} else {
				s = 0.f;
				t = glm::clamp(-e/c, 0.f, 1.f);
			}
		} else if(t < 0.f){
		    s = glm::clamp(-d/a, 0.f, 1.f);
		    t = 0.f;
		} else {
			float invDet = 1.f / det;
			s *= invDet;
			t *= invDet;
		}
	} else {
		if (s < 0.f) {
		    float tmp0 = b + d;
		    float tmp1 = c + e;
		    if (tmp1 > tmp0) {
		    	float number = tmp1 - tmp0;
		    	float denom = a - 2 * b + c;
		    	s = glm::clamp(number / denom, 0.f, 1.f);
		    	t = 1-s;
		    } else {
		        t = glm::clamp(-e/c, 0.f, 1.f);
		        s = 0.f;
		    }
		} else if (t < 0.f){
		    if (a + d > b + e) {
		    	float number = c + e - b - d;
		    	float denom = a - 2 * b + c;
		    	s = glm::clamp(number/denom, 0.f, 1.f);
		    	t = 1-s;
		    } else {
		    	s = glm::clamp(-e/c, 0.f, 1.f);
		    	t = 0.f;
		    }
		} else {
			float number = c + e - b -d;
			float denom = a - 2 * b + c;
			s = glm::clamp(number / denom, 0.f, 1.f);
			t = 1.f-s;
		}
	}

	return vertex_a + s * edge0 + t * edge1;
}


// Referenced from: https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
// http://realtimecollisiondetection.net/
glm::vec3 calculate_barycentric_coord(glm::vec3 vertex_a, glm::vec3 vertex_b, glm::vec3 vertex_c, glm::vec3 point) {
    glm::vec3 edge0 = vertex_b - vertex_a;
    glm::vec3 edge1 = vertex_c - vertex_a;
    glm::vec3 v0 = point - vertex_a;

    float d00 = glm::dot(edge0, edge0);
    float d01 = glm::dot(edge0, edge1);
    float d11 = glm::dot(edge1, edge1);
    float d20 = glm::dot(v0, edge0);
    float d21 = glm::dot(v0, edge1);

    float denom = d00 * d11 - d01 * d01;

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    return glm::vec3(u,v,w);
}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
	WalkPoint closest;

	float min_distance = MAXFLOAT;
	for (auto &triangle : this->triangles) {

		const glm::vec3 &vertex_a = this->vertices[triangle[0]];
		const glm::vec3 &vertex_b = this->vertices[triangle[1]];
		const glm::vec3 &vertex_c = this->vertices[triangle[2]];

	    glm::vec3 closest_point = closestPointOnTriangle(vertex_a, vertex_b, vertex_c, world_point);

	    float distance = glm::distance(closest_point, world_point);

	    if (distance < min_distance) {
	    	min_distance = distance;
	    	closest.triangle = triangle;
	    	closest.weights = calculate_barycentric_coord(vertex_a, vertex_b, vertex_c, closest_point);
	    }
	}

	return closest;
}


void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
	glm::vec3 weights_step;

	auto &vertex_a = this->vertices[wp.triangle[0]];
	auto &vertex_b = this->vertices[wp.triangle[1]];
	auto &vertex_c = this->vertices[wp.triangle[2]];

	auto old_point= vertex_a * wp.weights.x + vertex_b * wp.weights.y + vertex_c * wp.weights.z;
	auto new_point = old_point + step;

	auto barycoord  = calculate_barycentric_coord(vertex_a, vertex_b, vertex_c, new_point);

	weights_step = barycoord - wp.weights;

	float t = 0.0f;

	glm::vec3 sum_weight = wp.weights;

	while ((sum_weight.x > -0.0001 && sum_weight.y > -0.0001 && sum_weight.z > -0.0001)  && t < 1.0f) {
		t += 0.1;
	    sum_weight = wp.weights + t * weights_step;
	}

	if (sum_weight.x <= 0 && sum_weight.y <= 0) {
		sum_weight.x = 0.1;
	}
	else if (sum_weight.x <= 0 && sum_weight.z <= 0) {
		sum_weight.x = 0.1;
	}
	else if (sum_weight.y <= 0 && sum_weight.z <= 0) {
		sum_weight.z = 0.1;
	}

	if (t >= 1.0f) { //if a triangle edge is not crossed
		wp.weights += weights_step;
	} else { //if a triangle edge is crossed

		uint32_t edge_a;
		uint32_t edge_b;
		uint32_t oppo_vertex;

		if (sum_weight.x <= 0) {
			sum_weight.x = 0;
			edge_a = wp.triangle.z;
			edge_b = wp.triangle.y;
			oppo_vertex = wp.triangle.x;
		}
		if (sum_weight.y <= 0) {
			sum_weight.y = 0;
			edge_a = wp.triangle.x;
			edge_b = wp.triangle.z;
			oppo_vertex = wp.triangle.y;
		}
		if (sum_weight.z <= 0) {
			sum_weight.z = 0;
			edge_a = wp.triangle.y;
			edge_b = wp.triangle.x;
			oppo_vertex = wp.triangle.z;
		}



		std::unordered_map<glm::uvec2, uint32_t >::const_iterator got = next_vertex.find(glm::uvec2(edge_a, edge_b));

		if (got != next_vertex.end()) {
			sum_weight += weights_step * 0.2f;

			auto &vertex_a = this->vertices[wp.triangle[0]];
			auto &vertex_b = this->vertices[wp.triangle[1]];
			auto &vertex_c = this->vertices[wp.triangle[2]];

			auto new_point = sum_weight.x * vertex_a + sum_weight.y * vertex_b + sum_weight.z * vertex_c;

			wp.triangle[0] = edge_a;
		    wp.triangle[1] = edge_b;
		    wp.triangle[2] = got->second;


			auto &vertex2_a = this->vertices[wp.triangle[0]];
			auto &vertex2_b = this->vertices[wp.triangle[1]];
			auto &vertex2_c = this->vertices[wp.triangle[2]];


			auto project_point = closestPointOnTriangle(vertex2_a, vertex2_b, vertex2_c, new_point);
			auto barycoord  = calculate_barycentric_coord(vertex2_a, vertex2_b, vertex2_c, project_point);

			wp.weights = barycoord;

		} else {
		    wp.weights = wp.weights + 0.02f * weights_step;
		}
	}
}
