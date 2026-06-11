#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>

struct ModelDrawItem
{
	std::string DrawName;
	std::string MaterialName;
	std::string DiffuseTexturePath;
	std::string NormalTexturePath;
	std::string OrmTexturePath;
	std::string OpacityTexturePath;
	DirectX::XMFLOAT4 PositionOffset = {0.0f, 0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT4 PbrParams = {0.0f, 0.58f, 1.0f, 0.95f};
	std::uint32_t DiffuseSrvHeapIndex = 0;
	std::uint32_t NormalSrvHeapIndex = 0;
	std::uint32_t OrmSrvHeapIndex = 0;
	std::uint32_t OpacitySrvHeapIndex = 0;
	DirectX::XMFLOAT4 TextureFlags = {0.0f, 0.0f, 0.0f, 0.0f};
	bool HasAlphaCutout = false;
	bool CastShadows = true;
	bool IsDemoPbrGrid = false;
};
