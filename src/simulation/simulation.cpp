#include "simulation.h"
#include <iostream>

#include "imgui-SFML.h"
#include "imgui.h"

inline static constexpr int render_frame_rate = 144;
inline static constexpr int vysnc = false;
inline static constexpr float vel_var = -2.f;

Simulation::Simulation()
{
    init_window();
    init_camera();
    init_imGUI();
}

void Simulation::init_window()
{
    window.setFramerateLimit(render_frame_rate);
    window.setVerticalSyncEnabled(vysnc);
    window.resetGLStates();
    window.setView(window.getDefaultView());
}

void Simulation::init_camera()
{
    clock_.update_frame_rate();
    camera.m_mouse_prev_ = sf::Vector2f(sf::Mouse::getPosition(window));
}

// ── ImGui init ────────────────────────────────────────────────────────────────
void Simulation::init_imGUI()
{
    if (!ImGui::SFML::Init(window))
        std::cerr << "[ERROR]: Failed to initialize ImGui-SFML\n";

    ImGui::GetIO().FontGlobalScale = 1.0f;
}


void Simulation::run()
{
    m_sim_thread_ = std::thread([this]
        {
            while (running)
                update();
        });

    while (running)
    {
        render();
    }

    m_sim_thread_.join();
    ImGui::SFML::Shutdown();
}

void Simulation::update()
{
    resolve_modifications();

    frameCount++;

	particleManager.update_particles();

    // Package results into the triple buffer
    SimSnapshot& snap = m_sim_buffer_.get_write_buffer();

    // Filling the snapshot with information
    particleManager.fill_snapshot(snap);
	snap.stats.fps = static_cast<float>(clock_.get_average_frame_rate());
   
    m_sim_buffer_.publish();


}


void Simulation::render()
{
    // Always grab the freshest completed simulation frame
    const SimSnapshot& snap = m_sim_buffer_.begin_read();
    float dt = static_cast<float>(m_delta_time_.get_delta());
    //m_total_time_elapsed_ += dt;

    if (particleManager.stats.iterations_ <= 1)
        return;

    handle_events();       
    setCaption();

    window.clear();

	if (snap.toggles.draw_grid)
		particleManager.render_grid(camera.get_world_mouse_pos());

	renderer.render(snap, camera);


    handle_imGUI(snap, dt);

    m_sim_buffer_.end_read();

    ImGui::SFML::Render(window);
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


// ── Command dispatch ──────────────────────────────────────────────────────────
void Simulation::resolve_modifications()
{
    std::queue<SimCommand> local;
    {
        std::lock_guard<std::mutex> lock(m_cmd_mutex);
        std::swap(local, m_commands);
    }

    while (!local.empty())
    {
        SimCommand cmd = std::move(local.front());
        local.pop();

        switch (cmd.type)
        {
            // ── Toggles ───────────────────────────────────────────────────────────
        case CommandType::SetToggles:
            particleManager.toggles = cmd.toggles;
            break;

            // ── Reset ─────────────────────────────────────────────────────────────
        case CommandType::ResetSimulation:
            //particle_system_.init_grid_positioning();
            break;

        case CommandType::RandomizeSimulation:
           // particle_system_.randomize_sim();
            break;

        case CommandType::SetThreadCount:
            //particle_system_.set_thread_count(cmd.int_val);

            break;

        default:
            break;
        }
    }
}


void Simulation::dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos)
{
    if (event.is<sf::Event::Closed>())
        running = false;

    bool shifting = false;
    if (const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        handle_keyboard_events(key->code);

		if (key->code == sf::Keyboard::Key::LShift || key->code == sf::Keyboard::Key::RShift)
			shifting = true;
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

    if (shifting)
    {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle))
        {
			const float radius = 900;
            const float amount = 125;
            particleManager.add_particles_at_point(cam_pos, amount, radius);
        }

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
        {
			const float magnitude = 13.f;
			const float radius = 800.f;
            particleManager.repel_system_from_point(cam_pos, magnitude, radius);
        }
    }
}

void Simulation::handle_pause_toggle()
{
    //WorldToggles& toggles = particle_system_.toggles;

    bool& paused = paused;
    paused = !paused;
}

void Simulation::handle_mouse_press(const sf::Vector2f& cam_pos)
{
	if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
	{
        camera.begin_pan(); 
	} 
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
        particleManager.toggles.draw_grid = not particleManager.toggles.draw_grid;
        break;

    default: break;
    }
}