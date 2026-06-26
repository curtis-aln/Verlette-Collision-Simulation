#include "particle_system.h"
#include "../utilities/random.h"
#include <spatial_grid/simple_spatial_grid.h>


ParticleManager::ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds)
	: window_(window), bounds_(bounds), entities_(ParticleSettings::maximum_particle_count)
{
	init_entities();
}

void ParticleManager::init_entities()
{
	std::cout << "init entities\n";
	static float velocity_range = 10.21f;
	for (int i = 0; i < ParticleSettings::initial_particle_count; ++i)
	{
		Entity* entity = entities_.emplace(true);
		entity->position_ = Random::rand_pos_in_rect(*bounds_);
		create_random_entity(entity, entity->position_);
	}
}

sf::Color ParticleManager::mutate_color(const sf::Color& color, const int range)
{
	return sf::Color{
		static_cast<uint8_t>(std::clamp(color.r + Random::rand_range(-range, range), 0, 255)),
		static_cast<uint8_t>(std::clamp(color.g + Random::rand_range(-range, range), 0, 255)),
		static_cast<uint8_t>(std::clamp(color.b + Random::rand_range(-range, range), 0, 255)),
		color.a
	};
}

sf::Color ParticleManager::shift_hue(const sf::Color& color, const float degrees)
{
	// RGB -> normalised floats
	const float r = color.r * (1.f / 255.f);
	const float g = color.g * (1.f / 255.f);
	const float b = color.b * (1.f / 255.f);

	const float max_c = std::max({ r, g, b });
	const float min_c = std::min({ r, g, b });
	const float delta = max_c - min_c;

	// Compute hue in [0, 360)
	float h = 0.f;
	if (delta > 1e-6f)
	{
		if (max_c == r) h = 60.f * std::fmod((g - b) / delta, 6.f);
		else if (max_c == g) h = 60.f * ((b - r) / delta + 2.f);
		else                 h = 60.f * ((r - g) / delta + 4.f);
		if (h < 0.f) h += 360.f;
	}

	const float s = max_c > 1e-6f ? delta / max_c : 0.f;
	const float v = max_c;

	// Shift hue and wrap
	h = std::fmod(h + degrees, 360.f);
	if (h < 0.f) h += 360.f;

	// HSV -> RGB
	const float c = v * s;
	const float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
	const float m = v - c;

	float ro, go, bo;
	const int sector = static_cast<int>(h / 60.f);
	switch (sector)
	{
	case 0:  ro = c; go = x; bo = 0; break;
	case 1:  ro = x; go = c; bo = 0; break;
	case 2:  ro = 0; go = c; bo = x; break;
	case 3:  ro = 0; go = x; bo = c; break;
	case 4:  ro = x; go = 0; bo = c; break;
	default: ro = c; go = 0; bo = x; break;
	}

	return sf::Color{
		static_cast<uint8_t>((ro + m) * 255.f),
		static_cast<uint8_t>((go + m) * 255.f),
		static_cast<uint8_t>((bo + m) * 255.f),
		color.a
	};
}

sf::Color ParticleManager::velocity_to_color(const sf::Color rest, const sf::Color max_color, const float speed, const float max_speed)
{
	const float t = speed >= max_speed ? 1.f : speed * (1.f / max_speed);

	return sf::Color{
		static_cast<uint8_t>(rest.r + static_cast<int>((max_color.r - rest.r) * t)),
		static_cast<uint8_t>(rest.g + static_cast<int>((max_color.g - rest.g) * t)),
		static_cast<uint8_t>(rest.b + static_cast<int>((max_color.b - rest.b) * t)),
		static_cast<uint8_t>(rest.a + static_cast<int>((max_color.a - rest.a) * t))
	};
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

		float speed = velocity_.length();
		entity->color_ = velocity_to_color(entity->color_rest_, entity->color_max_, speed, SimulationSettings::maxSpeed);

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


void ParticleManager::repel_system_from_point(const sf::Vector2f point, const float magnitude, const float radius)
{
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

void ParticleManager::add_particles_at_point(const sf::Vector2f point, const int amount, const float radius)
{
	for (int i = 0; i < amount; ++i)
	{
		Entity* entity = entities_.add();
		if (entity == nullptr)
			entity = entities_.emplace(true);

		if (entity == nullptr)
			break;

		create_random_entity(entity, point + Random::rand_vector(-radius, radius));
	}
}

void ParticleManager::create_random_entity(Entity* entity, sf::Vector2f position)
{
	entity->position_ = position;
	entity->velocity_ = Random::rand_vector(-10.f, 10.f);
	entity->color_rest_ = { 30, 60, 200 };
	entity->color_max_ = { 255, 100, 0 };
	const float hue_shift = Random::rand_range(0.f, 40.f);
	entity->color_rest_ = shift_hue({ 30, 60, 200 }, hue_shift);
	entity->color_max_ = shift_hue({ 255, 140, 0 }, hue_shift);
	entity->radius_ = Random::rand_range(ParticleSettings::particle_radius_min, ParticleSettings::particle_radius_max);
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
	stats.cell_particle_count = n;

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