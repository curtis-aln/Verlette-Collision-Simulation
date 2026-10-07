#pragma once
#include <mutex>
#include <queue>
#include <utility>

#include "../../particle_system/state.h"

enum class CommandType
{
	// ── Toggle state ──────────────────────────────────────────────────────
	SetToggles,             // toggles field carries the full new WorldToggles
	ToggleGrid,             // flip draw_grid on the sim thread  ([G])
	ToggleHidePanels,       // flip hide_panels on the sim thread ([Q])

	// ── One-shot actions ──────────────────────────────────────────────────
	ResetSimulation,        // Wired: call particle_system_.init_grid_positioning()

	// ── Mouse tools (run on the sim thread, not the render thread) ────────
	AddParticlesAtPoint,    // x, y = world pos, float_val = amount, radius
	RepelFromPoint,         // x, y = world pos, float_val = magnitude, radius

	// ── Physics ───────────────────────────────────────────────────────────
	SetRestitution,
	SetCorrectionFactor,
	SetDensity,

	SetSurfaceGravity,
	SetCentralGravity,
	SetRandomJitter,

	// Density grid
	SetDensityCellSize,
	SetGaussianSigma,
	SetBoxFilterCascadePasses,
	SetSinSign,
	SetCosSign,

	// ── World ─────────────────────────────────────────────────────────────
	RandomizeSimulation,
	ClearBeacons,
	SetThreadCount,
};

struct SimCommand
{
	CommandType  type;
	WorldToggles toggles{};
	float        float_val = 0.f;
	int          int_val = 0;
	bool         bool_val = false;

	// Appended at the end so existing positional initialisers still work.
	float        x = 0.f;
	float        y = 0.f;
	float        radius = 0.f;
};

// Renamed from ImGuiContext — the original name collides with ImGui's own
// internal ImGuiContext type, causing silent ODR issues with ImGui headers.
struct SimCtx
{
	WorldToggles& toggles;    // mutable copy for this frame — write freely
	std::mutex& cmd_mutex;
	std::queue<SimCommand>& commands;

	void push(SimCommand cmd) const
	{
		std::lock_guard<std::mutex> lock(cmd_mutex);
		commands.push(std::move(cmd));
	}
};