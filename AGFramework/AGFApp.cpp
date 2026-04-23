#include "AGFApp.h"
#include <chrono>
#include <iostream>

using namespace std;

AGFApp::AGFApp() : m_running(false) {
	m_window = std::make_shared<Window>();
}

void AGFApp::OnWindowResized(int width, int height) {

}

void AGFApp::Initialize() {
	if (!m_window->Create(m_title, 1920, 1080)) {
		throw std::runtime_error("Failed to create window");
	}

	m_hWnd = m_window->GetHandle();
	m_hInstance = m_window->GetInstanceHandle();
	m_inputDevice = std::make_unique<InputDevice>(m_hWnd);

	m_window->OnRawKey.AddLambda([this](InputDevice::KeyboardInputEventArgs args) {
		m_inputDevice->OnKeyDown(args);
		});
	m_window->OnRawMouse.AddLambda([this](InputDevice::RawMouseEventArgs args) {
		m_inputDevice->OnMouseMove(args);
		});

	m_window->OnPause.AddLambda([this](bool isPaused)
		{
			m_isPaused = isPaused;
			if (m_isPaused)  m_timer.Stop();
			else m_timer.Start();
		});

	m_inputDevice->MouseMove.AddLambda([this](const InputDevice::MouseMoveEventArgs& args) {
		std::cout << "Mouse moved to: (" << args.Position.x << ", " << args.Position.y
			<< "), Offset: (" << args.Offset.x << ", " << args.Offset.y
			<< "), Wheel: " << args.WheelDelta << "\n";
		});

	m_window->OnResize.AddLambda([this](int w, int h) { OnWindowResized(w, h); });
	m_window->OnClose.AddLambda([](bool& canClose) { canClose = true; });

	m_window->Show();
	m_running = true;

	m_renderer = std::make_unique<DirectX12App>(m_hInstance, m_hWnd);
	m_renderer->Initialize();
}

void AGFApp::Update()
{

}

void AGFApp::Render()
{

}

void AGFApp::Shutdown()
{
}

void AGFApp::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	if ((m_timer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText =
			L"Another Graphic Framework    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(m_hWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 0.1f;
	}
}

void AGFApp::Run() {
	m_timer.Reset();

	while (m_running && !m_window->ShouldClose()) {
		m_timer.Tick();
		if (!m_window->ProcessMessages())
		{
			if (!m_isPaused)
			{
				CalculateFrameStats();
				m_renderer->Update(m_timer);
				m_renderer->Draw(m_timer);
			}
			else
			{
				Sleep(100);
			}

		}
	}
}
