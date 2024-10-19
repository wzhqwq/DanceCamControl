#include "framework.h"
#include "App.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <cmath>

using namespace std;
using namespace winrt;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::Graphics::Capture;

const double pi = 3.14159265358979323846;

void App::Initialize(ContainerVisual const& root) {
    m_canvas->Initialize(root);
}

void App::Start() {
    try {
            m_paraMgr->Start();

		m_thread = thread(&App::StateMachine, this);
	}
	catch (std::exception const& e) {
        OutputDebugStringA(e.what());
    }
}

void App::StateMachine() {
    while (true) {
        if (m_terminated) {
            break;
        }
		if (m_capture->IsClosed()) {
			m_stage = WaitForWindow;
		}
        switch (m_stage)
        {
        case WaitForWindow:
			TryCapture();
            break;
        case WaitForSign:
			TryMatchSign();
            this_thread::sleep_for(chrono::milliseconds(300));
            break;
        case WaitForPlaylist:
			TryFindPlaylist();
            this_thread::sleep_for(chrono::milliseconds(300));
            break;
        case FixAngle:
			DoFixAngle();
            this_thread::sleep_for(chrono::milliseconds(100));
            break;
        case FixDistance:
			DoFixDistance();
            this_thread::sleep_for(chrono::milliseconds(100));
            break;
        default:
            this_thread::sleep_for(chrono::milliseconds(1000));
            break;
        }
    }
}

void App::TryCapture()
{
    try {
        auto rect = m_capture->GetWindowClientRect();

		int captureWidth = (int)(rect.width * 0.2);
		int captureHeight = (int)(captureWidth / 0.8);
        m_capture->InitializeCapture({
            0,
			rect.height - captureHeight,
			captureWidth,
			captureHeight
		});

        m_capture->StartCapture();
		m_stage = WaitForSign;
	}
	catch (std::exception const& e) {
		OutputDebugStringA(e.what());
        this_thread::sleep_for(chrono::milliseconds(1000));
	}
}

void App::TryMatchSign()
{
    m_paraMgr->SetParameter("Tip", 1);
    m_paraMgr->SetParameter("PLCStage", 0);

    auto image = m_capture->RequestCapture();
	if (!CheckSign(image)) {
		return;
	}
	m_stage = WaitForPlaylist;
}

void App::TryFindPlaylist()
{
    m_paraMgr->SetParameter("Tip", 2);
    m_paraMgr->SetParameter("PLCStage", 0);

    auto image = m_capture->RequestCapture();
    if (!CheckSign(image)) {
		m_stage = WaitForSign;
        return;
    }

	auto contours = GetPlaylistContours(image);
	if (contours.size() > 0) {
		m_stage = FixAngle;
	}
}

void App::DoFixAngle()
{
    m_paraMgr->SetParameter("PLCStage", 1);
    m_paraMgr->SetParameter("Tip", 3);
    if (!m_paraMgr->IsAvailable()) {
		return;
	}
    auto image = m_capture->RequestCapture();

    if (!CheckSign(image)) {
        m_stage = WaitForSign;
        return;
    }

    auto contours = GetPlaylistContours(image);
    if (contours.size() == 0) {
        m_stage = WaitForPlaylist;
        return;
    }

    auto center = GetPlaylistCenter(contours[0]);

    cv::Mat canvas = cv::Mat::zeros(image.rows, image.cols, CV_8UC4);
	cv::drawContours(canvas, contours, -1, cv::Scalar(0, 0, 255, 255), cv::FILLED);
    cv::circle(canvas, center, 5, cv::Scalar(255, 0, 0, 255), cv::FILLED);

	m_canvas->SetCanvasImage(canvas);

	// camera vertical fov 60 degree, texture size 800 * 1000
    float angleVert = (float)(atan(((float)center.y / image.rows * 2 - 1) / tan(pi / 3)) * 180 / pi);
	float angleHorz = (float)(atan(((float)center.x / image.cols * 2 - 1) * 0.8 / tan(pi / 3)) * 180 / pi);
	OutputDebugString(std::format(L"angleVert: {}, angleHorz: {}\n", angleVert, angleHorz).c_str());

    if (abs(angleVert) > 1) {
        m_paraMgr->MoveDrone(3, angleVert);
		return;
    }
    if (abs(angleHorz) > 1) {
        m_paraMgr->MoveDrone(4, angleHorz);
        return;
    }
	m_stage = FixDistance;
}

void App::DoFixDistance()
{
    m_paraMgr->SetParameter("PLCStage", 1);
    m_paraMgr->SetParameter("Tip", 3);
    if (!m_paraMgr->IsAvailable()) {
        return;
    }
    auto image = m_capture->RequestCapture();

    if (!CheckSign(image)) {
        m_stage = WaitForSign;
        return;
    }

    auto contours = GetPlaylistContours(image);
    if (contours.size() == 0) {
        m_stage = WaitForPlaylist;
        return;
    }

    cv::Mat canvas = cv::Mat::zeros(image.rows, image.cols, CV_8UC4);
    cv::drawContours(canvas, contours, -1, cv::Scalar(0, 0, 255, 255), cv::FILLED);

    m_canvas->SetCanvasImage(canvas);

	auto bbox = cv::boundingRect(contours[0]);
    std::vector<float> margins = {
		(float)bbox.x / image.cols,
		(float)bbox.y / image.rows,
		(float)(image.cols - bbox.x - bbox.width) / image.cols,
		(float)(image.rows - bbox.y - bbox.height) / image.rows
	};
	auto min = *std::min_element(margins.begin(), margins.end());

	if (min < 1e-6) {
		// not a complete view of the playlist, so move backwards
		m_paraMgr->MoveDrone(2, -1.3f);
		return;
	}
    if (min > 0.2) {
        // too far away from the playlist, so move forwards
        m_paraMgr->MoveDrone(2, 0.7f);
        return;
    }
    if (min > 0.1) {
		// far away from the playlist, so move forwards slowly
		m_paraMgr->MoveDrone(2, 0.3f);
		return;
    }

    auto center = GetPlaylistCenter(contours[0]);
	if (max(abs((float)center.x / image.cols * 2 - 1), abs((float)center.y / image.rows * 2 - 1)) > 0.2) {
		m_stage = FixAngle;
		return;
	}

    // camera vertical fov 60 degree, texture size 800 * 1000
    m_stage = StereoAlign;
}

bool App::CheckSign(const cv::Mat image)
{
    // stride on B channel
    cv::Mat b;
    cv::extractChannel(image, b, 0);
    m_canvas->SetCanvasImage(b);

    vector<int> strideWidths;
    int start = 0;
    for (int i = 1; i < image.rows; i++) {
        if (b.at<uint8_t>(i, 0) < 50 && b.at<uint8_t>(i - 1, 0) > 200) {
            start = i;
        }
        if (b.at<uint8_t>(i, 0) > 200 && b.at<uint8_t>(i - 1, 0) < 50) {
            strideWidths.push_back(i - start);
        }
    }
    if (strideWidths.size() < 2) {
        return false;
    }
    for (int i = 2; i < strideWidths.size() - 1; i++) {
        if (abs(strideWidths[i] - strideWidths[1]) > 2) return false;
    }
    return true;
}

std::vector<std::vector<cv::Point>> App::GetPlaylistContours(const cv::Mat image)
{
    // copy R channel to gray
    cv::Mat r;
    cv::extractChannel(image, r, 2);
    // 255 to 0, < 255 to 1
    cv::Mat mask = r < 240;

    // morph open
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

    // morph close
    kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(20, 20));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    // find contours
    vector<vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	return contours;
}

cv::Point App::GetPlaylistCenter(const std::vector<cv::Point>& contour)
{
    cv::Moments m = cv::moments(contour);
    return cv::Point((int)(m.m10 / m.m00), (int)(m.m01 / m.m00));
}

void App::Terminate() {
    m_terminated = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_capture != nullptr) {
        m_capture->StopCapture();
    }
}