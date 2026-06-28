#include "collision_resolver.h"

#include <iostream>



void CollisionResolver::run_collision_detection()
{
	// clearing collision vectors for each thread
	for (auto& collision_vector : collision_indexes)
		collision_vector.clear();

	collision_thread_pool_.run_and_wait(); // single pass, no colour loop
}


void CollisionResolver::primitive_detect_collisions_for_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector)
{
	grid.find_from_index(grid_cell_id, &nearby_ids);

	const uint8_t self_size = grid.cell_capacities[grid_cell_id];
	const auto* self_contents = &grid.grid[grid_cell_id * grid.cell_max_capacity];

	for (uint8_t idx = 0; idx < self_size; ++idx)
	{
		const int pid = self_contents[idx];
		update_protozoa_cell(pid, nearby_ids, collision_vector, -1);
	}
}

void CollisionResolver::detect_collisions_for_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector)
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
			Entity* particle = entities_->at(pid);

			const sf::Vector2f pos = particle->position_;
			const float ax = pos.x;
			const float ay = pos.y;
			const float r = particle->radius_;

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


void CollisionResolver::update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids)
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

void CollisionResolver::update_protozoa_cell(
	const int protozoa_cell_index,
	const FixedSpan<uint32_t>&nearby_ids,
	CollisionVector & collision_vector,
	int check_count = -1)  // -1 means check all
{
	const int limit = (check_count < 0) ? nearby_ids.count : check_count;

	Entity* protozoa_cell = entities_->at(protozoa_cell_index);
	const sf::Vector2f pos = protozoa_cell->position_;
	const float ax = pos.x;
	const float ay = pos.y;
	const float rad_a = protozoa_cell->radius_;


	for (int idx = 0; idx < limit; ++idx)
	{
		const int id = nearby_ids.buffer[idx];

		// Only process forward pairs — eliminates all (b,a) duplicates
		if (id <= protozoa_cell_index) continue;

		Entity* other_cell = entities_->at(id);
		const sf::Vector2f other_pos = other_cell->position_;

		const float rad_b = other_cell->radius_; // Todo - dynamic radii
		const float rad_sq = (rad_a + rad_b) * (rad_a + rad_b);
		const float dx = ax - other_pos.x;
		const float dy = ay - other_pos.y;
		const float length_sq = dx * dx + dy * dy;

		const bool colliding = (length_sq < rad_sq && length_sq >= 0.01f);
		if (colliding)
			collision_vector.add(protozoa_cell_index, id);
	}
}