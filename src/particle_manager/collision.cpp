#include "particle_manager.h"

#include <iostream>


void ParticleManager::init_collision_jobs()
{
	collision_jobs_.clear();

	const int total_cells = grid.CellsX * grid.CellsY;
	const int chunk = std::max(1u, (total_cells + initial_thread_count - 1) / initial_thread_count);

	for (int t = 0; t < initial_thread_count; ++t)
	{
		const int begin = t * chunk;
		if (begin >= total_cells) break;
		const int end = std::min(begin + chunk, total_cells);

		collision_jobs_.emplace_back([this, begin, end, t]
			{
				thread_local FixedSpan<uint32_t> local_nearby_ids = tl_nearby_ids;
				for (int i = begin; i < end; ++i)
					detect_collisions_for_grid_cell(i, local_nearby_ids, collision_indexes[t]);
			});
	}
}

void ParticleManager::run_collision_detection()
{
	collision_thread_pool_.run_and_wait(); // single pass, no colour loop
}


void ParticleManager::detect_collisions_for_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector)
{
	// This function handles all the collision detection for a grid cell, it is far more computationally efficient
	// to collect all the particles around and in this cell into nearby_id's and then for each particle in this cell, check for collisions
	// than it is to go over each particle and re-calculate its nearby neighbours

		if (grid.cell_capacities[grid_cell_id] == 0)
			return;

		nearby_ids.clear();

		const int cell_index_x = grid_cell_id % grid.CellsX;
		const int cell_index_y = grid_cell_id / grid.CellsX;

		const uint8_t self_size = grid.cell_capacities[grid_cell_id];
		const auto* self_contents = &grid.grid[grid_cell_id * grid.cell_max_capacity];

		const float cell_w = grid.cell_width;
		const float cell_h = grid.cell_height;
		const float cell_min_x = cell_index_x * cell_w;
		const float cell_min_y = cell_index_y * cell_h;
		const float cell_max_x = cell_min_x + cell_w;
		const float cell_max_y = cell_min_y + cell_h;

		for (uint8_t idx = 0; idx < self_size; ++idx)
			nearby_ids.add(self_contents[idx]);

		const int self_only_count = nearby_ids.count;

		bool border_flags[cell_max_capacity] = {};
		uint8_t border_count = 0;

		for (uint8_t idx = 0; idx < self_size; ++idx)
		{
			const int pid = self_contents[idx];
			const sf::Vector2f pos = render.positions[pid];
			const float ax = pos.x;
			const float ay = pos.y;
			const float r = render.radii[pid];

			if (!(ax - r >= cell_min_x && ax + r <= cell_max_x &&
				ay - r >= cell_min_y && ay + r <= cell_max_y))
			{
				border_flags[idx] = true;
				++border_count;
			}
		}

		if (border_count > 0)
		{
			update_nearby_container(cell_index_x + 1, cell_index_y, nearby_ids);
			update_nearby_container(cell_index_x - 1, cell_index_y + 1, nearby_ids);
			update_nearby_container(cell_index_x, cell_index_y + 1, nearby_ids);
			update_nearby_container(cell_index_x + 1, cell_index_y + 1, nearby_ids);
		}

		for (uint8_t idx = 0; idx < self_size; ++idx)
		{
			const int pid = self_contents[idx];
			if (border_flags[idx])
				update_protozoa_cell(pid, nearby_ids, collision_vector, -1);
			else
				update_protozoa_cell(pid, nearby_ids, collision_vector, self_only_count);
		}
	}


void ParticleManager::update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids)
{
	// Out of bounds check
	if (neighbour_index_x < 0 || neighbour_index_x >= static_cast<int>(grid.CellsX) ||
		neighbour_index_y < 0 || neighbour_index_y >= static_cast<int>(grid.CellsY))
		return;

	const uint32_t neighbour_index = neighbour_index_y * grid.CellsX + neighbour_index_x;
	const uint8_t size = grid.cell_capacities[neighbour_index];

	// we multiply by grid.cell_max_capacity because the grid is a flat 1D array, and each cell has a fixed capacity
	const auto* contents = &grid.grid[neighbour_index * grid.cell_max_capacity];
	for (uint8_t idx = 0; idx < size; ++idx)
		nearby_ids.add(contents[idx]);
}

void ParticleManager::update_protozoa_cell(
	const int protozoa_cell_index,
	const FixedSpan<uint32_t>&nearby_ids,
	CollisionVector & collision_vector,
	int check_count = -1)  // -1 means check all
{
	const int limit = (check_count < 0) ? nearby_ids.count : check_count;

	const sf::Vector2f pos = render.positions[protozoa_cell_index];
	const float ax = pos.x;
	const float ay = pos.y;
	const float rad_a = render.radii[protozoa_cell_index];
	const float rad_sq = (rad_a + rad_a) * (rad_a + rad_a);


	for (int idx = 0; idx < limit; ++idx)
	{
		const int id = nearby_ids.buffer[idx];

		// Only process forward pairs — eliminates all (b,a) duplicates
		if (id <= protozoa_cell_index) continue;

		const sf::Vector2f other_pos = render.positions[id];
		const float dx = ax - other_pos.x;
		const float dy = ay - other_pos.y;
		const float length_sq = dx * dx + dy * dy;

		const bool colliding = (length_sq < rad_sq && length_sq >= 0.01f);
		if (colliding)
			collision_vector.add(protozoa_cell_index, id);
	}
}

void ParticleManager::handle_collision_resolutions()
{
	//debug_collision_duplicates(); // debugging

	for (CollisionVector& collision_vector : collision_indexes)
	{
		resolve_collision_vector_collisions(collision_vector);
	}
}


void ParticleManager::resolve_collision_vector_collisions(CollisionVector& collision_vector)
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
			particle_a = entities_.at(pair.index_a);
			cached_id = pair.index_a;
		}

		resolve_pair_collision(particle_a, entities_.at(pair.index_b));
	}

	collision_vector.clear();
}


void ParticleManager::resolve_pair_collision(Entity* particle_a, Entity* particle_b)
{

	float rad_a = particle_radius; // Todo - dynamic radii
	float rad_b = particle_radius;

	// Collision resolution
	sf::Vector2f direction = particle_a->position_ - particle_b->position_;

	float distance = direction.length();
	if (distance < 1e-6f) return;
	sf::Vector2f direction_normal = direction / distance;

	const float local_diam = rad_a + rad_b;
	const float overlap = distance - local_diam;

	const float correction_factor = 0.2f;

	const sf::Vector2f collision_resolution = direction_normal * (overlap * 0.5f * correction_factor);
	particle_a->position_ -= collision_resolution;
	particle_b->position_ += collision_resolution;

	// Velocity resolution
	sf::Vector2f vel_a = particle_a->velocity_;
	sf::Vector2f vel_b = particle_b->velocity_;

	float mass_a = rad_a; // Todo - dynamic mass
	float mass_b = rad_b;

	constexpr float restitution = .69f;

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
