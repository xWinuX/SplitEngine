#pragma once

#include "SplitEngine/ApplicationInfo.hpp"
#include "SplitEngine/RenderingSettings.hpp"
#include "SplitEngine/ShaderParserSettings.hpp"
#include "SplitEngine/Window.hpp"
#include "SplitEngine/Rendering/Vulkan/Device.hpp"
#include "SplitEngine/Rendering/Vulkan/PhysicalDevice.hpp"

#include <vulkan/vulkan.hpp>


namespace SplitEngine::Rendering::Vulkan
{
	class Instance
	{
		public:
			Instance(Window& window, ApplicationInfo& applicationInfo, ShaderParserSettings&& shaderParserSettings, RenderingSettings&& renderingSettings);

			void Destroy();

			void CreateAllocator();

			[[nodiscard]] static Instance& Get();

			[[nodiscard]] const Image&                GetDefaultImage() const;
			[[nodiscard]] const vk::Sampler*          GetDefaultSampler() const;
			[[nodiscard]] const RenderingSettings&    GetRenderingSettings() const;
			[[nodiscard]] const ShaderParserSettings& GetShaderParserSettings() const;
			[[nodiscard]] PhysicalDevice&             GetPhysicalDevice() const;
			[[nodiscard]] Allocator&                  GetAllocator() const;
			[[nodiscard]] const vk::Instance&         GetVkInstance() const;
			[[nodiscard]] const vk::SurfaceKHR&       GetVkSurface() const;
			[[nodiscard]] vk::Viewport                CreateViewport(const vk::Extent2D extent) const;

		private:
			static Instance* _instance;

			ShaderParserSettings _shaderParserSettings;
			RenderingSettings    _renderingSettings;

			std::unique_ptr<PhysicalDevice> _physicalDevice;
			std::unique_ptr<Allocator>      _allocator;

			Image              _defaultImage;
			const vk::Sampler* _defaultSampler = nullptr;

			vk::Instance   _vkInstance{};
			vk::SurfaceKHR _vkSurface{};
	};
}
