#pragma once
#include "../context/sim_command.h"
#include "../context/sim_snapshot.h"
#include "tabs/i_tab.h"
#include <memory>
#include <vector>


class ControlPanel
{
public:
	ControlPanel();

	void draw(const SimSnapshot& snap, SimCtx& ctx, float dt);

private:
	std::vector<std::unique_ptr<ITab>> m_tabs_;

	bool m_collapsed_ = false;
	bool m_visible_ = true;
};