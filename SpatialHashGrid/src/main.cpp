/*
    Makes a basic collision detection simulation Using a spatial Hash grid

    keys:
    > space - pause
    > Esc   - close
    > 1     - grid size +
    > 2     - grid size -
*/

#include <iostream>
#include <SFML/Graphics.hpp>

#include <vector>
#include <ctime>
#include <string>
#include <sstream>
#include "SpatialHashGrid/spatialHashGrid.h"
#include "circles/circles.hpp"
#include "entity.hpp"
#include "utilities/zoomableVertexArray.hpp"
#include "utilities/generalFunctions.hpp"



void setCaption(sf::RenderWindow& window, sf::Clock& clock)
{
    // FPS management
    const float timePerFrame = clock.restart().asSeconds();

    std::ostringstream oss;
    oss << "Collision Detection | MS Per Frame:" << 1.0f / timePerFrame << "";
    const std::string var = oss.str();
    window.setTitle(var);
}


std::unique_ptr<sf::RenderWindow> generateWindow(const float screenWidth, const float screenHeight, const std::string& name)
{
    auto window = std::make_unique<sf::RenderWindow>(sf::VideoMode(static_cast<int>(screenWidth), static_cast<int>(screenHeight)), name);
    window->setFramerateLimit(144);
    window->setVerticalSyncEnabled(false);
    window->resetGLStates();

    return window;
}

std::vector<Entity> generateEntities(const float screenWidth, const float screenHeight, const unsigned int amount, const float radius, 
    const float maxSpeed, const sf::Color colorActive, const sf::Color colorInctive, const sf::Rect<float>& border)
{
    std::vector<Entity> entities;
    entities.reserve(amount);

    for (size_t i = 0; i < amount; i++)
    {
	    const sf::Vector2f position = { randfloat(radius, screenWidth - radius), randfloat(radius, screenHeight - radius) };
	    const sf::Vector2f velocity = { randfloat(-2.0f, 2.0f) , randfloat(-2.0f, 2.0f) };

        Entity entity(position, velocity, colorActive, colorInctive, radius,i, maxSpeed, border);
        entities.emplace_back(entity);
    }

    return entities;
}


struct Settings
{
	const unsigned int particles;
    const unsigned int vertexReserve;
    const unsigned int circleSides;
    const unsigned int deltaGridRate;

    const float maxSpeed;
    const float entityRadius;
    const float zoomStrength;

    const float screenWidth;
    const float screenHeight;

    unsigned int CellsX;
    unsigned int CellsY;
};


struct RunTimeVariables
{
    bool paused;
    bool draw_grid;
    bool mousePressed;
    unsigned long long frameCount;
    sf::Vector2f mousePosition;
};


void pollEvents(sf::RenderWindow& window, RunTimeVariables& runVars, Settings& settings, 
    ZoomableVertexArray& zoomedCircles, ZoomableVertexArray& zoomedGrid, SpatialHashGrid& grid)
{
    // event handeler
    sf::Event event{};

    while (window.pollEvent(event))
    {
        if (event.type == sf::Event::Closed)
            window.close();

        else if (event.type == sf::Event::KeyPressed)
        {
            switch (event.key.code) {
            case sf::Keyboard::Escape:
                window.close();
                break;

            case sf::Keyboard::Space:
                runVars.paused = not runVars.paused;
                break;

            case sf::Keyboard::G:
                runVars.draw_grid = not runVars.draw_grid;
                break;

            case sf::Keyboard::Num1:
                settings.CellsX += settings.deltaGridRate;
                settings.CellsY += settings.deltaGridRate;
                grid.reSize(sf::FloatRect{ 0.0f, 0.0f, static_cast<float>(settings.CellsX), static_cast<float>(settings.CellsY) });
                break;

            case sf::Keyboard::Num2:
                settings.CellsX -= settings.deltaGridRate;
                settings.CellsY -= settings.deltaGridRate;
                grid.reSize(sf::FloatRect{ 0.0f, 0.0f, static_cast<float>(settings.CellsX), static_cast<float>(settings.CellsY) });
                break;
            }
        }

        else if (event.type == sf::Event::MouseWheelScrolled)
        {
            auto mousePos = sf::Vector2f(sf::Mouse::getPosition(window));
            zoomedCircles.updateMousePosition(mousePos, event.mouseWheelScroll.delta);
            zoomedGrid.updateMousePosition(mousePos, event.mouseWheelScroll.delta);
        }

        else if (event.type == sf::Event::MouseButtonPressed)
        {
            runVars.mousePressed = true;
        }

        else if (event.type == sf::Event::MouseButtonReleased)
        {
            runVars.mousePressed = false;
        }
    }
}


int main()
{
    Settings settings(
        10'000,
        5,
        13,
        10,
        0.04f,
        3.5f,
        0.25f,
        1920.0f,
        1080.0f
    );
    settings.CellsX = 1000;
    settings.CellsY = settings.CellsX;



    // initilising random
    std::srand(static_cast<unsigned>(time(nullptr)));

    // setting up the screen
    std::unique_ptr<sf::RenderWindow> window = generateWindow(settings.screenWidth, settings.screenHeight, "Spatial Hash Grid");
    auto clock = sf::Clock::Clock();

    ArrayOfCircles circles(settings.particles, settings.entityRadius, settings.circleSides);
    sf::Rect<float> border{ 0.0f, 0.0f, settings.screenWidth, settings.screenHeight };
    SpatialHashGrid grid{ border, {settings.CellsX, settings.CellsY} };

    std::vector<Entity> entities = generateEntities(
        settings.screenWidth, settings.screenHeight, settings.particles, settings.entityRadius, settings.maxSpeed, 
        { 255, 0, 0 }, { 255, 255, 255 }, border);


    RunTimeVariables runVars(
        false,
        false,
        false,
        0,
        getMousePositionFloat(*window)
    );

    sf::VertexArray v_array{};
    // Zooming
    ZoomableVertexArray zoomedCircles(&circles.m_circleArray, settings.zoomStrength, settings.screenWidth, settings.screenHeight);
    ZoomableVertexArray zoomedGrid(&v_array, settings.zoomStrength, settings.screenWidth, settings.screenHeight);

    

    // main game loop
    while (window->isOpen()) 
    {
        pollEvents(*window, runVars, settings, zoomedCircles, zoomedGrid, grid);

        if (runVars.mousePressed)
        {
            sf::Vector2f newPosition = getMousePositionFloat(*window);
            sf::Vector2f deltaPosition = newPosition - runVars.mousePosition;

            zoomedCircles.translate(deltaPosition);
            zoomedGrid.translate(deltaPosition);

            runVars.mousePosition = newPosition;
        }

        window->clear();

        if (!runVars.paused)
        {
	        grid.clear();

	        // first loop is for adding the points
	        for (int i{0}; i < entities.size(); i++)
	        {
	            grid.addAtom(entities[i].getPosition(), i);
	        }


	        // second loop is for querying the nearby
	        for (Entity& entity : entities)
	        {
                c_Vec& nearbyIndexes = grid.find(entity.p_position);

                entity.p_nearby.clear();
	        	entity.p_nearby.reserve(nearbyIndexes.size);

                for (unsigned i = 0; i < nearbyIndexes.size; i++)
                {
                    entity.p_nearby.emplace_back(&entities[nearbyIndexes.at(i)]);
                }
	        }

            // third loop is for updating the entities with their nearby entities
            for (Entity& entity : entities)
            {
                entity.update(circles);
            }
        }


        setCaption(*window, clock);
        zoomedCircles.drawVertexArray(*window, &circles.m_circleArray);
        if (runVars.draw_grid)
			zoomedGrid.drawVertexArray(*window, &v_array);
        window->display();

        runVars.frameCount++;
    }
}
