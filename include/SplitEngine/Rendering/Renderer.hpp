#pragma once

#include <SplitEngine/RenderingSettings.hpp>

#include "SplitEngine/ApplicationInfo.hpp"
#include "SplitEngine/Window.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Vulkan/Instance.hpp"

namespace SplitEngine
{
	class Application;

	namespace Rendering
	{
		class Renderer
		{
			friend class SDLEventSystem;
			friend class RenderingSystem;

			public:
				explicit Renderer(ApplicationInfo& applicationInfo, ShaderParserSettings&& shaderParserSettings, RenderingSettings&& renderingSettings);
				~Renderer();

				[[nodiscard]] Vulkan::CommandBuffer& GetCommandBuffer();
				[[nodiscard]] Vulkan::Instance&      GetVulkanInstance();
				[[nodiscard]] Window&                GetWindow();
				[[nodiscard]] bool                   WasSkipped() const;

			private:
				Window           _window;
				Vulkan::Instance _vulkanInstance;

				bool     _wasSkipped             = false;
				uint32_t _latestImageIndexResult = 0;

				Vulkan::CommandBuffer _commandBuffer;

				bool _frameBufferResized = false;

				void BeginRender();
				void EndRender();

				void HandleEvents(SDL_Event event);
		};
	}
}
