#include "collision_resolver.h"
#include "collision_vector.h"
#include <cmath>
#include <cstdint>
#include <particle_system/body.h>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <spatial_grid/fixed_span.h>
#include <spatial_grid/simple_spatial_grid.h>
#include <utilities/o_vector.hpp>

thread_local FixedSpan<packed_entry> CollisionResolver::tl_packed_entries_{ packed_entries_max };

thread_local FixedSpan<UnpackedEntry> CollisionResolver::tl_unpacked_entries_{ packed_entries_max };

namespace
{
	// 8 unit axes, so no normalise is needed when particles are coincident
	constexpr float kAx[8] = { 1.f, 0.70710678f, 0.f, -0.70710678f, -1.f, -0.70710678f,  0.f,  0.70710678f };
	constexpr float kAy[8] = { 0.f, 0.70710678f, 1.f,  0.70710678f,  0.f, -0.70710678f, -1.f, -0.70710678f };

	inline uint32_t xorshift32(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
}

namespace
{
	// maps iteration k to the visited index, reversed on odd frames
	constexpr int visit_index(int k, int size, bool reverse)
	{
		return reverse ? size - 1 - k : k;
	}
}


CollisionResolver::CollisionResolver(sf::Rect<float>* bounds, o_vector<Body>* entities,
	unsigned int init_thread_count, unsigned int max_collisions_per_thread, unsigned int max_particles)
	:
	collision_bodies_(entities), thread_count_(init_thread_count),
	spatial_grid_(cells_x, cells_y, cell_max_capacity, bounds->size.x, bounds->size.y),
	collision_thread_pool_(static_cast<int>(thread_count_)),
	add_to_grid_thread_pool_(static_cast<int>(thread_count_))
{
	spatial_grid_.prev_cells.reserve(max_particles);

	init_collision_jobs();
	collision_indexes_.resize(thread_count_, CollisionVector(max_collisions_per_thread));

	collision_thread_pool_.set_jobs(collision_jobs_);  // once


	spatial_grid_.prev_cells.resize(collision_bodies_->size());
	spatial_grid_.entity_slot.assign(collision_bodies_->size(), 0);
	add_particles_to_grid();
}


void CollisionResolver::handle_collision_resolutions()
{
	const bool reverse = (resolution_frame_ ^= 1);
	const int  n = static_cast<int>(collision_indexes_.size());

	for (int t = 0; t < n; ++t)
		resolve_collision_vector_collisions(collision_indexes_[visit_index(t, n, reverse)], reverse);
}

void CollisionResolver::resolve_collision_vector_collisions(const CollisionVector& cv, bool reverse)
{
	const int size = cv.size();

	for (int k = 0; k < size; ++k)
	{
		const CollisionPair p = cv[visit_index(k, size, reverse)];
		resolve_pair_collision(collision_bodies_->at(p.index_a), collision_bodies_->at(p.index_b));
	}
}

void CollisionResolver::resolve_pair_collision(Body* a, Body* b)
{
	const float inv_mass_a = a->inv_mass();
	const float inv_mass_b = b->inv_mass();
	const float inv_mass_sum = inv_mass_a + inv_mass_b;
	assert(a->get_mass() > 0.f && b->get_mass() > 0.f);   // both massless/recycled: nothing to resolve

	const float contact_dist = a->get_radius() + b->get_radius();
	const sf::Vector2f offset = a->get_pos() - b->get_pos();   // points B -> A
	const float dist_sq = offset.lengthSquared();
	if (dist_sq >= contact_dist * contact_dist) return;

	// One sqrt. Exactly coincident bodies get an arbitrary normal instead of being skipped.
	float dist = std::sqrt(dist_sq);
	sf::Vector2f normal{ 1.f, 0.f };
	if (dist > 1e-6f) normal = offset / dist; else dist = 0.f;

	// Positional correction: the lighter body moves further
	const float penetration = contact_dist - dist;
	const sf::Vector2f correction = normal * (penetration * correction_factor / inv_mass_sum);
	a->translate(correction * inv_mass_a);
	b->translate(-correction * inv_mass_b);

	// Velocity resolution: skip if already separating
	const float closing_speed = (a->get_vel() - b->get_vel()).dot(normal);   // < 0 when approaching
	if (closing_speed >= 0.f) return;

	// J = (1 + e) * |v_n| / (1/m_a + 1/m_b): this is the reduced mass again
	const float j = -(1.f + restitution) * closing_speed / inv_mass_sum;
	const sf::Vector2f impulse = normal * j;

	a->apply_impulse(impulse);    // dv_a = J / m_a
	b->apply_impulse(-impulse);    // dv_b = J / m_b, opposite direction

	a->impulse_ += j;              // equal and opposite, so both bodies took the same magnitude
	b->impulse_ += j;
}


void CollisionResolver::close_program()
{

}