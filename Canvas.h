#pragma once
#include <opencv2/core.hpp>

class Canvas
{
public:
    Canvas(winrt::com_ptr<ID3D11Device> d3dDevice);

    void SetCanvasImage(cv::Mat const& image);
	void Initialize(winrt::Windows::UI::Composition::ContainerVisual const& root);

private:
    winrt::Windows::UI::Composition::ICompositionSurface CreateSurface(
        winrt::Windows::UI::Composition::Compositor const& compositor);

    void PrepareTexture();
	void UpdateSize();

private:
    winrt::Windows::UI::Composition::Compositor m_compositor{ nullptr };
    winrt::Windows::UI::Composition::ContainerVisual m_root{ nullptr };
    winrt::Windows::UI::Composition::SpriteVisual m_visual{ nullptr };
    winrt::Windows::UI::Composition::CompositionSurfaceBrush m_brush{ nullptr };

    winrt::com_ptr<ID3D11Texture2D> m_textureOut{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::com_ptr<ID3D11Device> m_d3dDevice{ nullptr };
    winrt::com_ptr<IDXGISwapChain1> m_swapChain{ nullptr };
	winrt::com_ptr<ID3D11DeviceContext> m_d3dContext{ nullptr };

	cv::Size m_displaySize;
};

