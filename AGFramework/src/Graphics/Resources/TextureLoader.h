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

	struct SceneTextureSource
	{
		enum class Kind
		{
			DdsFile,
			DecodedRgba
		};

		Kind SourceKind = Kind::DecodedRgba;
		std::wstring Filename;
		ImageData DecodedImage;
	};

	static ImageData LoadImage(const std::wstring &filename);
	static ImageData LoadUncompressedTga(const std::wstring &filename);
	static SceneTextureSource LoadSceneTexture(const std::wstring &filename);
};
