#include "particle_system.h"
#include "../utilities/random.h"
#include <spatial_grid/simple_spatial_grid.h>


ParticleManager::ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds)
	: window_(window), bounds_(bounds), entities_(ParticleSettings::maximum_particle_count)
{
	init_entities();

	updating_jobs_.reserve(initial_thread_count);
	active_entities.reserve(entities_.raw_objects_size());

	init_updating_tp_jobs();
}

void ParticleManager::init_entities()
{
	std::cout << "init entities\n";
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

sf::Color ParticleManager::velocity_to_color(const sf::Color rest, const sf::Color max_color,
	const float speed_sq, const float max_speed_sq)
{
	// No sqrt — work in squared space entirely
	const float t = speed_sq >= max_speed_sq ? 1.f : speed_sq * (1.f / max_speed_sq);

	// Pack both colors into uint32 and do the lerp with integer math
	const int dr = max_color.r - rest.r;
	const int dg = max_color.g - rest.g;
	const int db = max_color.b - rest.b;
	const int da = max_color.a - rest.a;

	return sf::Color{
		static_cast<uint8_t>(rest.r + static_cast<int>(dr * t)),
		static_cast<uint8_t>(rest.g + static_cast<int>(dg * t)),
		static_cast<uint8_t>(rest.b + static_cast<int>(db * t)),
		static_cast<uint8_t>(rest.a + static_cast<int>(da * t))
	};
}

void ParticleManager::init_updating_tp_jobs()
{
	// This funciton should be called every time a particle is added or removed from the system, 
	// to ensure that the thread pool has the correct number of jobs to process

	// Collect all active entities first
	active_entities.clear();
	for (Entity* e : entities_)
		active_entities.push_back(e);

	const sf::Vector2f bounds_pos = bounds_->position;
	const sf::Vector2f bounds_size = bounds_->size;

	// Now split the *active* list evenly across threads
	const int total_active = active_entities.size();
	const int chunk = std::max(1, (total_active + (int)initial_thread_count - 1) / (int)initial_thread_count);

	for (int t = 0; t < (int)initial_thread_count; ++t)
	{
		const int begin = t * chunk;
		if (begin >= total_active) break;
		const int end = std::min(begin + chunk, total_active);

		updating_jobs_.emplace_back([this, begin, end, bounds_pos, bounds_size]
			{
				for (int i = begin; i < end; ++i)
				{
					update_particle(active_entities[i], bounds_pos, bounds_size);
				}
			});
	}
	updating_thread_pool_.set_jobs(updating_jobs_);
}


void ParticleManager::update_particles()
{
	frame_rate_smoothing_.update_frame_rate();
	stats.updating_fps = frame_rate_smoothing_.get_average_frame_rate();
	stats.iterations_++;

	// Collisions
	// // 1100fps down to 60fps
	collision_resolver_.add_particles_to_grid();        // Particles get added to the spatial grid
	
	// 63fps down to 45fps
	collision_resolver_.run_collision_detection();      // Overlapping particles are added to a container
	
	// 45fps down to 42fps
	collision_resolver_.handle_collision_resolutions(); // Overlapping particles are resolved

	// Multithreadding
	updating_thread_pool_.run_and_wait();
}

void ParticleManager::update_particle(Entity* entity, const sf::Vector2f& bounds_pos, const sf::Vector2f& bounds_size)
{
	sf::Vector2f& position_ = entity->position_;
	sf::Vector2f& velocity_ = entity->velocity_;

	position_ += velocity_;

	velocity_ *= friction;
	//velocity_ += sf::Vector2f(0, 0.01f); // gravity

	float speed_sq = velocity_.lengthSquared();
	static const float max_speed_sq = SimulationSettings::maxSpeed * SimulationSettings::maxSpeed;
	entity->color_ = velocity_to_color(entity->color_rest_, entity->color_max_, speed_sq, max_speed_sq);

	const float buffer = entity->radius_;

	const float x_min = bounds_pos.x + buffer;
	const float y_min = bounds_pos.y + buffer;
	const float x_max = bounds_pos.x + bounds_size.x - buffer;
	const float y_max = bounds_pos.y + bounds_size.y - buffer;

	// Branchless velocity flip — no branch misprediction
	velocity_.x *= 1.f - 2.f * (position_.x < x_min || position_.x > x_max);
	velocity_.y *= 1.f - 2.f * (position_.y < y_min || position_.y > y_max);

	// Clamp position
	position_.x = std::clamp(position_.x, x_min, x_max);
	position_.y = std::clamp(position_.y, y_min, y_max);
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
		{
			entity = entities_.emplace(true);
		}

		if (entity == nullptr)
		{
			std::cout << "ParticleManager::add_particles_at_point: Failed to create new particle entity.\n";
			break;
		}

		create_random_entity(entity, point + Random::rand_vector(-radius, radius));

		collision_resolver_.add_particles_to_grid();
	}

	init_updating_tp_jobs();
}

void ParticleManager::create_random_entity(Entity* entity, sf::Vector2f position)
{
	entity->position_ = position;
	entity->velocity_ = Random::rand_vector(-init_velocity_range, init_velocity_range);
	entity->color_rest_ = color_rest;
	entity->color_max_ = color_max;

	const float hue_shift = Random::rand_range(0.f, hue_shift_range);
	entity->color_rest_ = shift_hue(color_rest, hue_shift);
	entity->color_max_ = shift_hue(color_max, hue_shift);
	entity->radius_ = Random::rand_range(ParticleSettings::particle_radius_min, ParticleSettings::particle_radius_max);
}

sf::Color ParticleManager::get_rand_white_color()
{
	// depricated
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
	const int n = static_cast<int>(entities_.size());
	stats.cell_particle_count = n;

	snapshot.toggles = toggles;
	snapshot.stats = stats;

	// These should be done once at init / on entity count change, not every frame
	// snapshot.render.positions/colors/radii should already be the right size
	// If you must guard it:
	if (static_cast<int>(snapshot.render.positions.size()) != n)
	{
		snapshot.render.positions.resize(n);
		snapshot.render.colors.resize(n);
		snapshot.render.radii.resize(n);
	}

	// Single pass, write directly into snapshot — no intermediate buffer
	// If you have SoA (soa.x, soa.y, soa.radius) use that instead of entity pointers
	sf::Vector2f* __restrict pos_dst = snapshot.render.positions.data();
	sf::Color* __restrict col_dst = snapshot.render.colors.data();
	float* __restrict rad_dst = snapshot.render.radii.data();

	for (int i = 0; i < n; ++i)
	{
		const Entity* e = entities_.at(i);
		pos_dst[i] = e->position_;
		col_dst[i] = e->color_;
		rad_dst[i] = e->radius_;
	}
}