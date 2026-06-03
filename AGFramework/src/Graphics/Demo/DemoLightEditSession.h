#pragma once

#include "../LightSystem.h"

namespace Demo
{
class DemoLightEditSession
{
  public:
	using State = LightSystem::EditState;

	const State &GetState() const
	{
		return m_state;
	}

	void SetState(const State &state)
	{
		m_state = state;
	}

  private:
	State m_state;
};
} // namespace Demo
