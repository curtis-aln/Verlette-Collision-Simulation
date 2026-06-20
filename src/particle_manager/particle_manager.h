#pragma once

#include "../utilities/o_vector.hpp"
#include "../entity.hpp"
#include "../settings.h"
#include "../rendering/PPS_renderer.h"
#include "../spatial_grid/simple_spatial_grid.h"

#include "state.h"

// This class is resonsible for the updating and rendering of the particles in the simulation
class ParticleManager : ParticleSettings
{
	o_vector<Entity> entities_;
	sf::RenderWindow* window_;
	sf::Rect<float>* bounds_;

	SimpleSpatialGrid grid{ CellsX, CellsY, cell_max_capacity, world_width, world_height };


public:
	RenderData render{};
	WorldToggles toggles{};
	WorldStatistics stats{};

	ParticleManager(sf::RenderWindow* window, sf::Rect<float>* bounds);

	void init_entities();

	void update_particles();
	void fill_snapshot(SimSnapshot& snapshot);

private:
	void add_particles_to_grid();
	void resolve_collisions();
};