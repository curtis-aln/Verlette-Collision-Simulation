#include "collision_resolver.h"
#include "collision_vector.h"
#include <cmath>
#include <cstdint>
#include <particle_system/particle.h>
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


CollisionResolver::CollisionResolver(sf::Rect<float>* bounds, o_vector<Entity>* entities,
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

void CollisionResolver::resolve_pair_collision(Entity* particle_a, Entity* particle_b)
{

	float rad_a = particle_a->radius_; // Todo - dynamic radii
	float rad_b = particle_b->radius_;
	const float rad_sum = rad_a + rad_b;

	const sf::Vector2f d = particle_a->position_ - particle_b->position_;
	const float d2 = d.x * d.x + d.y * d.y;
	if (d2 >= rad_sum * rad_sum) return;            // not touching: no sqrt, no work


	float nx, ny, distance;
	if (d2 < 1e-8f) [[unlikely]]
	{
		static thread_local uint32_t s = 0x9E3779B9u;
		const uint32_t k = xorshift32(s) & 7u;       // random axis from table
		nx = kAx[k]; ny = kAy[k]; distance = 0.f;
	}
	else
	{
		distance = std::sqrt(d2);
		const float inv = 1.f / distance;
		nx = d.x * inv; ny = d.y * inv;
	}

	sf::Vector2f direction_normal = d / distance;

	const float local_diam = rad_a + rad_b;
	const float overlap = distance - local_diam;

	const sf::Vector2f collision_resolution = direction_normal * (overlap * 0.5f * correction_factor);
	particle_a->position_ -= collision_resolution;
	particle_b->position_ += collision_resolution;

	// Velocity resolution
	sf::Vector2f vel_a = particle_a->velocity_;
	sf::Vector2f vel_b = particle_b->velocity_;

	float mass_a = rad_a * rad_a * density;
	float mass_b = rad_b * rad_b * density;


	// Each particle gets a share weighted by the *other* particle's mass fraction
	const sf::Vector2f rel_vel = vel_a - vel_b;
	const float rel_vel_n = rel_vel.x * direction_normal.x + rel_vel.y * direction_normal.y;

	if (rel_vel_n > 0.f)
		return;  // positive = separating along A←B axis, skip

	const float impulse_scalar = (1.0f + restitution) * rel_vel_n;
	// rel_vel_n < 0 (approaching), so impulse_scalar < 0 — correct for the -= / += below

	const sf::Vector2f impulse = direction_normal * impulse_scalar;

	const float total_mass = mass_a + mass_b;
	const float inv_total = 1.0f / total_mass;

	const sf::Vector2f resolution_a = impulse * (mass_b * inv_total);
	const sf::Vector2f resolution_b = impulse * (mass_a * inv_total);

	particle_a->velocity_ -= resolution_a;
	particle_b->velocity_ += resolution_b;
}


void CollisionResolver::close_program()
{

}