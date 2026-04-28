#include "Renderer.h"

std::unique_ptr<Renderer> RendererFactory::CreateRenderer(RenderAPI api)
{
    (void)api;
    return nullptr;
}
