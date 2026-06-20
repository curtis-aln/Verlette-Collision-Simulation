#include "particle_manager.h"
#include "../utilities/random.h"
#include <spatial_grid/simple_spatial_grid.h>

ParticleManager::ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds)
	: window_(window), bounds_(bounds), entities_(ParticleSettings::particle_count)
{
	init_entities();
}

void ParticleManager::init_entities()
{
	for (int i = 0; i < ParticleSettings::particle_count; ++i)
	{
		Entity* entity = entities_.emplace(true);
		entity->position_ = Random::rand_pos_in_rect(*bounds_);
		entity->velocity_ = Random::rand_vector(-1.f, 1.f);
	}
}

void ParticleManager::update_particles()
{
	add_particles_to_grid();
	resolve_collisions();

	for (Entity* entity : entities_)
	{
		entity->update();
	}
}

void ParticleManager::render_particles()
{
	//renderer.render(window_, allCircles, entities_);
}


void ParticleManager::add_particles_to_grid()
{
	for (Entity* entity : entities_)
	{
		grid.add_object(entity->position_.x, entity->position_.y, entity->id_);
	}
}


void ParticleManager::resolve_collisions()
{
	FixedSpan<obj_idx> nearbyIndexes{ 25 };

	for (Entity* entity : entities_)
	{
		const sf::Vector2f pos = entity->getPosition();

		nearbyIndexes.clear();
		grid.find(pos.x, pos.y, &nearbyIndexes);

		// resolving the collisions here
		for (int i = 0; i < nearbyIndexes.count; ++i)
		{
			Entity* otherEntity = entities_.at(nearbyIndexes[i]);
			if (entity->id_ != otherEntity->id_)
			{
				const sf::Vector2f delta = otherEntity->position_ - entity->position_;
				const float distSquared = delta.x * delta.x + delta.y * delta.y;
				if (distSquared <= entity->m_radiusSquared * 2)
				{
					// Collision detected, handle it here
					// For example, you can change the color of the entities
					entity->collided = true;
					otherEntity->collided = true;
				}
			}
		}
	}
}


void ParticleManager::fill_snapshot(SimSnapshot& snapshot)
{

}