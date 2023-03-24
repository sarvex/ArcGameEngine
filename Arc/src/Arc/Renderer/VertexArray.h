#pragma once

#include "Arc/Renderer/Buffer.h"

namespace ArcEngine
{
	class IndexBuffer;
	class VertexBuffer;

	class VertexArray
	{
	public:
		virtual ~VertexArray() = default;
		
		virtual void Bind() const = 0;

		virtual void AddVertexBuffer(Ref<VertexBuffer>& vertexBuffer) = 0;
		virtual void SetIndexBuffer(Ref<IndexBuffer>& indexBuffer) = 0;

		[[nodiscard]] virtual const std::vector<Ref<VertexBuffer>>& GetVertexBuffer() const = 0;
		[[nodiscard]] virtual const Ref<IndexBuffer>& GetIndexBuffer() const = 0;
		
		[[nodiscard]] static Ref<VertexArray> Create();
	};
}
