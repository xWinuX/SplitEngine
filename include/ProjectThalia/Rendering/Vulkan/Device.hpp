#pragma once

#include "PhysicalDevice.hpp"
#include "Pipeline.hpp"
#include "RenderPass.hpp"
#include "Swapchain.hpp"
#include "glm/glm.hpp"
#include "vulkan/vulkan.hpp"

namespace ProjectThalia::Rendering::Vulkan
{
	class Device
	{
		public:
			explicit Device(PhysicalDevice& physicalDevice);

			void CreateSwapchain(vk::SurfaceKHR surfaceKhr, glm::ivec2 size);
			void CreateRenderPass();
			void CreatePipeline(const std::string& name, std::vector<Pipeline::ShaderInfo> shaderInfos);

			void Destroy();

			[[nodiscard]] const Swapchain&                          GetSwapchain() const;
			[[nodiscard]] const RenderPass&                         GetRenderPass() const;
			[[nodiscard]] const Pipeline&                           GetPipeline() const;
			[[nodiscard]] const vk::Device&                         GetVkDevice() const;
			[[nodiscard]] const PhysicalDevice&                     GetPhysicalDevice() const;
			[[nodiscard]] const vk::Queue&                          GetGraphicsQueue() const;
			[[nodiscard]] const vk::Queue&                          GetPresentQueue() const;
			[[nodiscard]] const vk::PhysicalDeviceMemoryProperties& GetMemoryProperties() const;

		private:
			vk::Device      _vkDevice;
			PhysicalDevice& _physicalDevice;

			Swapchain  _swapchain;
			RenderPass _renderPass;
			Pipeline   _pipeline;

			vk::Queue _graphicsQueue;
			vk::Queue _presentQueue;

			vk::PhysicalDeviceMemoryProperties _memoryProperties;
	};
}