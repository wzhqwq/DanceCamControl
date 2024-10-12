#include "framework.h"
#include "App.h"

using namespace winrt;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::Graphics::Capture;

void App::Initialize(ContainerVisual const& root) {
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
}

void App::Start() {
    try {
        if (m_capture == nullptr) {
		    //m_capture = std::make_unique<MyCapture>("VRChat", "UnityWndClass");
		    m_capture = std::make_unique<MyCapture>("cv", "CabinetWClass");
        }
        auto rect = m_capture->GetWindowClientRect();
		int captureWidth = (int)(rect.width * 0.2);
		int captureHeight = (int)(captureWidth / 0.8);
        m_capture->InitializeCapture({
            0,
			rect.height - captureHeight,
			captureWidth,
			captureHeight
		});

        auto surface = m_capture->CreateSurface(m_compositor);
        m_brush.Surface(surface);

        m_capture->StartCapture();
	}
	catch (std::exception const& e) {
        OutputDebugStringA(e.what());
    }
}