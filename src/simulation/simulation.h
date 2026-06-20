#pragma once
#include <SFML/Graphics.hpp>

#include "../settings.h"
#include "../entity.hpp"
#include "../spatial_grid/simple_spatial_grid.h"

#include "../utilities/random.h"
#include "../utilities/camera.h"
#include "../utilities/smooth_frame_rates.h"

#include "particle_manager/particle_manager.h"

#include "context/sim_command.h"
#include "context/sim_snapshot.h"
#include "context/triple_buffer.h"

#include "imgui/control_panel.h"

inline static constexpr int frame_smoothing_count = 30;

class Simulation : SimulationSettings
{
	sf::Vector2u size = { static_cast<unsigned int>(screen_width), static_cast<unsigned int>(screen_height) };
	sf::RenderWindow window{ sf::VideoMode(size), "Collision Detection", sf::Style::Default };

	bool paused = false;
	bool draw_grid = false;
	bool mousePressed = false;
	unsigned long long frameCount = 0;
	sf::Vector2f mousePosition{};

	sf::Rect<float> border{ {0.0f, 0.0f}, {ParticleSettings::world_width, ParticleSettings::world_height} };

	ParticleManager particleManager{ &window, &border };

	sf::VertexArray v_array{};
	
	FrameRateSmoothing<frame_smoothing_count> clock_{};
	Camera camera{ &window, 1.f / scale_factor };

	// ── Triple-buffer (sim → render, lock-free) ───────────────────────────────
	TripleBuffer<SimSnapshot> m_sim_buffer_{ ParticleSettings::particle_count };

	// ── Command queue (render → sim, mutex-protected) ─────────────────────────
	std::mutex             m_cmd_mutex{};
	std::queue<SimCommand> m_commands{};

	// ── Threading ─────────────────────────────────────────────────────────────
	std::thread       m_sim_thread_;
	std::atomic<bool> running{ true };

	// ── ImGui ─────────────────────────────────────────────────────────────────
	ControlPanel m_control_panel_;

public:
	Simulation();
	void run();

private:
	void update();
	void render();

	void setCaption();
	void handle_events();
	void dispatch_event(const sf::Event& event, const sf::Vector2f& cam_pos);
	void handle_pause_toggle();
	void handle_mouse_press(const sf::Vector2f& cam_pos);
	void handle_mouse_release();
	void handle_keyboard_events(const sf::Keyboard::Key& event_key_code);


	void handle_imGUI(const SimSnapshot& snap, float dt);
};