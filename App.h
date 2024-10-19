#pragma once
#include "MyCapture.h"
#include <thread>
#include "ParameterManager.h"
#include "Canvas.h"
#include "d3dHelpers.h"

enum AppStage
{
	Initializing,
	WaitForWindow,
	WaitForSign,
	WaitForPlaylist,
	FixAngle,
	FixDistance,
	StereoAlign,
};

class App
{
public:
	App() : m_d3dDevice(CreateD3DDevice()) {
		m_canvas = std::make_unique<Canvas>(m_d3dDevice);
		m_capture = std::make_unique<MyCapture>("VRChat", "UnityWndClass", m_d3dDevice);
		m_paraMgr = std::make_unique<ParameterManager>();
	};
	~App() {
		Terminate();
	}

	void Initialize(winrt::Windows::UI::Composition::ContainerVisual const& root);

	void Start();
	void Terminate();

private:
	void StateMachine();

	void TryCapture();
	void TryMatchSign();
	void TryFindPlaylist();
	void DoFixAngle();
	void DoFixDistance();
	void DoStereoAlign();

	bool CheckSign(const cv::Mat image);
	std::vector<std::vector<cv::Point>> GetPlaylistContours(const cv::Mat image);
	cv::Point GetPlaylistCenter(const std::vector<cv::Point>& contour);

private:
	winrt::com_ptr<ID3D11Device> m_d3dDevice;

	std::unique_ptr<MyCapture> m_capture;
	std::unique_ptr<Canvas> m_canvas;
	std::unique_ptr<ParameterManager> m_paraMgr;

	std::thread m_thread;

	std::atomic<bool> m_terminated = false;
	std::atomic<AppStage> m_stage = WaitForWindow;
};

