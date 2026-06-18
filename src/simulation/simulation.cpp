#include "simulation.h"
#include <iostream>

inline static constexpr int render_frame_rate = 144;
inline static constexpr int vysnc = false;
inline static constexpr float vel_var = -2.f;

Simulation::Simulation()
{
    window.setFramerateLimit(render_frame_rate);
    window.setVerticalSyncEnabled(vysnc);
    window.resetGLStates();

    clock_.update_frame_rate();
    camera.m_mouse_prev_ = sf::Vector2f(sf::Mouse::getPosition(window));

    window.setView(window.getDefaultView());
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

	particleManager.update_particles();
}

void Simulation::render()
{
    handle_events();       
    setCaption();

    particleManager.render_particles();

    window.clear();
    
    window.display();
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

    default: break;
    }
}