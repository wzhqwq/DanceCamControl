#pragma once

class MyCapture
{
public:
    MyCapture(
        std::string const& title,
        std::string const& className,
        winrt::com_ptr<ID3D11Device> d3ddevice);
    ~MyCapture() {
		StopCapture();
    }

    void FindWindow();
    WindowRect GetWindowClientRect();
    void InitializeCapture(const WindowRect& captureRect);

	void StartCapture();
	void StopCapture();
	bool IsClosed() const { return m_closed; }

    cv::Mat RequestCapture();

    winrt::Windows::Graphics::SizeInt32 GetCaptureSize() const;
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
	WindowRect m_captureRect;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
	winrt::com_ptr<ID3D11Device> m_d3dDevice{ nullptr };
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext{ nullptr };

    std::atomic<bool> m_closed = true;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker m_frameArrived;

    uint8_t* capturedImage;
    std::atomic<bool> requested = false;
};

