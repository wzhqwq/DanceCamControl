#pragma once
#include "MyCapture.h"

class App
{
public:
	void Initialize(winrt::Windows::UI::Composition::ContainerVisual const& root);

	void Start();
private:
	winrt::Windows::UI::Composition::Compositor m_compositor{ nullptr };
	winrt::Windows::UI::Composition::ContainerVisual m_root{ nullptr };
	winrt::Windows::UI::Composition::SpriteVisual m_visual{ nullptr };
	winrt::Windows::UI::Composition::CompositionSurfaceBrush m_brush{ nullptr };

	std::unique_ptr<MyCapture> m_capture{ nullptr };
};

