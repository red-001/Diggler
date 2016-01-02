#ifndef AABOX_HPP
#define AABOX_HPP
#include <vector>
#include <glm/glm.hpp>

namespace Diggler {

///
/// @brief Axis-Aligned Bounding Box.
///
class AABB {
public:
	glm::vec3 v1, v2;
	AABB(const glm::vec3 &vec1 = glm::vec3(), const glm::vec3 &vec2 = glm::vec3());
	
	///
	/// @brief Sets the AABB points.
	/// @param vec1 First point.
	/// @param vec2 Second point.
	/// @note Points coordinates are sorted so that each individual coordinate of vec2 is greater than vec1.
	///
	void set(const glm::vec3 &vec1, const glm::vec3 &vec2);

	///
	/// @brief Checks if a point is in the AABB.
	/// @param point Point to check intersection with.
	/// @returns `true` if point is in AABB, `false` otherwise.
	///
	bool intersects(const glm::vec3 &point) const;

	///
	/// @brief Checks if another AABB intersects this one.
	/// @param other AABB to check intersection with.
	/// @returns `true` if AABB intersects, `false` otherwise.
	///
	bool intersects(const AABB &other) const;
};

class AABBVector : public std::vector<AABB> {
public:
	///
	/// @brief Checks if a point is in the AABB vector.
	/// @param point Point to check intersection with.
	/// @returns `true` if point is in one AABB or more, `false` otherwise.
	///
	bool intersects(const glm::vec3 &point) const;

	///
	/// @brief Checks if an AABB intersects any of this vector.
	/// @param other AABB to check intersection with.
	/// @returns `true` if AABBs intersects, `false` otherwise.
	///
	bool intersects(const AABB &other) const;

	///
	/// @brief Checks if an AABB vector intersects this one.
	/// @param other AABB vector to check intersection with.
	/// @returns `true` if AABBs intersects, `false` otherwise.
	///
	bool intersects(const AABBVector &other) const;
};

}

#endif