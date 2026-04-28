#include "AGFramework.h"
#include <chrono>
#include <iostream>

using namespace std;

AGFramework::AGFramework() : m_running(false), m_isPaused(false)
{
	m_window = std::make_shared<Window>();
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
		m_inputDevice->OnKeyDown(args);
		});

	m_window->OnRawMouse.AddLambda([this](InputDevice::RawMouseEventArgs args) {
		m_inputDevice->OnMouseMove(args);
		});

	m_window->OnPause.AddLambda([this](bool isPaused) {
		m_isPaused = isPaused;
		if (m_isPaused) m_timer.Stop();
		else m_timer.Start();
		});

	m_inputDevice->MouseMove.AddLambda([](const InputDevice::MouseMoveEventArgs& args) {
		std::cout << "Mouse: (" << args.Position.x << ", " << args.Position.y
			<< ") Offset: (" << args.Offset.x << ", " << args.Offset.y
			<< ") Wheel: " << args.WheelDelta << "\n";
		});

	// Resize handler
	m_window->OnResize.AddLambda([this](int w, int h) {
		OnWindowResized(w, h);
		});

	m_window->OnClose.AddLambda([](bool& canClose) { canClose = true; });

	m_window->Show();
	m_running = true;

	// Initialize DirectX12
	m_renderer = std::make_unique<DirectX12App>(m_hInstance, m_hWnd);
	if (!m_renderer->Initialize())
		throw std::runtime_error("Failed to initialize DirectX12");

	m_renderer->OnResize(); // Initial resize
}

void AGFramework::Run()
{
	m_timer.Reset();

	while (m_running && !m_window->ShouldClose())
	{
		m_timer.Tick();

		if (!m_window->ProcessMessages())
		{
			CalculateFrameStats();

			if (!m_isPaused)
			{
				Update();
				Render();
			}
			else
			{
				Sleep(100);
			}
		}
	}

	Shutdown();
}

void AGFramework::Shutdown()
{
	if (m_renderer)
		m_renderer.reset();
}

void AGFramework::Update()
{
	if (m_renderer && !m_isPaused)
	{
		m_renderer->Update(m_timer);
	}
}

void AGFramework::Render()
{
	if (m_renderer && !m_isPaused)
		m_renderer->Draw(m_timer);
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
	if (m_renderer)
		m_renderer->OnWindowResize(width, height);
}
