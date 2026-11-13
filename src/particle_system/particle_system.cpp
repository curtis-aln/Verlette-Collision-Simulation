#include "../utilities/random.h"
#include "particle_system.h"
#include <algorithm>
#include <cmath>
#include <collision_resolver/collision_resolver.h>
#include <cstdint>
#include <iostream>
#include <settings.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_snapshot.h>


ParticleManager::ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds)
	: window_(window), bounds_(bounds), collision_bodies_(ParticleSettings::maximum_particle_count)
{
	init_entities();

	updating_jobs_.reserve(initial_thread_count);
	active_entities.reserve(collision_bodies_.raw_objects_size());

	init_updating_tp_jobs();

	collision_resolver_.add_particles_to_grid();
}


void ParticleManager::update_particle_step()
{
	ParticleStep& s = particle_step_;
	const sf::Vector2f bounds_pos = bounds_->position;
	const sf::Vector2f bounds_size = bounds_->size;

	s.x0 = bounds_pos.x;                    s.y0 = bounds_pos.y;
	s.x1 = bounds_pos.x + bounds_size.x;    s.y1 = bounds_pos.y + bounds_size.y;
	s.cx = bounds_pos.x + bounds_size.x * 0.5f;
	s.cy = bounds_pos.y + bounds_size.y * 0.5f;
	s.drag = friction;
	s.central_gravity = toggles.central_gravity;
	s.gravity_k = central_gravity_const;
	s.softening_sq = 250.f * 250.f;      // see note 2
	s.surface_gravity = toggles.surface_gravity ? surface_gravity_const : 0.f;
	s.jitter_on = toggles.random_jitter;
	s.jitter = random_jitter_const;
	s.restitution = ResolutionSettings::restitution;
	s.density = ResolutionSettings::density;

	const float max_speed_sq = SimulationSettings::maxSpeed * SimulationSettings::maxSpeed;
	const float max_mass = ParticleSettings::particle_radius_max * ParticleSettings::particle_radius_max * s.density;
	s.max_ke = 0.5f * max_mass * max_speed_sq;
}

void ParticleManager::init_entities()
{
	std::cout << "init entities\n";
	for (int i = 0; i < ParticleSettings::initial_particle_count; ++i)
	{
		Body* body = collision_bodies_.emplace(true);
		body->id_ = static_cast<BodyId>(i);
		body->set_pos(Random::rand_pos_in_rect(*bounds_));
		create_random_entity(body, body->get_pos());
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

void ParticleManager::init_updating_tp_jobs()
{
	// This funciton should be called every time a particle is added or removed from the system, 
	// to ensure that the thread pool has the correct number of jobs to process
	updating_jobs_.clear();


	// Collect all active entities first
	active_entities.clear();
	for (Body* e : collision_bodies_)
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
					update_particle(active_entities[i]);
				}
			});
	}
	updating_thread_pool_.set_jobs(updating_jobs_);
}


void ParticleManager::update(SimSnapshot& write_snapshot, bool paused)
{
	frame_rate_smoothing_.update_frame_rate();
	if (!paused)
	{
		for (int i = 0; i < ParticleSettings::sub_iterations; ++i)
			update_particles();
	}

	
	fill_snapshot(write_snapshot);

}

void ParticleManager::update_particles()
{
	
	update_particle_step();

	stats.updating_fps = frame_rate_smoothing_.get_average_frame_rate();
	stats.iterations_++;

	// Collisions
	collision_resolver_.add_particles_to_grid();        // Particles get added to the spatial grid

	// 63fps down to 45fps
	collision_resolver_.run_collision_detection();      // Overlapping particles are added to a container

	// 45fps down to 42fps
	collision_resolver_.handle_collision_resolutions();  // Overlapping particles are resolved

	// Multithreadding
	updating_thread_pool_.run_and_wait();

	iterations_clock_.update_frame_rate();
	stats.iterations_per_second = iterations_clock_.get_average_frame_rate();
}

void ParticleManager::update_particle(Body* e)
{
	const ParticleStep& s = particle_step_;

	// 1. Mass-independent accelerations (gravity, jitter), queued together with anything
	//    already applied this tick (repel_system_from_point, collision impulses).
	float ax = 0.f;
	float ay = s.surface_gravity;

	if (s.central_gravity)
	{
		// a = G * r_vec / (r^2 + eps^2)^(3/2)
		const sf::Vector2f& p = e->get_pos();
		const float dx = s.cx - p.x;
		const float dy = s.cy - p.y;
		const float inv_d = 1.f / std::sqrt(dx * dx + dy * dy + s.softening_sq);
		const float k = s.gravity_k * inv_d * inv_d * inv_d;
		ax += dx * k;
		ay += dy * k;
	}

	if (s.jitter_on)
	{
		const sf::Vector2f j = Random::rand_vector(-s.jitter, s.jitter);
		ax += j.x;
		ay += j.y;
	}

	e->apply_acceleration({ ax, ay });

	// 2. Integrate (vel += acc; pos += vel; acc = 0), then mass-aware drag.
	e->update_physics();
	e->apply_drag(s.drag);

	// 3. Walls: only touch the body when it is actually outside the bounds.
	const float radius = e->get_radius();
	const float x_min = s.x0 + radius, x_max = s.x1 - radius;
	const float y_min = s.y0 + radius, y_max = s.y1 - radius;

	const sf::Vector2f pos = e->get_pos();
	sf::Vector2f vel = e->get_vel();

	if (pos.x < x_min || pos.x > x_max || pos.y < y_min || pos.y > y_max)
	{
		if ((pos.x < x_min && vel.x < 0.f) || (pos.x > x_max && vel.x > 0.f)) vel.x *= -s.restitution;
		if ((pos.y < y_min && vel.y < 0.f) || (pos.y > y_max && vel.y > 0.f)) vel.y *= -s.restitution;

		e->set_vel(vel);
		e->set_pos({ std::min(std::max(pos.x, x_min), x_max),    // not std::clamp: UB if min > max
					 std::min(std::max(pos.y, y_min), y_max) });
	}
}


void ParticleManager::render_grid(sf::Vector2f query_pos)
{
	grid_renderer_.render(*window_, query_pos, 200);
}


void ParticleManager::repel_system_from_point(const sf::Vector2f point, const float magnitude, const float radius)
{
	static float rad_sq = radius * radius;

	for (Body* body : collision_bodies_)
	{
		sf::Vector2f pos = body->get_pos();
		sf::Vector2f rel = pos - point;
		float dist_sq = rel.lengthSquared();

		if (dist_sq > rad_sq)
			continue;

		sf::Vector2f dir = rel.normalized();

		body->apply_acceleration(dir * magnitude);
	}
}

void ParticleManager::add_particles_at_point(const sf::Vector2f point, const int amount, const float radius)
{
	for (int i = 0; i < amount; ++i)
	{
		Body* body = collision_bodies_.add();
		if (body == nullptr)
		{
			body = collision_bodies_.emplace(true);
		}

		if (body == nullptr)
		{
			std::cout << "ParticleManager::add_particles_at_point: Failed to create new particle body.\n";
			break;
		}

		create_random_entity(body, point + Random::rand_vector(-radius, radius));
	}

	collision_resolver_.add_particles_to_grid();

	init_updating_tp_jobs();
}

void ParticleManager::create_random_entity(Body* body, sf::Vector2f position)
{
	body->set_pos(position);
	body->set_vel(Random::rand_vector(-init_velocity_range, init_velocity_range));

	const float r = Random::rand_range(ParticleSettings::particle_radius_min, ParticleSettings::particle_radius_max);
	body->set_radius(r);
	body->set_mass(r * r * ResolutionSettings::density);
}


void ParticleManager::fill_snapshot(SimSnapshot& snapshot)
{
	const int n = static_cast<int>(collision_bodies_.size());
	stats.cell_particle_count = n;

	snapshot.toggles = toggles;
	snapshot.stats = stats;

	// These should be done once at init / on body count change, not every frame
	// snapshot.render.positions/colors/radii should already be the right size
	// If you must guard it:
	if (static_cast<int>(snapshot.render.positions.size()) != n)
	{
		snapshot.render.positions.resize(n);
		snapshot.render.radii.resize(n);
		snapshot.render.velocities.resize(n);
	}

	// Single pass, write directly into snapshot — no intermediate buffer
	// If you have SoA (soa.x, soa.y, soa.radius) use that instead of body pointers
	sf::Vector2f* __restrict pos_dst = snapshot.render.positions.data();
	sf::Vector2f* __restrict vel_dst = snapshot.render.velocities.data();
	float* __restrict rad_dst = snapshot.render.radii.data();

	for (int i = 0; i < n; ++i)
	{
		const Body* e = collision_bodies_.at(i);
		pos_dst[i] = e->get_pos();
		rad_dst[i] = e->get_radius();
		vel_dst[i] = e->get_vel();

	}
}