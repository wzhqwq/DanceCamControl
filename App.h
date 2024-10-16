#pragma once
#include "MyCapture.h"
#include <thread>
#include "ParameterManager.h"

enum AppStage
{
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
	App() = default;
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

	bool CheckSign(const cv::Mat image);
	std::vector<std::vector<cv::Point>> GetPlaylistContours(const cv::Mat image);
	cv::Point GetPlaylistCenter(const std::vector<cv::Point>& contour);

private:
	winrt::Windows::UI::Composition::Compositor m_compositor{ nullptr };
	winrt::Windows::UI::Composition::ContainerVisual m_root{ nullptr };
	winrt::Windows::UI::Composition::SpriteVisual m_visual{ nullptr };
	winrt::Windows::UI::Composition::CompositionSurfaceBrush m_brush{ nullptr };

	std::unique_ptr<MyCapture> m_capture{ nullptr };
	std::unique_ptr<ParameterManager> m_paraMgr{ nullptr };

	std::thread m_thread;

	std::atomic<bool> m_terminated = false;
	std::atomic<AppStage> m_stage = WaitForWindow;
};

