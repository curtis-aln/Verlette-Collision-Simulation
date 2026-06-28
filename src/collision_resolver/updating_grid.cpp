#include "collision_resolver.h"



void CollisionResolver::add_particles_to_grid()
{
    const int n = entities_->size();
    const int frame_parity = resolution_frame_ & 1;

    grid.clear();
    grid.prev_cells.resize(entities_->size());
    grid.entity_slot.assign(entities_->size(), 0);

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
    const int n = static_cast<int>(entities_->size());

    // Precompute to avoid member dereference in loop
    const uint32_t cap = grid.cell_max_capacity;
    const float    inv_cw = 1.f / grid.cell_width;
    const float    inv_ch = 1.f / grid.cell_height;

    cell_idx* __restrict prev = grid.prev_cells.data();
    uint8_t* __restrict slots = grid.entity_slot.data();
    uint8_t* __restrict counts = grid.cell_capacities.data();
    obj_idx* __restrict gdata = grid.grid.data();

    for (int obj_id = 0; obj_id < n; ++obj_id)
    {
        const sf::Vector2f pos = entities_->at(obj_id)->position_;

        const cell_idx new_cell = calcZOrder(
            static_cast<uint16_t>(pos.x * inv_cw),
            static_cast<uint16_t>(pos.y * inv_ch)
        );

        const cell_idx old_cell = prev[obj_id];
        if (new_cell == old_cell) continue;

        // O(1) remove using slot tracking
        const uint8_t  my_slot = slots[obj_id];
        uint8_t& old_cap = counts[old_cell];
        obj_idx* old_data = gdata + old_cell * cap;

        --old_cap;
        if (my_slot != old_cap)
        {
            const obj_idx displaced = old_data[old_cap];
            old_data[my_slot] = displaced;
            slots[displaced] = my_slot;
        }

        // O(1) insert
        uint8_t& new_cap = counts[new_cell];
        if (new_cap < cap)
        {
            gdata[new_cell * cap + new_cap] = static_cast<obj_idx>(obj_id);
            slots[obj_id] = new_cap;
            ++new_cap;
        }

        prev[obj_id] = new_cell;
    }
}