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
	std::cout << "init entities\n";
	for (int i = 0; i < ParticleSettings::particle_count; ++i)
	{
		Entity* entity = entities_.emplace(true);
		entity->position_ = Random::rand_pos_in_rect(*bounds_);
		entity->velocity_ = Random::rand_vector(-1.f, 1.f);
		entity->m_border = bounds_;
	}
}

void ParticleManager::update_particles()
{
	frame_rate_smoothing_.update_frame_rate();
	stats.updating_fps = frame_rate_smoothing_.get_average_frame_rate();
	stats.iterations_++;
	add_particles_to_grid();
	resolve_collisions();

	for (Entity* entity : entities_)
	{
		entity->update();
	}
}

void ParticleManager::add_particles_to_grid()
{
	int n = entities_.size();
	render.positions_x.resize(n);
	render.positions_y.resize(n);
	render.radii.resize(n);
	render.colors.resize(n);
	stats.cell_particle_count = n;

	grid.clear();

	int i = 0;
	for (Entity* entity : entities_)
	{
		const sf::Vector2f pos = entity->position_;
		grid.add_object(pos.x, pos.y, entity->id_);

		render.positions_x[i] = pos.x;
		render.positions_y[i] = pos.y;
		render.radii[i] = entity->m_radius;
		render.colors[i] = entity->collided ? entity->m_colorActive : entity->m_colorInactive;
		++i;
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
	const int n = entities_.size();

	snapshot.stats.cell_particle_count = n;

	snapshot.toggles = toggles;
	snapshot.stats = stats;

	snapshot.render.positions_x.resize(n);
	snapshot.render.positions_y.resize(n);
	snapshot.render.colors.resize(n);
	snapshot.render.radii.resize(n);

	std::memcpy(snapshot.render.positions_x.data(), render.positions_x.data(), n * sizeof(float));
	std::memcpy(snapshot.render.positions_y.data(), render.positions_y.data(), n * sizeof(float));
	std::memcpy(snapshot.render.colors.data(), render.colors.data(), n * sizeof(sf::Color));
	std::memcpy(snapshot.render.radii.data(), render.radii.data(), n * sizeof(float));
}