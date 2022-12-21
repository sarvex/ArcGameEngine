#pragma once

#include "Arc/Core/Window.h"

#include <GLFW/glfw3.h>

namespace ArcEngine
{
	class GraphicsContext;

	class WindowsWindow : public Window
	{
	public:
		explicit WindowsWindow(const WindowProps& props);
		~WindowsWindow() override;
		
		void OnUpdate() override;

		unsigned int GetWidth() const override { return m_Data.Width; }
		unsigned int GetHeight() const override { return m_Data.Height; }

		// Window attributes
		void SetEventCallBack(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
		void SetVSync(bool enabled) override;
		bool IsVSync() const override;

		bool IsMaximized() override { return glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED); }
		void Minimize() override;
		void Maximize() override;
		void Restore() override;
		void RegisterOverTitlebar(bool value) override;

		WindowHandle GetNativeWindow() const override { return m_Window; }
	private:
		void Init(const WindowProps& props);
		void Shutdown();

	private:
		GLFWwindow* m_Window;
		Scope<GraphicsContext> m_Context;

		struct WindowData
		{
			std::string Title;
			unsigned int Width;
			unsigned int Height;
			bool VSync;

			bool OverTitlebar = false;

			EventCallbackFn EventCallback;
		};

		WindowData m_Data;

		static uint8_t s_GLFWWindowCount;
	};
}

