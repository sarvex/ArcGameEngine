#pragma once

#include "Arc/Core/Layer.h"

#include "Arc/Events/ApplicationEvent.h"
#include "Arc/Events/KeyEvent.h"
#include "Arc/Events/MouseEvent.h"

namespace ArcEngine
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();
	private:
		float m_Time = 0.0f;
	};
}

