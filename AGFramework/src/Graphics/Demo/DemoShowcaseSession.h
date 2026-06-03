#pragma once

#include "../../Math/MathHelper.h"

namespace Demo
{
class DemoShowcaseSession
{
  public:
	const DirectX::XMFLOAT3 &GetPbrGridOffset() const
	{
		return m_pbrGridOffset;
	}

	void SetPbrGridOffset(const DirectX::XMFLOAT3 &offset)
	{
		m_pbrGridOffset = offset;
	}

  private:
	DirectX::XMFLOAT3 m_pbrGridOffset = {0.0f, 0.0f, 0.0f};
};
} // namespace Demo
