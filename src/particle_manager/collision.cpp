#include "particle_manager.h"

#include <iostream>

void ParticleManager::build_color_groups()
{
	for (auto& g : collision_color_groups) g.clear();

	for (int y = 0; y < grid.CellsY; ++y)
		for (int x = 0; x < grid.CellsX; ++x)
		{
			const int color = (x % 3) + 3 * (y % 2);
			const int cell_id = y * grid.CellsX + x;
			collision_color_groups[color].push_back(cell_id);
		}
}

void ParticleManager::init_collision_jobs()
{
	build_color_groups();
	collision_jobs_.clear();

	int updating_threads = ParticleSettings::initial_thread_count;

	int thread_id = 0;
	for (auto& group : collision_color_groups)
	{
		const int group_size = static_cast<int>(group.size());
		const int chunk = std::max(1, (group_size + updating_threads - 1) / updating_threads);

		colour_job_boundaries_.push_back(static_cast<int>(collision_jobs_.size()));

		for (int t = 0; t < updating_threads; ++t)
		{
			const int begin = t * chunk;
			if (begin >= group_size) break;
			const int end = std::min(begin + chunk, group_size);

			collision_jobs_.emplace_back([this, &group, begin, end, t]  // capture t as thread id
				{
					thread_local FixedSpan<uint32_t> local_nearby_ids = tl_nearby_ids;
					for (int i = begin; i < end; ++i)
						update_cells_in_grid_cell(group[i], local_nearby_ids, collision_indexes[t]);
				});
		}
	}
	colour_job_boundaries_.push_back(static_cast<int>(collision_jobs_.size()));
}

// collision_resolution.cpp
void ParticleManager::update_cells_in_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector)
{
	if (grid.cell_capacities[grid_cell_id] == 0)
		return;

	nearby_ids.clear();
	int neighbours_size = 0;

	const int cell_index_x = grid_cell_id % grid.CellsX;
	const int cell_index_y = grid_cell_id / grid.CellsX;

	// Current cell
	update_nearby_container(cell_index_x, cell_index_y, nearby_ids);

	// Right
	update_nearby_container(cell_index_x + 1, cell_index_y, nearby_ids);

	// Bottom-left
	update_nearby_container(cell_index_x - 1, cell_index_y + 1, nearby_ids);

	// Bottom
	update_nearby_container(cell_index_x, cell_index_y + 1, nearby_ids);

	// Bottom-right
	update_nearby_container(cell_index_x + 1, cell_index_y + 1, nearby_ids);

	// Left
	update_nearby_container(cell_index_x - 1, cell_index_y, nearby_ids);

	// Top-left
	update_nearby_container(cell_index_x - 1, cell_index_y - 1, nearby_ids);

	// Top
	update_nearby_container(cell_index_x, cell_index_y - 1, nearby_ids);

	// Top-right
	update_nearby_container(cell_index_x + 1, cell_index_y - 1, nearby_ids);

	const uint8_t cell_size = grid.cell_capacities[grid_cell_id];
	const obj_idx* cell_contents = &grid.grid[grid_cell_id * grid.cell_max_capacity];

	for (uint8_t idx = 0; idx < cell_size; ++idx)
		update_protozoa_cell(cell_contents[idx], nearby_ids, collision_vector);
}


void ParticleManager::update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids)
{
	// Out of bounds check, no wrapping needed
	if (neighbour_index_x < 0 || neighbour_index_x >= static_cast<int>(grid.CellsX) ||
		neighbour_index_y < 0 || neighbour_index_y >= static_cast<int>(grid.CellsY))
		return;

	const uint32_t neighbour_index = neighbour_index_y * grid.CellsX + neighbour_index_x;
	const uint8_t size = grid.cell_capacities[neighbour_index];


	const auto* contents = &grid.grid[neighbour_index * grid.cell_max_capacity];
	for (uint8_t idx = 0; idx < size; ++idx)
		nearby_ids.add(contents[idx]);
}

void ParticleManager::update_protozoa_cell(const int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector)
{
	// Fetching information on the Focus particle
	sf::Vector2f position_a = {
		render.positions_x[protozoa_cell_index],
		render.positions_y[protozoa_cell_index] };
	const float rad_a = render.radii[protozoa_cell_index];

	// For each particle which is nearby this one
	for (const uint32_t id : nearby_ids)
	{
		if (protozoa_cell_index == id)
			continue;

		// Calculate the distance between the two particles
		const sf::Vector2f position_b = sf::Vector2f{ 
			render.positions_x[id], render.positions_y[id] };
		const float rad_b = render.radii[id];

		const float length_sq = (position_a - position_b).lengthSquared();
		const float rad_sq = (rad_a + rad_b) * (rad_a + rad_b);

		// if the two particle are not colliding or they are the same particle, continue
		if (length_sq >= rad_sq || protozoa_cell_index == id || length_sq < 0.01)
			continue;

		// append this collision to the collision resolutions vector
		collision_vector.add(protozoa_cell_index, id);
	}
}

void ParticleManager::handle_collision_resolutions()
{
	// Processing collisions sequentually avoids data races in the collision detection section of the code
	// Todo - Computations can be halfed here somehow
	for (CollisionVector& collision_vector : collision_indexes)
	{
		for (int i = 0; i < collision_vector.size_; ++i)
		{
			// Retriving the pair that was
			CollisionVector::CollisionPair& pair = collision_vector.collision_pairs_[i];
			resolve_pair_collision(pair.first, pair.second);
		}
		
		// Clearing the vector for the next iteration
		collision_vector.clear();
	}
}

void ParticleManager::resolve_pair_collision(int particle_a_index, int particle_b_index)
{
	// Retriving the relevant particle data
	Entity* particle_a = entities_.at(particle_a_index);
	Entity* particle_b = entities_.at(particle_b_index);

	float rad_a = particle_radius; // Todo - dynamic radii
	float rad_b = particle_radius;

	// Collision resolution
	sf::Vector2f direction = particle_a->position_ - particle_b->position_;
	sf::Vector2f direction_normal = direction.normalized();
	float distance = direction.length();

	const float local_diam = rad_a + rad_b;
	const float overlap = distance - local_diam;

	const float correction_factor = 0.1f;

	const sf::Vector2f collision_resolution = direction_normal * (overlap * 0.5f * correction_factor);
	particle_a->position_ -= collision_resolution;
	particle_b->position_ += collision_resolution;

	// Velocity resolution
	sf::Vector2f vel_a = particle_a->velocity_;
	sf::Vector2f vel_b = particle_b->velocity_;

	float mass_a = rad_a; // Todo - dynamic mass
	float mass_b = rad_b;

	constexpr float restitution = .899f;

	// Each particle gets a share weighted by the *other* particle's mass fraction
	const sf::Vector2f rel_vel = vel_a = vel_b;
	const float rel_vel_n = rel_vel.x * direction_normal.x + rel_vel.y * direction_normal.y;

	const float overlap_ratio = overlap / local_diam; // 0..1
	const float impulse_scalar = -(1.0f + restitution) * rel_vel_n * overlap_ratio;

	const sf::Vector2f impulse = direction_normal * impulse_scalar;

	const float total_mass = mass_a + mass_b;
	const float inv_total = 1.0f / total_mass;

	const sf::Vector2f resolution_a = impulse * (mass_b * inv_total);
	const sf::Vector2f resolution_b = impulse * (mass_a * inv_total);

	particle_a->velocity_ += resolution_a * 0.5f;
	particle_b->velocity_ -= resolution_b * 0.5f;
}

void ParticleManager::run_collision_detection()
{
	//if (!toggles.toggle_collisions) return;

	// Zero the thread buffers once at the start
	const int entity_count = stats.cell_particle_count;

	for (int c = 0; c < static_cast<int>(colour_job_boundaries_.size()) - 1; ++c)
	{
		const int begin = colour_job_boundaries_[c];
		const int end = colour_job_boundaries_[c + 1];
		collision_thread_pool_.assign_jobs({ collision_jobs_.begin() + begin,
											 collision_jobs_.begin() + end });
		collision_thread_pool_.run_and_wait();
	}
}
