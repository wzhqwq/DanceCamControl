#include "framework.h"
#include "MyCapture.h"
#include "d3dHelpers.h"
#include "capture.interop.h"
#include <format>

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

MyCapture::MyCapture(string const& title, string const& className) {
	m_title = title;
	m_className = className;
	m_captureRect = { 0, 0, 0, 0 };

	// m_d3dDevice
	m_d3dDevice = CreateD3DDevice();
	// m_device
	auto dxgiDevice = m_d3dDevice.as<IDXGIDevice>();
	com_ptr<::IInspectable> d3d_device;
	check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), d3d_device.put()));
	m_device = d3d_device.as<IDirect3DDevice>();
	// m_d3dContext
	m_d3dDevice->GetImmediateContext(m_d3dContext.put());
}

void MyCapture::FindWindow() {
	m_hWnd = FindWindowA((LPCSTR)m_className.c_str(), (LPCSTR)m_title.c_str());
	if (m_hWnd == nullptr) {
		throw runtime_error(format("Window not found: title={}, className={}", m_title, m_className));
	}
}

WindowRect MyCapture::GetWindowClientRect() {
	if (m_hWnd == nullptr) {
		FindWindow();
	}
	RECT rect;
	GetClientRect(m_hWnd, &rect);
	return { rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top };
}

void MyCapture::InitializeCapture(const WindowRect& captureRect) {
	auto wndRect = GetWindowClientRect();

	if (!m_closed) {
		StopCapture();
	}

	m_captureRect = captureRect;

	auto size = SizeInt32{ wndRect.width, wndRect.height };
	m_lastSize = size;

	auto item = CreateCaptureItemForWindow(m_hWnd);
	item.Closed([this](auto&&, auto&&) {
		m_closed = true;
		StopCapture();
	});
	m_item = item;

	// m_swapChain
	m_swapChain = CreateDXGISwapChain(
		m_d3dDevice,
		static_cast<uint32_t>(m_captureRect.width),
		static_cast<uint32_t>(m_captureRect.height),
		static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
		2);

	// m_framePool
	auto framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(m_device, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
	m_frameArrived = framePool.FrameArrived(auto_revoke, { this, &MyCapture::OnFrameArrived });
	m_framePool = framePool;

	// m_session
	auto session = framePool.CreateCaptureSession(item);
	m_session = session;
}

void MyCapture::StartCapture() {
	m_session.StartCapture();
}

void MyCapture::StopCapture() {
	m_closed = true;
	m_frameArrived.revoke();
	m_session.Close();
	m_framePool.Close();
	m_item = nullptr;
	m_lastSize = { 0, 0 };
	m_device = nullptr;
	m_d3dDevice = nullptr;
	m_swapChain = nullptr;
	m_d3dContext = nullptr;
	m_textureIn = nullptr;
}

ICompositionSurface MyCapture::CreateSurface(Compositor const& compositor) {
	ICompositionSurface surface{ nullptr };
	auto compositorInterop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
	com_ptr<ABI::Windows::UI::Composition::ICompositionSurface> surfaceInterop;
	check_hresult(compositorInterop->CreateCompositionSurfaceForSwapChain(m_swapChain.get(), surfaceInterop.put()));
	check_hresult(surfaceInterop->QueryInterface(guid_of<winrt::Windows::UI::Composition::ICompositionSurface>(),
		reinterpret_cast<void**>(put_abi(surface))));
	return surface;
}

void MyCapture::PrepareTexture(com_ptr<IDXGISurface> surface) {
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

	auto textureOutDesc = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		m_captureRect.width, m_captureRect.height,
		1,
		1,
		0,
		D3D11_USAGE_STAGING,
		D3D11_CPU_ACCESS_WRITE,
		1,
		0,
		0
	);

	com_ptr<ID3D11Texture2D> textureOut;
	check_hresult(m_d3dDevice->CreateTexture2D(&textureOutDesc, nullptr, textureOut.put()));

	m_textureOut = textureOut;
}

void MyCapture::OnFrameArrived(Direct3D11CaptureFramePool const& sender, Foundation::IInspectable const& args) {
	if (m_closed) {
		return;
	}
	auto frame = sender.TryGetNextFrame();
	if (frame == nullptr || m_lastFrameTime + chrono::milliseconds(3000) > chrono::system_clock::now()) {
		return;
	}
	m_lastFrameTime = chrono::system_clock::now();

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

	auto captureData = new uint8_t[m_captureRect.width * m_captureRect.height * 4];
	for (int y = 0; y < m_captureRect.height; y++) {
		memcpy(
			captureData + y * m_captureRect.width * 4,
			(uint8_t *)(mapInput.pData) + (y + m_captureRect.y) * mapInput.RowPitch + m_captureRect.x * 4,
			m_captureRect.width * 4
		);
	}
	m_d3dContext->Unmap(m_textureIn.get(), 0);

	com_ptr<ID3D11Texture2D> backBuffer;
	check_hresult(m_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), backBuffer.put_void()));

	D3D11_MAPPED_SUBRESOURCE mapOutput;
	check_hresult(m_d3dContext->Map(m_textureOut.get(), 0, D3D11_MAP_WRITE, 0, &mapOutput));

	for (int y = 0; y < m_captureRect.height; y++) {
		memcpy(
			(uint8_t*)(mapOutput.pData) + y * mapOutput.RowPitch,
			captureData + y * m_captureRect.width * 4,
			m_captureRect.width * 4
		);
	}
	m_d3dContext->Unmap(m_textureOut.get(), 0);

	m_d3dContext->CopyResource(backBuffer.get(), m_textureOut.get());

	DXGI_PRESENT_PARAMETERS presentParameters = { 0 };
	m_swapChain->Present1(1, 0, &presentParameters);

	delete[] captureData;
}
