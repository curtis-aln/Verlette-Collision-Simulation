#include "collision_resolver.h"
#include "../utilities/random.h"
#include <spatial_grid/simple_spatial_grid.h>

thread_local FixedSpan<uint32_t> CollisionResolver::tl_nearby_ids{ nearby_ids_max };

CollisionResolver::CollisionResolver(sf::RenderWindow* window, sf::Rect<float>* bounds, o_vector<Entity>* entities)
	: window_(window), bounds_(bounds), entities_(entities)
{
	init_collision_jobs();
	collision_indexes.resize(initial_thread_count, CollisionVector(maximum_particle_count / initial_thread_count));

	collision_thread_pool_.set_jobs(collision_jobs_);  // once
}


void CollisionResolver::add_particles_to_grid()
{
	const int n = entities_->size();
	const int frame_parity = resolution_frame_ & 1;

	grid.clear();

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