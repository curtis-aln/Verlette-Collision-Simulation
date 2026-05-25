#include "simulation.h"

Simulation::Simulation()
{
    sf::Vector2u size = { static_cast<unsigned int>(screenWidth), static_cast<unsigned int>(screenHeight) };
	window = sf::RenderWindow(sf::VideoMode(size), "Collision Detection", sf::Style::Default); 
    window.setFramerateLimit(144);
    window.setVerticalSyncEnabled(false);
    window.resetGLStates();


    generateEntities(particles);
}


void Simulation::run()
{
    // main game loop
    while (window.isOpen())
    {
        pollEvents();

        if (mousePressed)
        {
            sf::Vector2f newPosition = getMousePositionFloat(window);
            sf::Vector2f deltaPosition = newPosition - mousePosition;

            zoomedCircles.translate(deltaPosition);
            zoomedGrid.translate(deltaPosition);

            mousePosition = newPosition;
        }

        window.clear();

        if (!paused)
        {
            grid.clear();

            // first loop is for adding the points
            for (int i{ 0 }; i < entities.size(); i++)
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


        setCaption();
        zoomedCircles.drawVertexArray(window, &circles.m_circleArray);
        if (draw_grid)
            zoomedGrid.drawVertexArray(window, &v_array);
        window.display();

        frameCount++;
    }
}


void Simulation::generateEntities(const int amount)
{
    Simulation::entities.reserve(amount);

    for (size_t i = 0; i < amount; i++)
    {
        const sf::Vector2f position = { Random::rand_range(entityRadius, screenWidth - entityRadius), Random::rand_range(entityRadius, screenHeight - entityRadius) };
        const sf::Vector2f velocity = { Random::rand_range(-2.0f, 2.0f) , Random::rand_range(-2.0f, 2.0f) };

        Entity entity(position, velocity, colorActive, colorInctive, entityRadius, i, maxSpeed, border);
        entities.emplace_back(entity);
    }
}

void Simulation::setCaption()
{
    // FPS management
    const float timePerFrame = clock.restart().asSeconds();

    std::ostringstream oss;
    oss << "Collision Detection | MS Per Frame:" << 1.0f / timePerFrame << "";
    const std::string var = oss.str();
    window.setTitle(var);
}

void Simulation::pollEvents()
{
    // event handler
    while (const std::optional<sf::Event> event = window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
            window.close();

        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            switch (keyPressed->code) {
            case sf::Keyboard::Key::Escape:
                window.close();
                break;

            case sf::Keyboard::Key::Space:
                paused = not paused;
                break;

            case sf::Keyboard::Key::G:
                draw_grid = not draw_grid;
                break;

            case sf::Keyboard::Key::Num1:
                CellsX += deltaGridRate;
                CellsY += deltaGridRate;
                grid.reSize(sf::FloatRect{
                    { 0.0f, 0.0f },
                    { static_cast<float>(CellsX), static_cast<float>(CellsY) }
                    });
                break;

            case sf::Keyboard::Key::Num2:
                CellsX -= deltaGridRate;
                CellsY -= deltaGridRate;
                grid.reSize(sf::FloatRect{
                    { 0.0f, 0.0f },
                    { static_cast<float>(CellsX), static_cast<float>(CellsY) }
                    });
                break;
            }
        }

        else if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>())
        {
            auto mousePos = sf::Vector2f(sf::Mouse::getPosition(window));
            zoomedCircles.updateMousePosition(mousePos, scroll->delta);
            zoomedGrid.updateMousePosition(mousePos, scroll->delta);
        }

        else if (event->is<sf::Event::MouseButtonPressed>())
        {
            mousePressed = true;
        }

        else if (event->is<sf::Event::MouseButtonReleased>())
        {
            mousePressed = false;
        }
    }
}