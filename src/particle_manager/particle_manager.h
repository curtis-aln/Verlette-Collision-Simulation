#pragma once

#include "../utilities/o_vector.hpp"
#include "../entity.hpp"
#include "../settings.h"
#include "../rendering/PPS_renderer.h"
#include "../spatial_grid/simple_spatial_grid.h"
#include "../spatial_grid/spatial_grid_renderer.h"


#include "state.h"
#include "../utilities/smooth_frame_rates.h"
#include "../utilities/thread_pool.h"
#include <functional>

// This class is resonsible for the updating and rendering of the particles in the simulation
class ParticleManager : ParticleSettings
{
	o_vector<Entity> entities_;
	sf::RenderWindow* window_;
	sf::Rect<float>* bounds_;

	SimpleSpatialGrid grid{ CellsX, CellsY, cell_max_capacity, world_width, world_height };
	SpatialGridRenderer grid_renderer_{ &grid };

	FrameRateSmoothing<30> frame_rate_smoothing_{};

	// for multithreadded collision resolution
	BarrierThreadPool collision_thread_pool_{ static_cast<int>(initial_thread_count) };

	static thread_local FixedSpan<uint32_t> tl_nearby_ids;

	alignas(64) std::vector<sf::Vector2f> entity_velocities_;
	std::array<std::vector<int>, 6> collision_color_groups;
	std::vector<int> colour_job_boundaries_;
	std::vector<std::function<void()>> collision_jobs_;

	alignas(64) std::vector<sf::Vector2f> collision_resolutions{};
	alignas(64) std::vector<sf::Vector2f> velocity_resolutions{};

	// ---------------------------

public:
	RenderData render{};
	WorldToggles toggles{};
	WorldStatistics stats{};

	ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds);

	void init_entities();

	void update_particles();
	void render_grid(sf::Vector2f query_pos);
	void fill_snapshot(SimSnapshot& snapshot);

private:
	void add_particles_to_grid();
	void resolve_collisions();
	void build_color_groups();
	void init_collision_jobs();
	void update_cells_in_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, const int thread_id);
	void update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids);
	void update_protozoa_cell(const int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids, const int thread_id);
	void calculate_collision_resolutions();
};