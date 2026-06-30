#include "collision_resolver.h"




void CollisionResolver::init_collision_jobs()
{
	collision_jobs_.clear();

	// Build the list of valid Morton indices up front
	const uint32_t cells_x = spatial_grid_.CellsX;
	const uint32_t cells_y = spatial_grid_.CellsY;

	// Total logical cells is still CellsX * CellsY, just not contiguous in memory
	const int total_cells = static_cast<int>(cells_x * cells_y);
	const int chunk = std::max(1, (total_cells + (int)thread_count_ - 1) / (int)thread_count_);

	for (int t = 0; t < (int)thread_count_; ++t)
	{
		const int begin = t * chunk;
		if (begin >= total_cells) break;
		const int end = std::min(begin + chunk, total_cells);

		collision_jobs_.emplace_back([this, begin, end, t]{
				for (int i = begin; i < end; ++i)
					primitive_detect_collisions_for_grid_cell(i, collision_indexes_[t]);
			});
	}
}