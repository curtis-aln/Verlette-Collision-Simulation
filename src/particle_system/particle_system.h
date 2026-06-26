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

public:
	RenderData render{};
	WorldToggles toggles{};
	WorldStatistics stats{};

	ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds);


	void init_entities();

	void update_particles();
	void render_grid(sf::Vector2f query_pos);
	void fill_snapshot(SimSnapshot& snapshot);

	void repel_system_from_point(const sf::Vector2f point);

private:
	sf::Color get_rand_white_color();
	void init_collision_jobs();
};