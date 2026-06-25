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

inline static const int nearby_ids_max = ParticleSettings::cell_max_capacity * 9;


// This is a basic datastructure which allows the adding and removing of collision indexes without memory reallocation
struct alignas(64) CollisionVector
{
	using CollisionPair = std::pair<int, int>;
	std::vector<CollisionPair> collision_pairs_{};
	int size_ = 0; // determines the active indexes in the collision_pair vector

	// a manual padding technique to force the struct to occupy exactly one 64-byte cache line
	char padding_[64 - sizeof(std::vector<CollisionPair>) - sizeof(int)] = {};
	
	CollisionVector(int reserve)
	{
		collision_pairs_.resize(reserve);
	}

	void add(int index_a, int index_b)
	{
		if (size_ >= static_cast<int>(collision_pairs_.size())) return;
		collision_pairs_[size_].first = index_a;
		collision_pairs_[size_].second = index_b;
		++size_;
	}

	void clear()
	{
		size_ = 0;
	}
};

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
	std::vector<std::function<void()>> collision_jobs_;
	
	std::vector<CollisionVector> collision_indexes{};

	// ---------------------------
	int resolution_frame_ = 0;  // toggles 0/1 each frame

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
	void add_particles_to_grid();
	
	sf::Color get_rand_white_color();
	void init_collision_jobs();
	void run_collision_detection();
	void detect_collisions_for_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector);
	void update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids);
	void update_protozoa_cell(const int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector);
	void handle_collision_resolutions();
	void resolve_pair_collision(Entity* particle_a, Entity* particle_b);
};