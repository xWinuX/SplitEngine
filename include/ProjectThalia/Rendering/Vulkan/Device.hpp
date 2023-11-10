#pragma once

#include "Allocator.hpp"
#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include "Pipeline.hpp"
#include "RenderPass.hpp"
#include "Swapchain.hpp"

#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace ProjectThalia::Rendering::Vulkan
{
	class Device
	{
		public:
			static const int MAX_FRAMES_IN_FLIGHT = 2;

			explicit Device(PhysicalDevice& physicalDevice);

			void CreateAllocator(const Instance& instance, const AllocatorCreateInfo& allocatorCreateInfo = {});
			void CreateSwapchain(vk::SurfaceKHR surfaceKhr, glm::ivec2 size);
			void CreateRenderPass();
			void CreatePipeline(const std::string&                             name,
								const std::vector<Pipeline::ShaderInfo>&       shaderInfos,
								const vk::ArrayProxy<vk::DescriptorSetLayout>& uniformBuffers = nullptr);
			void CreateGraphicsCommandPool();

			void Destroy();

			[[nodiscard]] const Swapchain&                          GetSwapchain() const;
			[[nodiscard]] const RenderPass&                         GetRenderPass() const;
			[[nodiscard]] const Pipeline&                           GetPipeline() const;
			[[nodiscard]] const vk::Device&                         GetVkDevice() const;
			[[nodiscard]] const PhysicalDevice&                     GetPhysicalDevice() const;
			[[nodiscard]] const vk::Queue&                          GetGraphicsQueue() const;
			[[nodiscard]] const vk::Queue&                          GetPresentQueue() const;
			[[nodiscard]] const vk::PhysicalDeviceMemoryProperties& GetMemoryProperties() const;
			[[nodiscard]] const vk::CommandPool&                    GetGraphicsCommandPool() const;
			[[nodiscard]] Allocator&                                GetAllocator();

			[[nodiscard]] int FindMemoryTypeIndex(const vk::MemoryRequirements&                memoryRequirements,
												  const vk::Flags<vk::MemoryPropertyFlagBits>& memoryPropertyFlags) const;


			[[nodiscard]] vk::CommandBuffer BeginOneshotCommands() const;
			void                            EndOneshotCommands(vk::CommandBuffer commandBuffer) const;

		private:
			vk::Device      _vkDevice;
			PhysicalDevice& _physicalDevice;
			Allocator       _allocator;

			Swapchain  _swapchain;
			RenderPass _renderPass;
			Pipeline   _pipeline;

			vk::CommandPool _graphicsCommandPool;

			vk::Queue _graphicsQueue;
			vk::Queue _presentQueue;

			vk::PhysicalDeviceMemoryProperties _memoryProperties;
	};
}
