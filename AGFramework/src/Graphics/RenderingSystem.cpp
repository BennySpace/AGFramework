#include "RenderingSystem.h"

#include "../Core/GameTimer.h"

RenderingSystem::~RenderingSystem() = default;

bool RenderingSystem::Initialize(HINSTANCE hInstance, HWND hWnd)
{
    if (m_renderer)
    {
        return true;
    }

    m_renderer = std::make_unique<DirectX12App>(hInstance, hWnd);
    if (!m_renderer->Initialize())
    {
        m_renderer.reset();
        return false;
    }

    return true;
}

void RenderingSystem::Update(const GameTimer& gameTimer)
{
    if (m_renderer)
    {
        m_renderer->Update(gameTimer);
    }
}

void RenderingSystem::Render(const GameTimer& gameTimer)
{
    if (m_renderer)
    {
        m_renderer->Draw(gameTimer);
    }
}

void RenderingSystem::OnWindowResize(int width, int height)
{
    if (m_renderer)
    {
        m_renderer->OnWindowResize(width, height);
    }
}

void RenderingSystem::Shutdown()
{
    m_renderer.reset();
}
