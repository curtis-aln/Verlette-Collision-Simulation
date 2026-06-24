#pragma once


struct SimulationSettings
{
    inline static const unsigned int vertexReserve = 100;

    inline static const unsigned int circleSides = 15;
    inline static const unsigned int deltaGridRate = 1;

    inline static const float maxSpeed = 50.0f;
    
    inline static const float zoomStrength = 1.0f;

    inline static const float screen_width = 1900.0f;
    inline static const float screen_height = 1000.0f;
    inline static const float aspect_ratio = screen_width / screen_height;

    inline static float scale_factor = 12.0f;
};

struct ParticleSettings
{
    inline static unsigned initial_thread_count = 15;

    inline static const unsigned int particle_count = 140'300;

    inline static const float world_width = SimulationSettings::screen_width * SimulationSettings::scale_factor;
    inline static const float world_height = SimulationSettings::screen_height * SimulationSettings::scale_factor;

    inline static const float particle_radius = 35.0f;

    inline static uint32_t CellsX = 200;
    inline static uint32_t CellsY = CellsX / SimulationSettings::aspect_ratio;
	inline static const uint32_t cell_max_capacity = 4;

    sf::Color colorActive = { 255, 0, 0 };
    sf::Color colorInctive = { 255, 255, 255 };
};
