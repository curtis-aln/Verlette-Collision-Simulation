#include "particle_system.h"
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
	static float velocity_range = 10.21f;
	for (int i = 0; i < ParticleSettings::particle_count; ++i)
	{
		Entity* entity = entities_.emplace(true);
		entity->position_ = Random::rand_pos_in_rect(*bounds_);
		entity->velocity_ = Random::rand_vector(-velocity_range, velocity_range);
		entity->color_ = get_rand_white_color();
		entity->radius_ = Random::rand_range(ParticleSettings::particle_radius_min, ParticleSettings::particle_radius_max);
	}
}

void ParticleManager::update_particles()
{
	frame_rate_smoothing_.update_frame_rate();
	stats.updating_fps = frame_rate_smoothing_.get_average_frame_rate();
	stats.iterations_++;

	// Collisions
	collision_resolver_.add_particles_to_grid();        // Particles get added to the spatial grid
	collision_resolver_.run_collision_detection();      // Overlapping particles are added to a container
	collision_resolver_.handle_collision_resolutions(); // Overlapping particles are resolved

	for (Entity* entity : entities_)
	{
		sf::Vector2f& position_ = entity->position_;
		sf::Vector2f& velocity_ = entity->velocity_;

		position_ += velocity_;
		velocity_ *= 0.999995f;

		const float buffer = entity->radius_;

		const bool x_out_of_bounds = position_.x < bounds_->position.x + buffer || position_.x > bounds_->position.x + bounds_->size.x - buffer;
		const bool y_out_of_bounds = position_.y < bounds_->position.y + buffer || position_.y > bounds_->position.y + bounds_->size.y - buffer;

		if (x_out_of_bounds) {
			velocity_.x *= -1;
		}

		if (y_out_of_bounds) {
			velocity_.y *= -1;
		}

		position_.x = std::max(bounds_->position.x + buffer, std::min(position_.x, bounds_->position.x + bounds_->size.x - buffer));
		position_.y = std::max(bounds_->position.y + buffer, std::min(position_.y, bounds_->position.y + bounds_->size.y - buffer));

	}
}


void ParticleManager::render_grid(sf::Vector2f query_pos)
{
	grid_renderer_.render(*window_, query_pos, 200);
}


void ParticleManager::repel_system_from_point(const sf::Vector2f point)
{
	static float magnitude = 142.f;
	static float radius = ParticleSettings::particle_radius_min * 20.f;
	static float rad_sq = radius * radius;

	for (Entity* entity : entities_)
	{
		sf::Vector2f pos = entity->position_;
		sf::Vector2f rel = pos - point;
		float dist_sq = rel.lengthSquared();

		if (dist_sq > rad_sq)
			continue;

		sf::Vector2f dir = rel.normalized();

		entity->velocity_ += dir * magnitude;
	}
}

sf::Color ParticleManager::get_rand_white_color()
{
	// generates a random white color so that when particles are densily packed you can easily see each one
	constexpr int min = 160;
	constexpr int max = 255;
	constexpr std::uint8_t transparency = 150;

	const int col = Random::rand_range(min, max);
	const std::uint8_t col_t = static_cast<std::uint8_t>(col);
	return sf::Color{ col_t, col_t, col_t, transparency };
}



void ParticleManager::fill_snapshot(SimSnapshot& snapshot)
{
	const int n = entities_.size();

	snapshot.stats.cell_particle_count = n;

	snapshot.toggles = toggles;
	snapshot.stats = stats;

	snapshot.render.positions.resize(n);
	snapshot.render.colors.resize(n);
	snapshot.render.radii.resize(n);
	render.positions.resize(n);
	render.colors.resize(n);
	render.radii.resize(n);

	for (int i = 0; i < n; ++i)
	{
		Entity* entity = entities_.at(i);
		render.positions[i] = entity->position_;
		render.colors[i] = entity->color_;
		render.radii[i] = entity->radius_;
	}

	std::memcpy(snapshot.render.positions.data(), render.positions.data(), n * sizeof(sf::Vector2f));
	std::memcpy(snapshot.render.colors.data(), render.colors.data(), n * sizeof(sf::Color));
	std::memcpy(snapshot.render.radii.data(), render.radii.data(), n * sizeof(float));
}