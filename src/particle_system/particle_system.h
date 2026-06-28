#pragma once

#include "../utilities/o_vector.hpp"
#include "particle.h"
#include "../settings.h"
#include "../rendering/PPS_renderer.h"
#include "../spatial_grid/simple_spatial_grid.h"
#include "../spatial_grid/spatial_grid_renderer.h"

#include "../collision_resolver/collision_resolver.h"

#include "state.h"
#include "../utilities/smooth_frame_rates.h"
#include "../utilities/thread_pool.h"
#include <functional>
#include <set>


// This class is resonsible for the updating and rendering of the particles in the simulation
class ParticleManager : ParticleSettings
{
	o_vector<Entity> entities_;
	sf::RenderWindow* window_;
	sf::Rect<float>* bounds_;

	CollisionResolver collision_resolver_{ window_, bounds_, &entities_ };
	SpatialGridRenderer grid_renderer_{ &collision_resolver_.grid };
	FrameRateSmoothing<30> frame_rate_smoothing_{};

	// This threadpool is responsible for updating the particles in the simulation
	BarrierThreadPool updating_thread_pool_{ static_cast<int>(initial_thread_count) };
	std::vector<std::function<void()>> updating_jobs_;

	std::vector<Entity*> active_entities;

public:
	RenderData render{};
	WorldToggles toggles{};
	WorldStatistics stats{};

	ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds);

	void init_entities();

	sf::Color mutate_color(const sf::Color& color, const int range);

	sf::Color shift_hue(const sf::Color& color, const float degrees);


	sf::Color velocity_to_color(const sf::Color rest, const sf::Color max_color, const float speed, const float max_speed);

	void init_updating_tp_jobs();

	void update_particles();
	void update_particle(Entity* entity, const sf::Vector2f& bounds_pos, const sf::Vector2f& bounds_size);
	void render_grid(sf::Vector2f query_pos);
	void fill_snapshot(SimSnapshot& snapshot);

	void repel_system_from_point(const sf::Vector2f point, const float magnitude, const float radius);

	void add_particles_at_point(const sf::Vector2f point, const int amount, const float radius);

	void create_random_entity(Entity* entity, sf::Vector2f position);

private:
	sf::Color get_rand_white_color();
};