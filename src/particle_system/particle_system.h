#pragma once

#include "../settings.h"
#include "../spatial_grid/spatial_grid_renderer.h"
#include "../utilities/o_vector.hpp"

#include "../collision_resolver/collision_resolver.h"

#include "../utilities/smooth_frame_rates.h"
#include "../utilities/thread_pool.h"
#include "state.h"
#include <functional>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <simulation/context/sim_snapshot.h>
#include <vector>


struct ParticleStep
{
	float x0, y0, x1, y1;       // bounds edges (before the radius buffer)
	float cx, cy;               // bounds centre
	float drag;
	float gravity_k, softening_sq;
	float surface_gravity;      // 0 when the toggle is off
	float jitter;
	float restitution, density;
	float max_ke;
	bool  central_gravity, jitter_on;
};


// This class is resonsible for the updating and rendering of the particles in the simulation
class ParticleManager : ParticleSettings
{
	o_vector<Body> collision_bodies_;
	sf::RenderWindow* window_;
	sf::Rect<float>* bounds_;

	FrameRateSmoothing<50> iterations_clock_{};

	CollisionResolver collision_resolver_{ bounds_, &collision_bodies_, initial_thread_count,
		maximum_particle_count, maximum_particle_count };
	SpatialGridRenderer grid_renderer_{ collision_resolver_.get_grid() };
	FrameRateSmoothing<30> frame_rate_smoothing_{};

	// This threadpool is responsible for updating the particles in the simulation
	BarrierThreadPool updating_thread_pool_{ static_cast<int>(initial_thread_count) };
	std::vector<std::function<void()>> updating_jobs_;

	std::vector<Body*> active_entities;

	ParticleStep particle_step_{};

public:
	RenderData render{};
	WorldToggles toggles{};
	WorldStatistics stats{};

	ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds);

	void update_particle_step();

	void init_entities();

	sf::Color mutate_color(const sf::Color& color, const int range);

	sf::Color shift_hue(const sf::Color& color, const float degrees);


	void init_updating_tp_jobs();

	void update(SimSnapshot& write_snapshot, bool paused);


private:
	void update_particles();
	void update_particle(Body* e);

public:
	void render_grid(sf::Vector2f query_pos);
	void fill_snapshot(SimSnapshot& snapshot);

	void repel_system_from_point(const sf::Vector2f point, const float magnitude, const float radius);

	void add_particles_at_point(const sf::Vector2f point, const int amount, const float radius);

	void create_random_entity(Body* body, sf::Vector2f position);

};