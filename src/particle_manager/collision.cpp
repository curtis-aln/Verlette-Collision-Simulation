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
						update_cells_in_grid_cell(group[i], local_nearby_ids, t);
				});
		}
	}
	colour_job_boundaries_.push_back(static_cast<int>(collision_jobs_.size()));
}

// collision_resolution.cpp
void ParticleManager::update_cells_in_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, const int thread_id)
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
		update_protozoa_cell(cell_contents[idx], nearby_ids, thread_id);
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

void ParticleManager::update_protozoa_cell(const int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids, const int thread_id)
{
	// Fetching information on the Focus particle
	float pos_a_x = render.positions_x[protozoa_cell_index];
	float pos_a_y = render.positions_y[protozoa_cell_index];
	sf::Vector2f pos_a = sf::Vector2f(pos_a_x, pos_a_y);

	const float rad_a = render.radii[protozoa_cell_index];
	const float mass_a = rad_a; // mass ∝ radius

	// For each particle which is nearby this one
	for (const uint32_t id : nearby_ids)
	{
		if (protozoa_cell_index == id)
			continue;

		// Calculate the distance between the two particles
		const sf::Vector2f pos_b = sf::Vector2f(render.positions_x[id], render.positions_y[id]);
		const sf::Vector2f diff = pos_a - pos_b;
		const float diff_x = diff.x;
		const float diff_y = diff.y;

		const float dist_sq = diff_x * diff_x + diff_y * diff_y;
		const float rad_b = render.radii[id];
		const float local_diam = rad_a + rad_b;

		// if the distance is greater than the sum of the radii, they are not colliding
		if (dist_sq > local_diam * local_diam)
			continue;

		// if the distance is zero, we have a problem, so skip this collision
		const float dist = std::sqrt(dist_sq);
		if (dist == 0.0f)
			continue;

		// Positional resolution
		const float overlap = local_diam - dist;
		const sf::Vector2f normal = sf::Vector2f(diff_x, diff_y) / dist;
		const float correction_factor = 0.3f;
		const sf::Vector2f collision = normal * (overlap * 0.5f * correction_factor);

		collision_resolutions[protozoa_cell_index] += collision;
		collision_resolutions[id] -= collision;

		//Velocity resolution (inelastic impulse)
		// if the velocity is too small we neglect it
		const sf::Vector2f vel_a = entity_velocities_[protozoa_cell_index];
		const sf::Vector2f vel_b = entity_velocities_[id];

		const float too_slow = 4.f;
		if (std::abs(vel_a.x) < too_slow && std::abs(vel_a.y) < too_slow &&
			std::abs(vel_b.x) < too_slow && std::abs(vel_b.y) < too_slow)
			continue;

		const float mass_b = rad_b;
		const float total_mass = mass_a + mass_b;
		const float inv_total = 1.0f / total_mass;

		// Relative velocity along the collision normal

		const float rel_vel_n = (vel_a - vel_b).x * normal.x + (vel_a - vel_b).y * normal.y;

		// Only resolve if cells are approaching
		if (rel_vel_n >= 0.0f)
			continue;

		// Coefficient of restitution < 1 → inelastic
		constexpr float restitution = 0.1f;

		// Each particle gets a share weighted by the *other* particle's mass fraction
		const float overlap_ratio = overlap / local_diam; // 0..1
		const float impulse_scalar = -(1.0f + restitution) * rel_vel_n * overlap_ratio;

		const sf::Vector2f impulse = normal * impulse_scalar;

		const sf::Vector2f resolution_a = impulse * (mass_b * inv_total);
		const sf::Vector2f resolution_b = impulse * (mass_a * inv_total);

		velocity_resolutions[protozoa_cell_index] += resolution_a * 0.5f;
		velocity_resolutions[id] -= resolution_b * 0.5f;
	}
}

void ParticleManager::calculate_collision_resolutions()
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
