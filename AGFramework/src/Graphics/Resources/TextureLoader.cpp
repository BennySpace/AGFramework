#include "TextureLoader.h"

#include <array>
#include <fstream>
#include <stdexcept>

namespace
{
	std::uint16_t ReadUInt16LE(const std::uint8_t* bytes)
	{
		return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
	}
}

TextureLoader::ImageData TextureLoader::LoadUncompressedTga(const std::wstring& filename)
{
	std::ifstream input(filename, std::ios::binary);
	if (!input)
	{
		throw std::runtime_error("Failed to open TGA texture file.");
	}

	std::array<std::uint8_t, 18> header{};
	input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
	if (!input)
	{
		throw std::runtime_error("Failed to read TGA header.");
	}

	const std::uint8_t idLength = header[0];
	const std::uint8_t colorMapType = header[1];
	const std::uint8_t imageType = header[2];
	const std::uint16_t width = ReadUInt16LE(&header[12]);
	const std::uint16_t height = ReadUInt16LE(&header[14]);
	const std::uint8_t bitsPerPixel = header[16];
	const std::uint8_t imageDescriptor = header[17];

	if (colorMapType != 0 || imageType != 2)
	{
		throw std::runtime_error("Only uncompressed true-color TGA textures are supported.");
	}

	if (bitsPerPixel != 24 && bitsPerPixel != 32)
	{
		throw std::runtime_error("Unsupported TGA pixel format.");
	}

	if (idLength > 0)
	{
		input.seekg(idLength, std::ios::cur);
	}

	const UINT srcPixelSize = bitsPerPixel / 8;
	const size_t srcDataSize = static_cast<size_t>(width) * height * srcPixelSize;
	std::vector<std::uint8_t> srcPixels(srcDataSize);
	input.read(reinterpret_cast<char*>(srcPixels.data()), static_cast<std::streamsize>(srcDataSize));
	if (!input)
	{
		throw std::runtime_error("Failed to read TGA pixel data.");
	}

	ImageData textureData;
	textureData.Width = width;
	textureData.Height = height;
	textureData.Pixels.resize(static_cast<size_t>(width) * height * 4);

	const bool topLeftOrigin = (imageDescriptor & 0x20) != 0;

	for (UINT y = 0; y < height; ++y)
	{
		const UINT srcY = topLeftOrigin ? y : (height - 1 - y);
		for (UINT x = 0; x < width; ++x)
		{
			const size_t srcIndex = (static_cast<size_t>(srcY) * width + x) * srcPixelSize;
			const size_t dstIndex = (static_cast<size_t>(y) * width + x) * 4;

			textureData.Pixels[dstIndex + 0] = srcPixels[srcIndex + 2];
			textureData.Pixels[dstIndex + 1] = srcPixels[srcIndex + 1];
			textureData.Pixels[dstIndex + 2] = srcPixels[srcIndex + 0];
			textureData.Pixels[dstIndex + 3] = (srcPixelSize == 4) ? srcPixels[srcIndex + 3] : 255;
		}
	}

	return textureData;
}
