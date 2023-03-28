#pragma once

#include "Arc/Renderer/RendererAPI.h"

namespace ArcEngine
{
	class Dx12Context;

	class Dx12RendererAPI : public RendererAPI
	{
	public:
		void Init() override;
		void BeginFrame() override;
		void EndFrame() override;
		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		void SetClearColor(const glm::vec4& color) override;
		void Clear() override;
		void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) override;
		void Draw(const Ref<VertexBuffer>& vertexArray, uint32_t vertexCount) override;
		void DrawLines(const Ref<VertexBuffer>& vertexBuffer, uint32_t vertexCount) override;

		void ComputeDispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override;

		void Execute() override;

	private:
		Dx12Context* m_Context = nullptr;
	};
}