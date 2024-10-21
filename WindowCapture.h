#pragma once
#include "GameCapture.h"

class WindowCapture : public GameCapture
{
public:
    WindowCapture(
        std::string const& title,
        std::string const& className,
        winrt::com_ptr<ID3D11Device> d3ddevice);
    ~WindowCapture() {
		StopCapture();
    }

    void FindWindow();

    WindowRect GetGameTotalRect();
    void Initialize(std::function<WindowRect(WindowRect)> calculateCaptureRect);
	void StartCapture();
	void StopCapture();
private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);
	void PrepareTexture(winrt::com_ptr<IDXGISurface> surface);

private:
    HWND m_hWnd{ nullptr };
	std::string m_title;
	std::string m_className;

	winrt::com_ptr<ID3D11Texture2D> m_textureIn{ nullptr };

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
    winrt::Windows::Graphics::SizeInt32 m_lastSize;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
	winrt::com_ptr<ID3D11Device> m_d3dDevice{ nullptr };
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext{ nullptr };

    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker m_frameArrived;
};

