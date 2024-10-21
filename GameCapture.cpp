#include "framework.h"
#include "GameCapture.h"

using namespace std;

WindowRect GameCapture::GetGameTotalRect()
{
	throw logic_error("The method is not implemented.");
}

void GameCapture::Initialize(std::function<WindowRect(WindowRect)> calculateCaptureRect)
{
	throw logic_error("The method is not implemented.");
}

void GameCapture::StartCapture()
{
	throw logic_error("The method is not implemented.");
}

void GameCapture::StopCapture()
{
	throw logic_error("The method is not implemented.");
}

bool GameCapture::IsClosed() const
{
	return m_state == GameCaptureState::Closed;
}

cv::Mat GameCapture::RequestCapture() {
	m_state = GameCaptureState::Requested;
	while (m_state != GameCaptureState::Closed && m_state != GameCaptureState::Finished) {
		this_thread::sleep_for(chrono::milliseconds(10));
	}
	if (m_state != GameCaptureState::Finished) {
		return cv::Mat(m_captureRect.height, m_captureRect.width, CV_8UC4, cv::Scalar(0, 0, 0, 255));
	}
	m_state = GameCaptureState::Idle;
	return cv::Mat(m_captureRect.height, m_captureRect.width, CV_8UC4, m_capturedImage);
}
