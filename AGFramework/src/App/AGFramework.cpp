#include "AGFramework.h"

#include <stdexcept>

AGFramework::AGFramework() : m_isPaused(false)
{
	m_window = std::make_shared<Window>();
	m_renderingSystem = std::make_unique<RenderingSystem>();
}

void AGFramework::Initialize()
{
	if (!m_window->Create(m_title, 1920, 1080))
		throw std::runtime_error("Failed to create window");

	m_hWnd = m_window->GetHandle();
	m_hInstance = m_window->GetInstanceHandle();

	m_inputDevice = std::make_unique<InputDevice>(m_hWnd);

	// Input callbacks
	m_window->OnRawKey.AddLambda([this](InputDevice::KeyboardInputEventArgs args) {
		m_inputDevice->HandleKeyboardInput(args);
		});

	m_window->OnRawMouse.AddLambda([this](InputDevice::RawMouseEventArgs args) {
		m_inputDevice->HandleMouseInput(args);
		});

	m_window->OnPause.AddLambda([this](bool isPaused) {
		m_isPaused = isPaused;
		if (m_isPaused) m_timer.Stop();
		else m_timer.Start();
		});

	// Resize handler
	m_window->OnResize.AddLambda([this](int w, int h) {
		OnWindowResized(w, h);
		});

	m_window->OnClose.AddLambda([](bool& canClose) { canClose = true; });

	m_window->Show();

	if (!m_renderingSystem->Initialize(m_hInstance, m_hWnd))
		throw std::runtime_error("Failed to initialize rendering system");
}

void AGFramework::Run()
{
	m_timer.Reset();

	while (!m_window->ShouldClose())
	{
		m_timer.Tick();
		m_window->ProcessMessages();

		if (m_window->ShouldClose())
		{
			break;
		}

		if (m_isPaused)
		{
			Sleep(100);
			continue;
		}

		CalculateFrameStats();
		Update();
		Draw();
	}

	Shutdown();
}

void AGFramework::Shutdown()
{
	if (m_renderingSystem)
		m_renderingSystem->Shutdown();
}

void AGFramework::Update()
{
	if (m_renderingSystem && !m_isPaused)
	{
		m_renderingSystem->Update(m_timer);
	}
}

void AGFramework::Draw()
{
	if (m_renderingSystem && !m_isPaused)
		m_renderingSystem->Render(m_timer);
}

void AGFramework::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	if ((m_timer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;

		std::wstring windowText = L"Another Graphics Framework | FPS: "
			+ std::to_wstring(fps) + L" | mspf: " + std::to_wstring(mspf);

		SetWindowText(m_hWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void AGFramework::OnWindowResized(int width, int height)
{
	if (m_renderingSystem)
		m_renderingSystem->OnWindowResize(width, height);
}
