#pragma once
#include <opencv2/core.hpp>

enum class GameCaptureState
{
	Idle,
	Requested,
	Processing,
	Finished,
	Closed,
};

class GameCapture
{
public:
	virtual WindowRect GetGameTotalRect();
	virtual void Initialize(std::function<WindowRect(WindowRect)> calculateCaptureRect);
	virtual void StartCapture();
	virtual void StopCapture();

	bool IsClosed() const;
	cv::Mat RequestCapture();

protected:
	std::atomic<GameCaptureState> m_state = GameCaptureState::Idle;

	std::function<WindowRect(WindowRect)> m_calculateCaptureRect;

	uint8_t* m_capturedImage{ nullptr };
	WindowRect m_captureRect;
};

