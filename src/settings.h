#pragma once
#include <SFML/Graphics.hpp>

struct SimulationSettings
{
    inline static const sf::Color bg_color = { 0, 0, 20 };

    inline static const unsigned int vertexReserve = 100;

    inline static const unsigned int circleSides = 15;
    inline static const unsigned int deltaGridRate = 1;

    inline static const float maxSpeed = 50.0f;

    inline static const float zoomStrength = 1.0f;

    inline static const float screen_width = 1900.0f;
    inline static const float screen_height = 1000.0f;
    inline static const float aspect_ratio = screen_width / screen_height;

    inline static float scale_factor = 50.0f;
};

struct ParticleSettings
{
    inline static const sf::Color color_rest = { 30, 60, 200 };
    inline static const sf::Color color_max = { 255, 100, 0 };
    inline static const float hue_shift_range = 40;
    inline static const float init_velocity_range = 10.f;

    inline static constexpr float friction = 0.999995f;

    inline static constexpr float correction_factor = 0.2f;
    inline static constexpr float restitution = .69f;

    inline static unsigned initial_thread_count = 15;

    inline static const unsigned int initial_particle_count = 5000;
    inline static const unsigned int maximum_particle_count = 1'000'000;

    inline static const float world_width = SimulationSettings::screen_width * SimulationSettings::scale_factor;
    inline static const float world_height = SimulationSettings::screen_height * SimulationSettings::scale_factor;

    inline static const float particle_radius_min = 28.0f;
    inline static const float particle_radius_max = 70.0f;

    inline static uint32_t CellsX = 400;
    inline static uint32_t CellsY = CellsX / SimulationSettings::aspect_ratio;
	inline static const uint32_t cell_max_capacity = 6;

    sf::Color colorActive = { 255, 0, 0 };
    sf::Color colorInctive = { 255, 155, 255 };
};
