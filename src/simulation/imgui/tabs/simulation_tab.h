#pragma once
#include "i_tab.h"
#include <collision_resolver/collision_resolver.h>
#include <settings.h>
#include <simulation/context/sim_command.h>
#include <simulation/context/sim_snapshot.h>

class SimulationTab final : public ITab
{
public:
	const char* label() const override { return "Simulation"; }
	void draw(const SimSnapshot& snap, SimCtx& ctx) override;

private:
	int   m_thread_count_ = static_cast<int>(ParticleSettings::initial_thread_count);
	bool  m_first_draw_ = true;

	float restitution = ResolutionSettings::restitution;
	float correction_factor = ResolutionSettings::correction_factor;
	float density = ResolutionSettings::density;
};