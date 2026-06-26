#include "../utilities/o_vector.hpp"
#include "../particle_system/particle.h"
#include "../settings.h"
#include "../rendering/PPS_renderer.h"
#include "../spatial_grid/simple_spatial_grid.h"
#include "../spatial_grid/spatial_grid_renderer.h"


#include "../utilities/smooth_frame_rates.h"
#include "../utilities/thread_pool.h"
#include <functional>
#include <set>

#include "collision_vector.h"

inline static const int nearby_ids_max = ParticleSettings::cell_max_capacity * 9;




// This class is resonsible for the updating and rendering of the particles in the simulation
class CollisionResolver : ParticleSettings
{
public:
	o_vector<Entity>* entities_;
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
	CollisionResolver(sf::RenderWindow* window, sf::Rect<float>* bounds, o_vector<Entity>* entities);

	void add_particles_to_grid();
	
	void init_collision_jobs();
	void run_collision_detection();

	void detect_collisions_for_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector);
	void update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids);
	void update_protozoa_cell(const int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector, int check_count);
	void handle_collision_resolutions();
	void resolve_collision_vector_collisions(CollisionVector& collision_vector);
	void resolve_pair_collision(Entity* particle_a, Entity* particle_b);
};