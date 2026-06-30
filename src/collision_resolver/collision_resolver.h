#pragma once

/* Collision Resolver Class
This class handles collision detection and resolution between particles in a 2D simulation. 
It uses a spatial grid to efficiently detect potential collisions and resolves them based on their positions and velocities.
Modify the Settings struct to change the grid size, cell capacity, and collision resolution parameters.
*/

#include "../utilities/o_vector.hpp" // Containing the particles
#include "../particle_system/particle.h" // The particle class Themselves
#include "../spatial_grid/simple_spatial_grid.h" // Collision Detection
#include "../utilities/thread_pool.h" // Multithreadding

#include <functional>
#include <set>

#include "collision_vector.h" // To know what to resolve

struct ResolutionSettings
{
	inline static uint32_t cells_x = (1u << 10); // for morton indexing, must be a power of 2
	inline static uint32_t cells_y = cells_x;     // square worlds
	inline static const uint32_t cell_max_capacity = 6; // maximum number of particles per cell, must be less than 256, but really shouldnt be any greater than 6

	inline static constexpr float correction_factor = 0.2f; // how much of the overlap is corrected each frame, 0.2 is a good value, 1.0 is too much and causes jittering
	inline static constexpr float restitution = .69f; // how much of the velocity is retained after a collision, 1.0 is perfectly elastic, 0.0 is perfectly inelastic
};

// The maximum number of nearby particles that can be detected for a given particle, 
// this is used to allocate the thread local buffer for nearby particles
inline static const int nearby_ids_max = ResolutionSettings::cell_max_capacity * 9;


// This class is resonsible for the updating and rendering of the particles in the simulation
class CollisionResolver : ResolutionSettings
{
	int thread_count_{};

	o_vector<Entity>* collision_bodies_; // pointer to the particle vector, not owned by this class

	// The grid used for collision resolution
	SimpleSpatialGrid spatial_grid_;

	// for multithreadded collision resolution
	BarrierThreadPool collision_thread_pool_;
	BarrierThreadPool add_to_grid_thread_pool_;

	// Pre-Creating the thread jobs for the collision detection and resolution
	std::vector<std::function<void()>> collision_jobs_;
	std::vector<std::function<void()>> add_to_grid_jobs_;

	// This is used in the collision detection to collect all the nearby particles for a given cell
	static thread_local FixedSpan<uint32_t> tl_nearby_ids_;

	// This is used to store the collisions detected by each thread, each thread has its own collision vector to avoid contention
	std::vector<CollisionVector> collision_indexes_{};

	// ---------------------------
	int resolution_frame_ = 0;  // toggles 0/1 each frame

public:
	CollisionResolver(sf::Rect<float>* bounds, o_vector<Entity>* entities, 
		unsigned int init_thread_count, unsigned int max_collisions_per_thread, unsigned int max_particles);

	// This function goes through each cell and updates their position in the grid rather than clearing the grid and re-adding all particles, this is more efficient
	void update_particles_grid_indexes();

	// This clears the grid and re-adds all particles to the grid, this is less efficient than update_particles_grid_indexes but is simpler and 
	// faster if the particles move a lot
	void add_particles_to_grid();

	// detects collisions for all particles in the grid, this is done in parallel using the thread pool, all collisions are stored in the collision_indexes vector, which is then used to resolve the collisions
	void run_collision_detection();

	// resolves all collisions in the collision_indexes vector, this is done in parallel using the thread pool
	void handle_collision_resolutions();

	// Closes The Threads safely
	void close_program();

	// Fetching Functions
	SimpleSpatialGrid* get_grid() { return &spatial_grid_; }

private:
	// The collision jobs for the threads are pre-calculated so there is no overhead of creating them each frame
	void init_collision_jobs();
	
	// Collision Detection Functions
	void primitive_detect_collisions_for_grid_cell(const int grid_cell_id, CollisionVector& collision_vector);
	void detect_collisions_for_grid_cell(const int grid_cell_id, FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector);
	void update_nearby_container(const int32_t neighbour_index_x, const int32_t neighbour_index_y, FixedSpan<uint32_t>& nearby_ids);
	void check_collisions_for_body(const int protozoa_cell_index, const FixedSpan<uint32_t>& nearby_ids, CollisionVector& collision_vector, int check_count = -1);

	// Collision Resolution Functions
	void resolve_collision_vector_collisions(CollisionVector& collision_vector);
	void resolve_pair_collision(Entity* particle_a, Entity* particle_b);
};