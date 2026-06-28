#include "collision_resolver.h"
#include "../utilities/random.h"
#include <spatial_grid/simple_spatial_grid.h>

thread_local FixedSpan<uint32_t> CollisionResolver::tl_nearby_ids{ nearby_ids_max };

CollisionResolver::CollisionResolver(sf::RenderWindow* window, sf::Rect<float>* bounds, o_vector<Entity>* entities)
	: window_(window), bounds_(bounds), entities_(entities)
{
    grid.prev_cells.reserve(maximum_particle_count);

	init_collision_jobs();
	collision_indexes.resize(initial_thread_count, CollisionVector(maximum_particle_count / initial_thread_count));

	collision_thread_pool_.set_jobs(collision_jobs_);  // once

    
    grid.prev_cells.resize(entities_->size());
    add_particles_to_grid();
}


void CollisionResolver::add_particles_to_grid()
{
	const int n = entities_->size();
	const int frame_parity = resolution_frame_ & 1;

	grid.clear();
    grid.prev_cells.resize(entities_->size());

	int i = 0;
	for (Entity* entity : *entities_)
	{
		const sf::Vector2f pos = entity->position_;

		// only add this particle to the grid if it matches this frame's parity
		//if ((i & 1) == frame_parity)
		grid.add_object(pos.x, pos.y, i);

		++i;
	}
}

void CollisionResolver::update_particles_grid_indexes()
{
    grid.prev_cells.resize(entities_->size(), 0);

    for (int obj_id = 0; obj_id < static_cast<int>(entities_->size()); ++obj_id)
    {
        const Entity* entity = entities_->at(obj_id);
        const sf::Vector2f pos = entity->position_;

        const cell_idx new_cell = grid.hash(pos.x, pos.y);
        const cell_idx old_cell = grid.prev_cells[obj_id];

        if (new_cell == old_cell) continue;

        // Remove from old cell by swapping with the last entry
        uint8_t& old_cap = grid.cell_capacities[old_cell];
        obj_idx* old_data = &grid.grid[old_cell * grid.cell_max_capacity];
        for (uint8_t i = 0; i < old_cap; ++i)
        {
            if (old_data[i] == static_cast<obj_idx>(obj_id))
            {
                old_data[i] = old_data[--old_cap]; // fill gap with last element
                break;
            }
        }

        // Insert into new cell
        uint8_t& new_cap = grid.cell_capacities[new_cell];
        if (new_cap < grid.cell_max_capacity)
            grid.grid[new_cell * grid.cell_max_capacity + new_cap++] = static_cast<obj_idx>(obj_id);

        grid.prev_cells[obj_id] = new_cell;
    }
}

void CollisionResolver::close_program()
{

}