#pragma once
#include "GameCapture.h"
#include "SpoutDX.h"
#include <thread>
#include <regex>

class SpoutCapture : public GameCapture
{
public:
	SpoutCapture(std::regex senderPattern);

	WindowRect GetGameTotalRect();
	void Initialize(std::function<WindowRect(WindowRect)> calculateCaptureRect);
	void StartCapture();
	void StopCapture();

private:
	void CaptureThread();
	void FindSender();
	void UpdateTotalRect();

private:
	std::regex m_senderPattern;
	std::shared_ptr<spoutDX> m_receiver;
	std::thread m_thread;

	uint8_t* m_totalCapture{ nullptr };
	uint m_totalWidth;
	uint m_totalHeight;

	std::atomic<bool> m_closed = true;
};

