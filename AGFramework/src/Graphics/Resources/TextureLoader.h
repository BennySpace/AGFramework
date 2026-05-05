#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

class TextureLoader
{
public:
	struct ImageData
	{
		UINT Width = 0;
		UINT Height = 0;
		std::vector<std::uint8_t> Pixels;
	};

	static ImageData LoadUncompressedTga(const std::wstring& filename);
};
