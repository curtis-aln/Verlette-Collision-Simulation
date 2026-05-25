#pragma once
#include <SFML/Graphics.hpp>

#include "settings.h"
#include "entity.hpp"
#include "SpatialHashGrid/spatialHashGrid.h"
#include "circles/circles.hpp"

#include "utilities/random.h"
#include "utilities/zoomableVertexArray.hpp"

class Simulation : Settings
{
	sf::RenderWindow window{};
	sf::Clock clock{};

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

	// Zooming
	ZoomableVertexArray zoomedCircles{ &circles.m_circleArray, zoomStrength, screenWidth, screenHeight };
	ZoomableVertexArray zoomedGrid{& v_array, zoomStrength, screenWidth, screenHeight};

public:
	Simulation();
	void run();

private:
	void pollEvents();
	void update();
	void render();

	void generateEntities(const int amount);

	void setCaption();
};