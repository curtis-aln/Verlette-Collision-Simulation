// Required edits in simulation.h:
//   1.  bool paused = false;        ->   std::atomic<bool> paused{ false };
//   2.  add to the private section:      void push_command(SimCommand cmd);

#include "simulation.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

#include "context/sim_command.h"
#include "context/sim_snapshot.h"
#include "imgui.h"
#include "imgui-SFML.h"
#include <collision_resolver/collision_resolver.h>
#include <ios>
#include <mutex>
#include <queue>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <thread>

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

	if (!paused)
	{
		frameCount++;
		particleManager.update_particles();
	}

	// Package results into the triple buffer. We keep publishing while paused
	// so toggle changes made in the UI still show up in the snapshot.
	SimSnapshot& snap = m_sim_buffer_.get_write_buffer();

	particleManager.fill_snapshot(snap);
	snap.stats.fps = static_cast<float>(clock_.get_average_frame_rate());

	m_sim_buffer_.publish();

	if (paused)
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
}


void Simulation::render()
{
	// Sample dt first so a skipped frame doesn't produce one huge delta later.
	const float dt = static_cast<float>(m_delta_time_.get_delta());

	// Events must be pumped every frame, even before the sim has produced
	// anything — otherwise the window (and ImGui) can't respond to input.
	handle_events();
	setCaption();

	if (!m_sim_buffer_.has_published())
		return;

	// Always grab the freshest completed simulation frame
	const SimSnapshot& snap = m_sim_buffer_.begin_read();

	// Use the snapshot, not particleManager.stats — that is owned by the
	// sim thread and reading it here is a data race.
	if (snap.stats.iterations_ <= 1)
	{
		m_sim_buffer_.end_read();
		return;
	}

	window.clear(bg_color);

	// NOTE: render_grid() still reads live particleManager state from this
	// thread. Long term, copy the grid data into SimSnapshot instead.
	if (snap.toggles.draw_grid)
		particleManager.render_grid(camera.get_world_mouse_pos());

	renderer.render(snap, camera);

	// ImGui asserts on a zero delta time, so clamp it.
	handle_imGUI(snap, std::max(dt, 1e-5f));

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
		// Without this call ImGui never sees mouse/keyboard input, which is
		// why nothing in the panel could be clicked.
		ImGui::SFML::ProcessEvent(window, *event);
		dispatch_event(*event, cam_pos);
	}

	// ── Held-input tools (Shift + mouse button) ──────────────────────────────
	// Checked once per frame from live input state. They are queued as
	// commands so the sim thread performs the actual particle changes.
	const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
		|| sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

	if (shift && !ImGui::GetIO().WantCaptureMouse)
	{
		if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle))
		{
			SimCommand cmd{ CommandType::AddParticlesAtPoint };
			cmd.x = cam_pos.x;
			cmd.y = cam_pos.y;
			cmd.float_val = 125.f;   // amount
			cmd.radius = 900.f;
			push_command(std::move(cmd));
		}

		if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
		{
			SimCommand cmd{ CommandType::RepelFromPoint };
			cmd.x = cam_pos.x;
			cmd.y = cam_pos.y;
			cmd.float_val = 13.f;    // magnitude
			cmd.radius = 800.f;
			push_command(std::move(cmd));
		}
	}

	const float dt = clock_.get_delta_time();
	camera.update(dt);
}


void Simulation::push_command(SimCommand cmd)
{
	std::lock_guard<std::mutex> lock(m_cmd_mutex);
	m_commands.push(std::move(cmd));
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

		case CommandType::ToggleGrid:
			particleManager.toggles.draw_grid = !particleManager.toggles.draw_grid;
			break;

		case CommandType::ToggleHidePanels:
			particleManager.toggles.hide_panels = !particleManager.toggles.hide_panels;
			break;

			// ── Mouse tools ───────────────────────────────────────────────────────
		case CommandType::AddParticlesAtPoint:
			particleManager.add_particles_at_point(
				sf::Vector2f{ cmd.x, cmd.y }, cmd.float_val, cmd.radius);
			break;

		case CommandType::RepelFromPoint:
			particleManager.repel_system_from_point(
				sf::Vector2f{ cmd.x, cmd.y }, cmd.float_val, cmd.radius);
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

		case CommandType::SetRestitution:
			ResolutionSettings::restitution = cmd.float_val;
			break;

		case CommandType::SetCorrectionFactor:
			ResolutionSettings::correction_factor = cmd.float_val;
			break;

		case CommandType::SetDensity:
			ResolutionSettings::density = cmd.float_val;
			break;

		default:
			break;
		}
	}
}


void Simulation::dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos)
{
	const ImGuiIO& io = ImGui::GetIO();

	if (event.is<sf::Event::Closed>())
		running = false;

	if (const auto* key = event.getIf<sf::Event::KeyPressed>())
	{
		// Don't fire sim hotkeys (Space, G, Q...) while ImGui is using the keyboard.
		if (!io.WantCaptureKeyboard)
			handle_keyboard_events(key->code);
	}
	else if (const auto* scroll = event.getIf<sf::Event::MouseWheelScrolled>())
	{
		if (!io.WantCaptureMouse)  // don't zoom the sim while scrolling over the panel
			camera.zoom(scroll->delta);
	}
	else if (event.is<sf::Event::MouseButtonPressed>())
	{
		if (!io.WantCaptureMouse)  // don't pan the sim when clicking the panel
			handle_mouse_press(cam_pos);
	}
	else if (event.is<sf::Event::MouseButtonReleased>())
	{
		// Always release, so a pan can never get stuck if the cursor
		// ends up over the panel.
		handle_mouse_release();
	}
}

void Simulation::handle_pause_toggle()
{
	// (The old `bool& paused = paused;` declared a new reference initialised
	// with itself, so the member was never actually toggled.)
	paused.store(!paused.load());
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
	case sf::Keyboard::Key::Escape: running = false;                 break;
	case sf::Keyboard::Key::Space:  handle_pause_toggle();           break;

		// Toggles are owned by the sim thread, so go through the command queue
		// instead of writing particleManager.toggles from the render thread.
	case sf::Keyboard::Key::G: push_command({ CommandType::ToggleGrid });        break;
	case sf::Keyboard::Key::Q: push_command({ CommandType::ToggleHidePanels });  break;

	default: break;
	}
}