#pragma once

#include "TextureLoader.h"
#include "../dx12/d3dUtil.h"

class DirectX12Context;

class ResourceUploader
{
  public:
	static void UploadTexture2D(DirectX12Context &context, Texture &texture, const void *pixelData, UINT width, UINT height,
	                            DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
	static void UploadSceneTexture(DirectX12Context &context, Texture &texture, const TextureLoader::SceneTextureSource &source,
	                               DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

	static void UploadTextureCube(DirectX12Context &context, Texture &texture, const void *facePixelData, UINT faceWidth, UINT faceHeight,
	                              DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
};
