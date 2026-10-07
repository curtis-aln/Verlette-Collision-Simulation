#include "controls_tab.h"

#include <imgui.h>
#include <initializer_list>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>

namespace
{
	struct ControlRow
	{
		const char* input;    // key or mouse input
		const char* action;   // what it does
	};

	const ImVec4 kInputCol{ 0.55f, 0.72f, 1.00f, 1.f };   // bright blue, matches the [key] hints
	const ImVec4 kActionCol{ 0.88f, 0.88f, 0.88f, 1.f };
	const ImVec4 kNoteCol{ 0.38f, 0.38f, 0.50f, 1.f };    // muted, same as other tabs' notes

	// Two-column table: input on the left, wrapped description on the right.
	void controls_table(const char* id, std::initializer_list<ControlRow> rows)
	{
		if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_BordersInnerH))
			return;

		ImGui::TableSetupColumn("input", ImGuiTableColumnFlags_WidthFixed, 104.f);
		ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthStretch);

		for (const ControlRow& row : rows)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::PushStyleColor(ImGuiCol_Text, kInputCol);
			ImGui::TextWrapped("%s", row.input);
			ImGui::PopStyleColor();

			ImGui::TableSetColumnIndex(1);
			ImGui::PushStyleColor(ImGuiCol_Text, kActionCol);
			ImGui::TextWrapped("%s", row.action);
			ImGui::PopStyleColor();
		}

		ImGui::EndTable();
	}
}

void ControlsTab::draw(const SimSnapshot& snap, SimCtx& /*ctx*/)
{
	// ══ CAMERA ════════════════════════════════════════════════════════════════
	section_header("CAMERA");
	controls_table("##tbl_camera", {
		{ "Left drag", "Hold the left mouse button and drag to pan the view." },
		{ "Scroll",    "Zoom in and out." },
		});

	// ══ SIMULATION ════════════════════════════════════════════════════════════
	section_header("SIMULATION");
	controls_table("##tbl_sim", {
		{ "Space",  "Pause or resume the physics. You can still pan and zoom while paused." },
		{ "G",      "Show or hide the spatial hash grid overlay." },
		{ "Escape", "Quit the simulation." },
		});

	// ══ PARTICLE TOOLS ════════════════════════════════════════════════════════
	section_header("PARTICLE TOOLS");
	controls_table("##tbl_tools", {
		{ "Shift + Middle", "Hold both to continuously spawn new particles around the cursor." },
		{ "Shift + Right",  "Hold both to push nearby particles away from the cursor." },
		});

	// ══ INTERFACE ═════════════════════════════════════════════════════════════
	section_header("INTERFACE");
	controls_table("##tbl_ui", {
		{ "Q",            "Hide or show every panel. Press Q again to bring them back." },
		{ "F1",           "Hide or show this control panel. While hidden, a small \"Show panel\" button appears top-left." },
		{ "Arrow button", "Minimize the panel to its header. Click again to expand." },
		{ "X button",     "Hide this panel (restore with F1 or the \"Show panel\" button)." },
		});

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Text, kNoteCol);
	ImGui::TextWrapped("Mouse input over this panel does not reach the simulation, "
		"and hotkeys are ignored while a panel widget has keyboard focus.");
	ImGui::PopStyleColor();

	// ══ STATUS ════════════════════════════════════════════════════════════════
	section_header("STATUS");
	indicator_row(snap.toggles.draw_grid, "Spatial grid overlay", "G");
}