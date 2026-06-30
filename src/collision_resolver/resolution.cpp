#include "collision_resolver.h"
#include "../utilities/random.h"
#include <spatial_grid/simple_spatial_grid.h>

thread_local FixedSpan<uint32_t> CollisionResolver::tl_nearby_ids_{ nearby_ids_max };

CollisionResolver::CollisionResolver(sf::Rect<float>* bounds, o_vector<Entity>* entities, 
	unsigned int init_thread_count, unsigned int max_collisions_per_thread, unsigned int max_particles)
	: 
	collision_bodies_(entities), thread_count_(init_thread_count),
	spatial_grid_(cells_x, cells_y, cell_max_capacity, bounds->size.x, bounds->size.y),
	collision_thread_pool_( static_cast<int>(thread_count_)),
	add_to_grid_thread_pool_( static_cast<int>(thread_count_))
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
	//debug_collision_duplicates(); // debugging

	for (CollisionVector& collision_vector : collision_indexes_)
	{
		resolve_collision_vector_collisions(collision_vector);
	}
}

void CollisionResolver::resolve_collision_vector_collisions(CollisionVector& collision_vector)
{
	const int size = collision_vector.size();
	if (size == 0)
		return;

	Entity* particle_a = nullptr;
	int cached_id = -1;

	for (CollisionPair pair : collision_vector)
	{
		if (pair.index_a != cached_id)
		{
			particle_a = collision_bodies_->at(pair.index_a);
			cached_id = pair.index_a;
		}

		resolve_pair_collision(particle_a, collision_bodies_->at(pair.index_b));
	}
}


void CollisionResolver::resolve_pair_collision(Entity* particle_a, Entity* particle_b)
{

	float rad_a = particle_a->radius_; // Todo - dynamic radii
	float rad_b = particle_b->radius_;

	// Collision resolution
	sf::Vector2f direction = particle_a->position_ - particle_b->position_;

	float distance = direction.length();
	if (distance < 1e-6f) return;
	sf::Vector2f direction_normal = direction / distance;

	const float local_diam = rad_a + rad_b;
	const float overlap = distance - local_diam;

	const sf::Vector2f collision_resolution = direction_normal * (overlap * 0.5f * correction_factor);
	particle_a->position_ -= collision_resolution;
	particle_b->position_ += collision_resolution;

	// Velocity resolution
	sf::Vector2f vel_a = particle_a->velocity_;
	sf::Vector2f vel_b = particle_b->velocity_;

	float mass_a = rad_a; // Todo - dynamic mass
	float mass_b = rad_b;



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