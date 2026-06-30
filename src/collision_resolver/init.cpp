#include "collision_resolver.h"




void CollisionResolver::init_collision_jobs()
{
	collision_jobs_.clear();

	// Build the list of valid Morton indices up front
	const uint32_t cells_x = grid.CellsX;
	const uint32_t cells_y = grid.CellsY;

	// Total logical cells is still CellsX * CellsY, just not contiguous in memory
	const int total_cells = static_cast<int>(cells_x * cells_y);
	const int chunk = std::max(1, (total_cells + (int)thread_count - 1) / (int)thread_count);

	// Pre-build the Morton index list so threads just index into it
	std::vector<uint32_t> morton_indices;
	morton_indices.reserve(total_cells);
	for (uint32_t cy = 0; cy < cells_y; ++cy)
		for (uint32_t cx = 0; cx < cells_x; ++cx)
			morton_indices.push_back(calcZOrder(static_cast<uint16_t>(cx),
				static_cast<uint16_t>(cy)));

	for (int t = 0; t < (int)thread_count; ++t)
	{
		const int begin = t * chunk;
		if (begin >= total_cells) break;
		const int end = std::min(begin + chunk, total_cells);

		collision_jobs_.emplace_back([this, begin, end, t, morton_indices]
			{
				thread_local FixedSpan<uint32_t> local_nearby_ids = tl_nearby_ids;
				for (int i = begin; i < end; ++i)
					primitive_detect_collisions_for_grid_cell(morton_indices[i], local_nearby_ids, collision_indexes[t]);
			});
	}
}