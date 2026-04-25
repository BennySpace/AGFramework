#include "Renderer.h"

std::unique_ptr<Renderer> RendererFactory::CreateRenderer(RenderAPI api)
{
    switch (api)
    {
    // case RenderAPI::DirectX12:
    //     return std::make_unique<Renderer>(DirectX12Renderer);
    // case RenderAPI::Vulkan:
    //     return std::unique_ptr<Renderer>(&VulkanRenderer::GetInstance());
    default:
        return nullptr;
    }
}
