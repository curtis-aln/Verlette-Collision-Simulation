#include "collision_resolver.h"

// This function runs the collision detection for all grid cells in parallel using the thread pool
void CollisionResolver::run_collision_detection()
{
	// clearing collision vectors for each thread
	for (auto& collision_vector : collision_indexes_)
		collision_vector.clear();

	collision_thread_pool_.run_and_wait();
}

// This function runs the collision detection for a single grid cell, and adds any detected collisions to the provided collision_vector
void CollisionResolver::primitive_detect_collisions_for_grid_cell(const int grid_cell_id, CollisionVector& collision_vector)
{
	// Filling the packed_entries with all the particles in this cell and the neighbouring cells
	spatial_grid_.find_from_index(grid_cell_id, &tl_unpacked_entries_);

	const uint8_t self_size = spatial_grid_.cell_capacities[grid_cell_id];
	const auto* self_contents = &spatial_grid_.grid[grid_cell_id * spatial_grid_.cell_max_capacity];

	const uint32_t cell_x = mortonToX(grid_cell_id);
	const uint32_t cell_y = mortonToY(grid_cell_id);

	// For each particle in this cell, check for collisions with all nearby particles
	for (uint8_t idx = 0; idx < self_size; ++idx)
	{
		const auto& unpacked = spatial_grid_.unpack_nearby(self_contents[idx], cell_x, cell_y);
		check_collisions_for_body(unpacked, tl_unpacked_entries_, collision_vector, -1);
	}
}

void CollisionResolver::detect_collisions_for_grid_cell(const int grid_cell_id, FixedSpan<packed_entry>& packed_entries, CollisionVector& collision_vector)
{
	// This function handles all the collision detection for a grid cell, it is far more computationally efficient
	// to collect all the particles around and in this cell into packed entries and then for each particle in this cell, check for collisions
	// than it is to go over each particle and re-calculate its nearby neighbours

		if (spatial_grid_.cell_capacities[grid_cell_id] == 0)
			return;

		packed_entries.clear();

		const int cell_index_x = grid_cell_id % spatial_grid_.CellsX;
		const int cell_index_y = grid_cell_id / spatial_grid_.CellsX;

		const uint8_t self_size = spatial_grid_.cell_capacities[grid_cell_id];
		const auto* self_contents = &spatial_grid_.grid[grid_cell_id * spatial_grid_.cell_max_capacity];

		const float cell_w = spatial_grid_.cell_width;
		const float cell_h = spatial_grid_.cell_height;
		const float cell_min_x = cell_index_x * cell_w;
		const float cell_min_y = cell_index_y * cell_h;
		const float cell_max_x = cell_min_x + cell_w;
		const float cell_max_y = cell_min_y + cell_h;

		for (uint8_t idx = 0; idx < self_size; ++idx)
			packed_entries.add(self_contents[idx]);

		const int self_only_count = packed_entries.count;

		bool border_flags[cell_max_capacity] = {};
		uint8_t border_count = 0;

		for (uint8_t idx = 0; idx < self_size; ++idx)
		{
			
		}

		if (border_count > 0)
		{
			update_nearby_container(cell_index_x + 1, cell_index_y, packed_entries);
			update_nearby_container(cell_index_x - 1, cell_index_y + 1, packed_entries);
			update_nearby_container(cell_index_x, cell_index_y + 1, packed_entries);
			update_nearby_container(cell_index_x + 1, cell_index_y + 1, packed_entries);
		}

		for (uint8_t idx = 0; idx < self_size; ++idx)
		{
			const packed_entry pid = self_contents[idx];
			//if (border_flags[idx])
			//	check_collisions_for_body(pid, packed_entries, collision_vector, -1);
			//else
			//	check_collisions_for_body(pid, packed_entries, collision_vector, self_only_count);
		}
	}


void CollisionResolver::update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<packed_entry>& packed_entries)
{
	// Out of bounds check
	if (neighbour_index_x < 0 || neighbour_index_x >= static_cast<int>(spatial_grid_.CellsX) ||
		neighbour_index_y < 0 || neighbour_index_y >= static_cast<int>(spatial_grid_.CellsY))
		return;

	const uint32_t neighbour_index = neighbour_index_y * spatial_grid_.CellsX + neighbour_index_x;
	const uint8_t size = spatial_grid_.cell_capacities[neighbour_index];

	// we multiply by grid.cell_max_capacity because the grid is a flat 1D array, and each cell has a fixed capacity
	const auto* contents = &spatial_grid_.grid[neighbour_index * spatial_grid_.cell_max_capacity];
	for (uint8_t idx = 0; idx < size; ++idx)
		packed_entries.add(contents[idx]);
}

void CollisionResolver::check_collisions_for_body(const UnpackedEntry& self_entry,
	const FixedSpan<UnpackedEntry>& packed_entries,
	CollisionVector& collision_vector,
	int check_count)
{
	const int limit = (check_count < 0) ? packed_entries.count : check_count;

	const uint32_t collision_body_id = self_entry.idx;
	const float ax = self_entry.x;
	const float ay = self_entry.y;
	const float rad_a = self_entry.radius;

	for (int idx = 0; idx < limit; ++idx)
	{
		const UnpackedEntry entry = packed_entries.buffer[idx];

		const uint32_t id = entry.idx;
		const float other_x = entry.x;
		const float other_y = entry.y;


		// Only process forward pairs
		if (id <= collision_body_id)
			continue;

		const float dx = ax - other_x;
		const float dy = ay - other_y;

		const float rad_b = entry.radius;

		const float radius_sum = rad_a + rad_b;
		const float length_sq = dx * dx + dy * dy;

		if (length_sq < radius_sum * radius_sum && length_sq >= 0.01f)
		{
			collision_vector.add(collision_body_id, id);
		}
	}
}