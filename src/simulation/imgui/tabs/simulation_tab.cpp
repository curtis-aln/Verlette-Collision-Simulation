#include "../../../collision_resolver/collision_resolver.h"
#include "../../../settings.h"
#include "simulation_tab.h"

#include <cstdio>
#include <imgui.h>
#include <particle_system/state.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>

void SimulationTab::draw(const SimSnapshot& snap, SimCtx& ctx)
{
	if (m_first_draw_)
	{
		m_thread_count_ = static_cast<int>(ParticleSettings::initial_thread_count);
		m_first_draw_ = false;
	}

	// ══ THREADING ═════════════════════════════════════════════════════════════
	section_header("THREADING");

	ImGui::PushStyleColor(ImGuiCol_Text, { 0.52f, 0.52f, 0.66f, 1.f });
	ImGui::TextUnformatted("Worker Threads");
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::SliderInt("##threads", &m_thread_count_, 1, 64))
		ctx.push({ CommandType::SetThreadCount,{}, 0.f, m_thread_count_ });

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Text, { 0.38f, 0.38f, 0.50f, 1.f });
	ImGui::TextWrapped("Changes take effect next frame. "
		"High counts can increase contention on small particle counts.");
	ImGui::PopStyleColor();

	// ══ SPATIAL GRID ══════════════════════════════════════════════════════════
	section_header("SPATIAL GRID");

	// Grid update frequency (already a non-const int, safe to write directly)
	ImGui::PushStyleColor(ImGuiCol_Text, { 0.52f, 0.52f, 0.66f, 1.f });
	ImGui::TextUnformatted("Update Every N Frames");
	ImGui::PopStyleColor();

	// Read-only grid dimensions (constexpr — displayed for reference)
	auto const_row = [](const char* label, const char* val)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, { 0.52f, 0.52f, 0.66f, 1.f });
			ImGui::Text("%-20s", label);
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, { 0.62f, 0.62f, 0.72f, 1.f });
			ImGui::TextUnformatted(val);
			ImGui::PopStyleColor();
		};

	char buf[32];

	std::snprintf(buf, sizeof(buf), "%zu", ResolutionSettings::cells_x);
	const_row("Grid Cells X", buf);

	std::snprintf(buf, sizeof(buf), "%zu", ResolutionSettings::cells_y);
	const_row("Grid Cells Y", buf);

	std::snprintf(buf, sizeof(buf), "%d", ResolutionSettings::cell_max_capacity);
	const_row("Cell Capacity", buf);

	thin_sep();

	ImGui::PushStyleColor(ImGuiCol_Text, { 0.38f, 0.38f, 0.50f, 1.f });
	ImGui::TextWrapped("Grid dimensions are compile-time constants. "
		"Rebuild the project to change them in settings.h.");
	ImGui::PopStyleColor();

	// ══ WORLD CONSTANTS ═══════════════════════════════════════════════════════
	section_header("WORLD CONSTANTS");

	std::snprintf(buf, sizeof(buf), "%u", snap.stats.cell_particle_count);
	const_row("Particle Count", buf);

	const float ww = static_cast<float>(ParticleSettings::world_width);
	const float wh = static_cast<float>(ParticleSettings::world_height);
	std::snprintf(buf, sizeof(buf), "%.0f x %.0f", ww, wh);
	const_row("World Size", buf);

	// ══ COLLISION SETTINGS ═══════════════════════════════════════════════════════
	section_header("COLLISION SETTINGS");

	if (ImGui::SliderFloat("##restitution", &restitution, 0.f, 1.f, "Restitution: %.3f"))
		ctx.push({ CommandType::SetRestitution,{}, restitution, 0 });

	if (ImGui::SliderFloat("##correction_factor", &correction_factor, 0.f, 1.f, "Correction Factor: %.3f"))
		ctx.push({ CommandType::SetCorrectionFactor,{}, correction_factor, 0 });

	if (ImGui::SliderFloat("##density", &density, 1.f, 1000.f, "Density: %.1f"))
		ctx.push({ CommandType::SetDensity,{}, density, 0 });

	// ══ ENVIRONMENT SETTINGS ══════════════════════════════════════════════════════
	section_header("ENVIRONMENT SETTINGS");
	toggle(ctx, "Surface Gravity", &WorldToggles::surface_gravity);
	if (ctx.toggles.surface_gravity) // inline slider to control strength of surface gravity
		if (ImGui::SliderFloat("##surface_gravity_strength", &surface_gravity_const, 0.f, 0.5f, "Surface Gravity Strength: %.3f"))
			ctx.push({ CommandType::SetSurfaceGravity,{}, surface_gravity_const, 0 });

	toggle(ctx, "Central Gravity", &WorldToggles::central_gravity);
	if (ctx.toggles.central_gravity) // inline slider to control strength of central gravity
		if (ImGui::SliderFloat("##central_gravity_strength", &central_gravity_const, 0.f, 10000000.f, "Central Gravity Strength: %.3f"))
			ctx.push({ CommandType::SetCentralGravity,{}, central_gravity_const, 0 });

	toggle(ctx, "Random Jitter", &WorldToggles::random_jitter);
	if (ctx.toggles.random_jitter) // inline slider to control strength of random jitter
		if (ImGui::SliderFloat("##random_jitter_strength", &random_jitter_const, 0.f, 5.f, "Random Jitter Strength: %.3f"))
			ctx.push({ CommandType::SetRandomJitter,{}, random_jitter_const, 0 });


	// ══ LIVE PERFORMANCE ══════════════════════════════════════════════════════
	section_header("PERFORMANCE");

	fps_row("Render FPS", snap.stats.fps);
	fps_row("Update FPS", snap.stats.updating_fps);
	stat_row("Sim tick", "%.3f s", snap.sim_tick_seconds);
	stat_row("Iterations", "%d", snap.stats.iterations_);
}