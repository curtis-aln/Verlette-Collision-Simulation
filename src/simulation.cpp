#include "simulation.h"

Simulation::Simulation()
{
    window.setFramerateLimit(144);
    window.setVerticalSyncEnabled(false);
    window.resetGLStates();

    generateEntities(particles);

    clock_.update_frame_rate();
    camera.m_mouse_prev_ = sf::Vector2f(sf::Mouse::getPosition(window));

    window.setView(window.getDefaultView());
    //camera.m_view_.move({ world_width / 2.f, world_height / 2.f });
}


void Simulation::run()
{
    // main game loop
    while (running)
    {
        if (!paused)
            update();
        
        render();
    }
}

void Simulation::update()
{
    frameCount++;

    for (int i{ 0 }; i < entities.size(); i++)
    {
        grid.addAtom(entities[i].getPosition(), i);
    }


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


    for (Entity& entity : entities)
    {
        entity.update(circles);
    }
}

void Simulation::render()
{
    handle_events();       
    setCaption();

    window.clear();
    window.draw(circles.m_circleArray);

    
    window.display();
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
    float fps_ = static_cast<float>(clock_.get_average_frame_rate());
    clock_.update_frame_rate();

    std::ostringstream title;
    title << "Spatial Hash Grid"
        << " | FPS: " << std::fixed << std::setprecision(1) << fps_;
    window.setTitle(title.str());
}

void Simulation::handle_events()
{
    const sf::Vector2f cam_pos = camera.get_world_mouse_pos();

    while (const std::optional event = window.pollEvent())
    {
        //ImGui::SFML::ProcessEvent(window, *event);
        dispatch_event(*event, cam_pos);
    }

	const float dt = clock_.get_delta_time();
    camera.update(dt);
}


void Simulation::dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos)
{
    if (event.is<sf::Event::Closed>())
        running = false;

    else if (const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        handle_keyboard_events(key->code);
    }
    else if (const auto* scroll = event.getIf<sf::Event::MouseWheelScrolled>())
    {
        //if (!ImGui::GetIO().WantCaptureMouse)  // don't zoom sim if imgui is using scroll
            camera.zoom(scroll->delta);
    }
    else if (event.is<sf::Event::MouseButtonPressed>())
    {
        //if (!ImGui::GetIO().WantCaptureMouse)  // don't interact with sim if imgui is focused
            handle_mouse_press(cam_pos);
    }
    else if (event.is<sf::Event::MouseButtonReleased>())
        handle_mouse_release();
}

void Simulation::handle_pause_toggle()
{
    //WorldToggles& toggles = particle_system_.toggles;

    bool& paused = paused;
    paused = !paused;
}

void Simulation::handle_mouse_press(const sf::Vector2f& cam_pos)
{
    camera.begin_pan();  // start pan only if we didn't click an organism
}

void Simulation::handle_mouse_release()
{
    camera.end_pan();
}

void Simulation::handle_keyboard_events(const sf::Keyboard::Key& event_key_code)
{
    switch (event_key_code)
    {
    case sf::Keyboard::Key::Escape: running = false;              break;
    case sf::Keyboard::Key::Space:  handle_pause_toggle();           break;
   

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

    default: break;
    }
}