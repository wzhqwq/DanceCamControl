#include "framework.h"
#include "App.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <cmath>
#include "WindowCapture.h"
#include "SpoutCapture.h"

using namespace std;
using namespace winrt;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::Graphics::Capture;

const double pi = 3.14159265358979323846;

App::App() : m_d3dDevice(CreateD3DDevice()) {
    m_canvas = make_unique<Canvas>(m_d3dDevice);
    //m_capture = make_unique<WindowCapture>("VRChat", "UnityWndClass", m_d3dDevice);
	m_capture = make_unique<SpoutCapture>(regex("VRCSender"));
    m_paraMgr = make_unique<ParameterManager>();
}

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
		case StereoAlign:
			DoStereoAlign();
			this_thread::sleep_for(chrono::milliseconds(1000));
			break;
        default:
            this_thread::sleep_for(chrono::milliseconds(1000));
            break;
        }
    }
}

void App::TryCapture()
{
    m_paraMgr->SetParameter("Tip", 1);
    try {
        m_capture->Initialize([](WindowRect rect) -> WindowRect
        {
            int captureWidth = (int)(rect.width * 0.2);
            int captureHeight = (int)(captureWidth / 0.8);
            return {
                0,
                rect.height - captureHeight,
                captureWidth,
                captureHeight
            };
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
    auto image = m_capture->RequestCapture();
	if (!CheckSign(image)) {
		return;
	}

    cv::Mat imageLeft;
    cv::extractChannel(image, imageLeft, 2);
    auto contours = GetPlaylistContours(imageLeft);
    if (contours.size() > 0) {
        m_stage = FixAngle;
        m_lastMask = cv::Mat();
    }
    else {
        m_stage = WaitForPlaylist;
    }
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

    cv::Mat imageLeft;
    cv::extractChannel(image, imageLeft, 2);
    auto contours = GetPlaylistContours(imageLeft);
    if (contours.size() > 0) {
		m_stage = FixAngle;
		m_lastMask = cv::Mat();
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

    if (!m_maskChanged) return;

    if (contours.size() == 0) {
        m_stage = WaitForPlaylist;
        return;
    }

    auto center = GetPlaylistCenter(contours[0]);

    cv::Mat canvas = cv::Mat::ones(image.rows, image.cols, CV_8UC4) * 100;
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
    m_nextStep = 3.0f;
    m_lastMask = cv::Mat();
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

    if (!m_maskChanged) {
        cv::Mat canvas = cv::Mat::ones(image.rows, image.cols, CV_8UC4) * 100;
        cv::drawContours(canvas, contours, -1, cv::Scalar(255, 0, 0, 255), cv::FILLED);

        m_canvas->SetCanvasImage(canvas);
        return;
    }

    if (contours.size() == 0) {
        if (!m_blankBackRetried) {
		    m_paraMgr->MoveDrone(2, -m_nextStep);
		    m_blankBackRetried = true;
            m_nextStep /= 2;
        }
        return;
    }
    m_blankBackRetried = false;

    cv::Mat canvas = cv::Mat::ones(image.rows, image.cols, CV_8UC4) * 100;
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
		m_paraMgr->MoveDrone(2, -m_nextStep);
		m_nextStep /= 2;
		return;
	}
	if (m_nextStep < 0.1) {
		m_stage = StereoAlign;
        m_lastMask = cv::Mat();
        return;
	}
    if (min > 0.1) {
		// far away from the playlist, so move forwards
		m_paraMgr->MoveDrone(2, m_nextStep);
        if (m_nextStep < 2.0f) m_nextStep /= 2;
        return;
    }

    auto center = GetPlaylistCenter(contours[0]);
	if (max(abs((float)center.x / image.cols * 2 - 1), abs((float)center.y / image.rows * 2 - 1)) > 0.2) {
		m_stage = FixAngle;
		return;
	}

    // camera vertical fov 60 degree, texture size 800 * 1000
    m_stage = StereoAlign;
    m_lastMask = cv::Mat();
}

void App::DoStereoAlign()
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

	cv::Mat imageLeft, imageRight;
	cv::extractChannel(image, imageLeft, 2);
	cv::extractChannel(image, imageRight, 1);

	auto leftContours = GetPlaylistContours(imageLeft);
	auto rightContours = GetPlaylistContours(imageRight);

    if (leftContours.size() == 0 || rightContours.size() == 0) {
        m_stage = WaitForPlaylist;
        return;
    }

    cv::Mat canvas = cv::Mat::ones(image.rows, image.cols, CV_8UC4) * 100;
    cv::drawContours(canvas, leftContours, 0, cv::Scalar(0, 0, 255, 255));
	cv::drawContours(canvas, rightContours, 0, cv::Scalar(0, 255, 0, 255));

	auto leftCorners = GetPlaylistCorners(leftContours[0]);
	auto rightCorners = GetPlaylistCorners(rightContours[0]);

    for (int i = 0; i < leftCorners.size(); i++) {
		cv::circle(canvas, leftCorners[i], 2 * (i + 1), cv::Scalar(0, 0, 255, 255), cv::FILLED);
    }
    for (int i = 0; i < rightCorners.size(); i++) {
        cv::circle(canvas, rightCorners[i], 2 * (i + 1), cv::Scalar(0, 255, 0, 255), cv::FILLED);
    }

    if (leftCorners.size() == 4 && rightCorners.size() == 4) {
        vector<float> disparities;
        for (int i = 0; i < 4; i++) {
            disparities.push_back(cv::norm(leftCorners[i] - rightCorners[i]));
        }
        vector<float> zs;
		float f = (float)image.rows / 2 / tan(pi / 6);
        for (int i = 0; i < 4; i++) {
            // z = b * f / d
			zs.push_back(0.2 * f / disparities[i]);
        }
		vector<float> xs, ys;
        for (int i = 0; i < 4; i++) {
            xs.push_back(zs[i] * (float)(leftCorners[i].x - image.cols / 2) / f);
            ys.push_back(zs[i] * (float)(leftCorners[i].y - image.rows / 2) / f);
        }

        for (int i = 0; i < 4; i++) {
            // reprojection
            auto x = (int)((xs[i] - 0.1f) * f / zs[i] + image.cols / 2);
            auto y = (int)((ys[i] - 0.1f) * f / zs[i] + image.rows / 2);
            // based on z
            auto r = (int)(10 - zs[i] * 2);
			cv::circle(canvas, cv::Point(x, y), r, cv::Scalar(255, 0, 0, 255), cv::FILLED);
        }

		// calculate the normal vector
		cv::Point3f v1(xs[1] - xs[0], ys[1] - ys[0], zs[1] - zs[0]);
		cv::Point3f v2(xs[3] - xs[0], ys[3] - ys[0], zs[3] - zs[0]);
		auto normal = v1.cross(v2);
		normal = normal / cv::norm(normal);
		auto up = -v2 / cv::norm(v2);
		auto right = normal.cross(up);
		right = right / cv::norm(right);

		// calculate the rotation matrix
		cv::Matx33f rotation(right.x, up.x, normal.x, right.y, up.y, normal.y, right.z, up.z, normal.z);
		cv::Vec3f euler;
		cv::Rodrigues(rotation, euler);
		OutputDebugString(std::format(L"euler: {}, {}, {}\n", euler[0], euler[1], euler[2]).c_str());

		cv::Point3f center((xs[0] + xs[1] + xs[2] + xs[3]) / 4, (ys[0] + ys[1] + ys[2] + ys[3]) / 4, (zs[0] + zs[1] + zs[2] + zs[3]) / 4);
		OutputDebugString(std::format(L"center: {}, {}, {}\n", center.x, center.y, center.z).c_str());
		auto distance = cv::norm(center);

		//m_paraMgr->MoveDrone(2, distance);
		//m_paraMgr->MoveDrone(3, -euler[0]);
  //      m_paraMgr->MoveDrone(2, -distance);
    }

    m_canvas->SetCanvasImage(canvas);
}

bool App::CheckSign(const cv::Mat image)
{
    // stride on B channel
    cv::Mat b;
    cv::extractChannel(image, b, 0);
	if (m_stage == WaitForSign) m_canvas->SetCanvasImage(b);

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
    cv::Mat mask;
    // 255 to 0, < 255 to 1
    if (image.channels() > 1) {
        cv::Mat r, g;
        cv::extractChannel(image, r, 2);
		cv::extractChannel(image, g, 1);
		mask = r < 240 | g < 240;
    }
    else {
		mask = image < 240;
    }

    // morph open
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

    // morph close
    kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(20, 20));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

	if (m_lastMask.empty()) {
		m_maskChanged = true;
	}
	else {
        cv::Mat diff;
		cv::absdiff(mask, m_lastMask, diff);
        m_maskChanged = cv::countNonZero(diff) > 10;
	}
	m_lastMask = mask;

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

std::vector<cv::Point> App::GetPlaylistCorners(const std::vector<cv::Point>& contour)
{
	std::vector<cv::Point2f> contourf;
	for (auto& p : contour) {
		contourf.push_back(static_cast<cv::Point2f>(p));
	}

    std::vector<cv::Point2f> interpolated;
	cv::Point2f last = contourf.back();
	for (int i = 0; i < contourf.size(); i++) {
        int splits = (int)(cv::norm(contourf[i] - last) - 1);
        auto splitVec = (contourf[i] - last) / (splits + 1);
		interpolated.push_back(last);
		for (int j = 0; j < splits; j++) {
			interpolated.push_back(last + splitVec * (j + 1));
		}
		last = contourf[i];
	}

    // find lines
	std::vector<cv::Vec4f> lines;
    std::vector<std::vector<cv::Point2f>> coveredPoints;
    int i;
    for (i = 0; i < interpolated.size(); i++) {
        if (lines.size() > 0) {
			auto& current = lines.back();
			auto& points = coveredPoints.back();

			points.push_back(interpolated[i]);
			// if the point is on the line, check the block dist
			auto t = (interpolated[i].x - current[2]) / (current[0] + 1e-6);
			auto y = current[3] + t * current[1];

            cv::fitLine(points, current, cv::DIST_L2, 0, 0.01, 0.01);
            if (abs(y - interpolated[i].y) < 10) {
                continue;
            }
        }
		cv::Vec4f line;
		std::vector<cv::Point2f> points;
		for (int j = 0; j < 5; j++) {
			if (i >= interpolated.size()) {
				break;
			}
			points.push_back(interpolated[i++]);
		}
		cv::fitLine(points, line, cv::DIST_L2, 0, 0.01, 0.01);
		lines.push_back(line);
		coveredPoints.push_back(points);
    }
    while (lines.size() > 4) {
        bool merged = false;
        for (int i = 0; i < lines.size(); i++) {
            auto& a = lines[i];
            auto& b = lines[(i + 1) % lines.size()];
			auto aVec = cv::Point2f(a[0], a[1]);
			auto bVec = cv::Point2f(b[0], b[1]);
            if (abs(aVec.dot(bVec)) > 0.9) {
				std::vector<cv::Point2f> mergedPoints = coveredPoints[i];
				auto& bPoints = coveredPoints[(i + 1) % lines.size()];
				mergedPoints.insert(mergedPoints.end(), bPoints.begin(), bPoints.end());

				cv::fitLine(mergedPoints, b, cv::DIST_L2, 0, 0.01, 0.01);
				bPoints = mergedPoints;

				lines.erase(lines.begin() + i);
				coveredPoints.erase(coveredPoints.begin() + i);

                merged = true;
				break;
            }
        }
        if (!merged) break;
    }
    while (lines.size() > 4) {
        int minIndex = 0;
        for (int i = 0; i < coveredPoints.size(); i++) {
            if (coveredPoints[i].size() > coveredPoints[minIndex].size()) {
                minIndex = i;
            }
        }
		lines.erase(lines.begin() + minIndex);
		coveredPoints.erase(coveredPoints.begin() + minIndex);
    }
    if (lines.size() != 4) {
		return {};
	}

	// find intersections
	std::vector<cv::Point> corners;
	for (int i = 0; i < 4; i++) {
		auto a = lines[i];
		auto b = lines[(i + 1) % 4];
		auto t = ((a[3] - b[3]) * b[0] - (a[2] - b[2]) * b[1]) / (b[1] * a[0] - b[0] * a[1]);
		auto x = a[2] + t * a[0];
		auto y = a[3] + t * a[1];
		corners.push_back(cv::Point((int)x, (int)y));
	}

	auto center = GetPlaylistCenter(contour);
	std::sort(corners.begin(), corners.end(), [center](cv::Point a, cv::Point b) {
		return atan2(a.y - center.y, a.x - center.x) < atan2(b.y - center.y, b.x - center.x);
	});

    return corners;
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