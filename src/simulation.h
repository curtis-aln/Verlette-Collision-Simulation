#pragma once
#include <SFML/Graphics.hpp>

#include "settings.h"
#include "entity.hpp"
#include "SpatialHashGrid/spatialHashGrid.h"
#include "circles/circles.hpp"

#include "utilities/random.h"
#include "utilities/camera.h"
#include "utilities/smooth_frame_rates.h"

class Simulation : Settings
{
	sf::Vector2u size = { static_cast<unsigned int>(screenWidth), static_cast<unsigned int>(screenHeight) };
	sf::RenderWindow window{ sf::VideoMode(size), "Collision Detection", sf::Style::Default };

	bool running = true;
	bool paused = false;
	bool draw_grid = false;
	bool mousePressed = false;
	unsigned long long frameCount = 0;
	sf::Vector2f mousePosition{};

	std::vector<Entity> entities{};

	sf::Rect<float> border{ {0.0f, 0.0f}, {screenWidth, screenHeight} };

	ArrayOfCircles circles{particles, entityRadius, circleSides};

	SpatialHashGrid grid{ border, {CellsX, CellsY} };


	sf::VertexArray v_array{};
	
	FrameRateSmoothing<100> clock_{};
	Camera camera{ &window, 1.f / scale_factor };



public:
	Simulation();
	void run();

private:
	void update();
	void render();

	void generateEntities(const int amount);

	void setCaption();
	void handle_events();
	void dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos);
	void handle_pause_toggle();
	void handle_mouse_press(const sf::Vector2f& cam_pos);
	void handle_mouse_release();
	void handle_keyboard_events(const sf::Keyboard::Key& event_key_code);
};