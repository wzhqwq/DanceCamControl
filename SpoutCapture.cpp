#include "framework.h"
#include "SpoutCapture.h"

using namespace std;

SpoutCapture::SpoutCapture(regex senderPattern)
	: m_receiver(make_shared<spoutDX>()), m_senderPattern(senderPattern)
{
	m_receiver->SetSwap(true);
}

WindowRect SpoutCapture::GetGameTotalRect()
{
	FindSender();
	return { 0, 0, (int) m_receiver->GetSenderWidth(), (int)m_receiver->GetSenderHeight() };
}

void SpoutCapture::Initialize(function<WindowRect(WindowRect)> calculateCaptureRect)
{
	m_calculateCaptureRect = calculateCaptureRect;
	UpdateTotalRect();
}

void SpoutCapture::StartCapture()
{
	if (!m_receiver->OpenDirectX11()) {
		throw runtime_error("Failed to open DirectX11.");
	}
	m_thread = thread(&SpoutCapture::CaptureThread, this);
}

void SpoutCapture::StopCapture()
{
	m_state = GameCaptureState::Closed;
	m_thread.join();
	m_receiver->ReleaseReceiver();
	m_receiver->CloseDirectX11();
}

void SpoutCapture::CaptureThread()
{
	while (m_state != GameCaptureState::Closed) {
		if (m_state != GameCaptureState::Requested) {
			this_thread::sleep_for(chrono::milliseconds(10));
			continue;
		}

		if (m_receiver->IsUpdated()) {
			try {
				FindSender();
			}
			catch (exception& e) {
				m_state = GameCaptureState::Closed;
				return;
			}
			auto totalRect = GetGameTotalRect();
			if (totalRect.width != m_totalWidth || totalRect.height != m_totalHeight) {
				UpdateTotalRect();
			}
		}
		if (m_receiver->ReceiveImage(m_totalCapture, m_totalWidth, m_totalHeight)) {
			//auto format = m_receiver->GetSenderFormat();
			if (m_totalHeight == 0 || m_totalWidth == 0) {
				continue;
			}

			for (int y = 0; y < m_captureRect.height; y++) {
				memcpy(
					m_capturedImage + y * m_captureRect.width * 4,
					m_totalCapture + ((y + m_captureRect.y) * m_totalWidth + m_captureRect.x) * 4,
					m_captureRect.width * 4
				);
			}
			m_state = GameCaptureState::Finished;
		}
		else {
			m_state = GameCaptureState::Idle;
		}
	}
}

void SpoutCapture::FindSender()
{
	int count = m_receiver->GetSenderCount();
	if (count == 0) {
		throw runtime_error("No sender found.");
	}
	for (int i = 0; i < count; i++) {
		char name[256];
		m_receiver->GetSender(i, name);
		if (regex_search(name, m_senderPattern)) {
			m_receiver->SetReceiverName(name);
			return;
		}
	}
	throw runtime_error("No sender matched.");
}

void SpoutCapture::UpdateTotalRect()
{
	auto totalRect = GetGameTotalRect();
	m_totalWidth = totalRect.width;
	m_totalHeight = totalRect.height;
	if (m_totalCapture != nullptr) {
		delete[] m_totalCapture;
	}
	m_totalCapture = new uint8_t[totalRect.width * totalRect.height * 4];

	m_captureRect = m_calculateCaptureRect(totalRect);
	if (m_capturedImage != nullptr) {
		delete[] m_capturedImage;
	}
	m_capturedImage = new uint8_t[m_captureRect.width * m_captureRect.height * 4];
}
