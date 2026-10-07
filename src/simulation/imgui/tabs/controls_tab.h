#pragma once
#include "i_tab.h"

// Read-only reference tab: lists every keyboard / mouse control in the sim.
class ControlsTab final : public ITab
{
public:
	const char* label() const override { return "Controls"; }
	void draw(const SimSnapshot& snap, SimCtx& ctx) override;
};