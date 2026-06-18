#pragma once


struct SimulationSettings
{
    inline static const unsigned int vertexReserve = 100;

    inline static const unsigned int circleSides = 15;
    inline static const unsigned int deltaGridRate = 1;

    inline static const float maxSpeed = 5.0f;
    
    inline static const float zoomStrength = 1.0f;

    inline static const float screen_width = 1600.0f;
    inline static const float screen_height = 900.0f;

    inline static float scale_factor = 10.0f;
};

struct ParticleSettings
{
    inline static const unsigned int particle_count = 100'000;

    inline static const float world_width = SimulationSettings::screen_width * SimulationSettings::scale_factor;
    inline static const float world_height = SimulationSettings::screen_height * SimulationSettings::scale_factor;

    inline static const float particle_radius = 5.0f;

    inline static uint32_t CellsX = 300;
    inline static uint32_t CellsY = 300;
	inline static uint32_t cell_max_capacity = 25;

    sf::Color colorActive = { 255, 0, 0 };
    sf::Color colorInctive = { 255, 255, 255 };
};
