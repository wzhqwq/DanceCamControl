#pragma once
#include "GameCapture.h"
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
	App();
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
	std::vector<cv::Point> GetPlaylistCorners(const std::vector<cv::Point>& contour);

private:
	winrt::com_ptr<ID3D11Device> m_d3dDevice;

	std::unique_ptr<GameCapture> m_capture;
	std::unique_ptr<Canvas> m_canvas;
	std::unique_ptr<ParameterManager> m_paraMgr;

	std::thread m_thread;

	std::atomic<bool> m_terminated = false;
	std::atomic<AppStage> m_stage = WaitForWindow;

	cv::Mat m_lastMask;
	bool m_maskChanged = false;

	// distance fixing
	bool m_blankBackRetried = false;
	float m_nextStep = 3.0f;
};

