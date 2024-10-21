#include "framework.h"
#include "WindowCapture.h"
#include "d3dHelpers.h"
#include "capture.interop.h"
#include <format>
#include "opencv2/imgproc.hpp"

using namespace std;
using namespace winrt;
using namespace winrt::Windows;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;

WindowCapture::WindowCapture(string const& title, string const& className, com_ptr<ID3D11Device> d3ddevice)
	: m_title(title), m_className(className), m_d3dDevice(d3ddevice) {

	m_captureRect = { 0, 0, 0, 0 };

	// m_device
	auto dxgiDevice = m_d3dDevice.as<IDXGIDevice>();
	com_ptr<::IInspectable> d3d_device;
	check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), d3d_device.put()));
	m_device = d3d_device.as<IDirect3DDevice>();
	// m_d3dContext
	m_d3dDevice->GetImmediateContext(m_d3dContext.put());
}

void WindowCapture::FindWindow() {
	m_hWnd = FindWindowA((LPCSTR)m_className.c_str(), (LPCSTR)m_title.c_str());
	if (m_hWnd == nullptr) {
		throw runtime_error(format("Window not found: title={}, className={}", m_title, m_className));
	}
}

WindowRect WindowCapture::GetGameTotalRect() {
	if (m_hWnd == nullptr) {
		FindWindow();
	}
	RECT rect;
	GetClientRect(m_hWnd, &rect);
	return { rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top };
}

void WindowCapture::Initialize(function<WindowRect(WindowRect)> calculateCaptureRect) {
	auto wndRect = GetGameTotalRect();

	m_captureRect = calculateCaptureRect(wndRect);
	m_calculateCaptureRect = calculateCaptureRect;

	if (m_capturedImage != nullptr) {
		delete[] m_capturedImage;
	}
	m_capturedImage = new uint8_t[m_captureRect.width * m_captureRect.height * 4];

	auto size = SizeInt32{ wndRect.width, wndRect.height };
	m_lastSize = size;

	// m_item
	auto item = CreateCaptureItemForWindow(m_hWnd);
	item.Closed([this](auto&&, auto&&) {
		StopCapture();
	});
	m_item = item;

	// m_framePool
	auto framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(m_device, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
	m_frameArrived = framePool.FrameArrived(auto_revoke, { this, &WindowCapture::OnFrameArrived });
	m_framePool = framePool;

	// m_session
	auto session = framePool.CreateCaptureSession(item);
	m_session = session;
}

void WindowCapture::StartCapture() {
	m_session.StartCapture();
	m_state = GameCaptureState::Idle;
}

void WindowCapture::StopCapture() {
	if (m_state != GameCaptureState::Closed) {
		m_session.Close();
		m_frameArrived.revoke();
		m_framePool.Close();
		m_state = GameCaptureState::Closed;
	}
	m_framePool = nullptr;
	m_session = nullptr;
	m_item = nullptr;
	if (m_capturedImage != nullptr) {
		delete[] m_capturedImage;
		m_capturedImage = nullptr;
	}
}

void WindowCapture::PrepareTexture(com_ptr<IDXGISurface> surface) {
	DXGI_SURFACE_DESC desc;
	check_hresult(surface->GetDesc(&desc));

	auto textureInDesc = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		desc.Width, desc.Height,
		1,
		1,
		0,
		D3D11_USAGE_STAGING,
		D3D11_CPU_ACCESS_READ,
		1,
		0,
		0
	);

	com_ptr<ID3D11Texture2D> textureIn;
	check_hresult(m_d3dDevice->CreateTexture2D(&textureInDesc, nullptr, textureIn.put()));

	m_textureIn = textureIn;
}

void WindowCapture::OnFrameArrived(Direct3D11CaptureFramePool const& sender, Foundation::IInspectable const& args) {
	if (m_state == GameCaptureState::Closed) {
		return;
	}
	auto frame = sender.TryGetNextFrame();
	if (frame == nullptr || m_state != GameCaptureState::Requested) {
		return;
	}
	m_state = GameCaptureState::Processing;

	auto surface = frame.Surface();
	auto const interop = surface.as<IDirect3DDxgiInterfaceAccess>();
	com_ptr<IDXGISurface> dxgiSurface;
	check_hresult(interop->GetInterface(__uuidof(IDXGISurface), dxgiSurface.put_void()));
	if (m_textureIn == nullptr) {
		PrepareTexture(dxgiSurface);
	}

	com_ptr<ID3D11Resource> input;
	check_hresult(interop->GetInterface(IID_PPV_ARGS(&input)));

	// copy frame into CPU-readable resource
	// this and the Map call can be done at each frame
	m_d3dContext->CopyResource(m_textureIn.get(), input.get());

	D3D11_MAPPED_SUBRESOURCE mapInput;
	check_hresult(m_d3dContext->Map(m_textureIn.get(), 0, D3D11_MAP_READ, 0, &mapInput));

	for (int y = 0; y < m_captureRect.height; y++) {
		memcpy(
			m_capturedImage + y * m_captureRect.width * 4,
			(uint8_t *)(mapInput.pData) + (y + m_captureRect.y) * mapInput.RowPitch + m_captureRect.x * 4,
			m_captureRect.width * 4
		);
	}
	m_d3dContext->Unmap(m_textureIn.get(), 0);

	m_state = GameCaptureState::Finished;
}
