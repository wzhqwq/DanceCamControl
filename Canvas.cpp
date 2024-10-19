#include "framework.h"
#include "Canvas.h"
#include <opencv2/imgproc.hpp>
#include "d3dHelpers.h"

using namespace std;
using namespace winrt;
using namespace Windows::UI::Composition;
using namespace winrt::Windows::Graphics::DirectX;

Canvas::Canvas(com_ptr<ID3D11Device> d3dDevice)
	: m_d3dDevice(d3dDevice) {
	m_displaySize = { 10, 10 };

	m_d3dDevice->GetImmediateContext(m_d3dContext.put());
	m_swapChain = CreateDXGISwapChain(
		m_d3dDevice,
		static_cast<uint32_t>(m_displaySize.width),
		static_cast<uint32_t>(m_displaySize.height),
		static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
		2
	);
}

void Canvas::Initialize(ContainerVisual const& root) {
    m_compositor = root.Compositor();
    m_root = m_compositor.CreateContainerVisual();
    m_visual = m_compositor.CreateSpriteVisual();
    m_brush = m_compositor.CreateSurfaceBrush();

    m_root.RelativeSizeAdjustment({ 1, 1 });
    root.Children().InsertAtTop(m_root);

    m_visual.AnchorPoint({ 0.5f, 0.5f });
    m_visual.RelativeOffsetAdjustment({ 0.5f, 0.5f, 0 });
    m_visual.RelativeSizeAdjustment({ 1, 1 });
    m_visual.Size({ -80, -80 });
    m_visual.Brush(m_brush);
    m_brush.HorizontalAlignmentRatio(0.5f);
    m_brush.VerticalAlignmentRatio(0.5f);
    m_brush.Stretch(CompositionStretch::Uniform);
    m_root.Children().InsertAtTop(m_visual);

	auto surface = CreateSurface(m_compositor);
	m_brush.Surface(surface);

	PrepareTexture();
}

ICompositionSurface Canvas::CreateSurface(Compositor const& compositor) {
    ICompositionSurface surface{ nullptr };
    auto compositorInterop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
    com_ptr<ABI::Windows::UI::Composition::ICompositionSurface> surfaceInterop;
    check_hresult(compositorInterop->CreateCompositionSurfaceForSwapChain(m_swapChain.get(), surfaceInterop.put()));
    check_hresult(surfaceInterop->QueryInterface(guid_of<winrt::Windows::UI::Composition::ICompositionSurface>(),
        reinterpret_cast<void**>(put_abi(surface))));
    return surface;
}

void Canvas::PrepareTexture()
{
	auto textureOutDesc = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		m_displaySize.width, m_displaySize.height,
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

void Canvas::UpdateSize()
{
	m_swapChain->ResizeBuffers(
		2,
		static_cast<uint32_t>(m_displaySize.width),
		static_cast<uint32_t>(m_displaySize.height),
		static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
		0
	);
	PrepareTexture();
}


void Canvas::SetCanvasImage(cv::Mat const& image)
{
	// check channel count
	if (image.channels() == 1) {
		// convert to 4 channels
		cv::Mat bgra;
		cv::cvtColor(image, bgra, cv::COLOR_GRAY2BGRA);
		SetCanvasImage(bgra);
		return;
	}
	else if (image.channels() != 4) {
		throw runtime_error("Invalid channel count");
	}

	if (image.size().width != m_displaySize.width || image.size().height != m_displaySize.height) {
		m_displaySize = image.size();
		UpdateSize();
	}

	com_ptr<ID3D11Texture2D> backBuffer;
	check_hresult(m_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), backBuffer.put_void()));

	D3D11_MAPPED_SUBRESOURCE mapOutput;
	check_hresult(m_d3dContext->Map(m_textureOut.get(), 0, D3D11_MAP_WRITE, 0, &mapOutput));

	for (int y = 0; y < m_displaySize.height; y++) {
		memcpy(
			(uint8_t*)(mapOutput.pData) + y * mapOutput.RowPitch,
			image.ptr<uint8_t>(y),
			static_cast<size_t>(m_displaySize.width) * 4
		);
	}
	m_d3dContext->Unmap(m_textureOut.get(), 0);

	m_d3dContext->CopyResource(backBuffer.get(), m_textureOut.get());

	DXGI_PRESENT_PARAMETERS presentParameters = { 0 };
	m_swapChain->Present1(1, 0, &presentParameters);
}




