#pragma once
#include "DescriptorSetAllocator.hpp"
#include "Device.hpp"
#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include "SplitEngine/Window.hpp"

#include <vector>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.hpp>

namespace SplitEngine::Rendering::Vulkan
{
	class Context
	{
		public:
			Context() = default;

			void        Initialize(Window* window);
			static void WaitForIdle();
			void        Destroy();

			static Device* GetDevice();

			[[nodiscard]] const vk::Semaphore&     GetImageAvailableSemaphore() const;
			[[nodiscard]] const vk::Semaphore&     GetRenderFinishedSemaphore() const;
			[[nodiscard]] const vk::Fence&         GetInFlightFence() const;
			[[nodiscard]] const Instance&          GetInstance() const;
			[[nodiscard]] const vk::CommandBuffer& GetCommandBuffer() const;

		private:
			const std::vector<const char*> _validationLayers = { "VK_LAYER_KHRONOS_validation" };
			const std::vector<const char*> _deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME };

			Window* _window = nullptr; // TODO: Move to renderer

			static std::unique_ptr<Device> _device;

			Instance       _instance{};
			PhysicalDevice _physicalDevice{};

			InFlightResource<vk::CommandBuffer> _commandBuffer{};
			InFlightResource<vk::Semaphore>     _imageAvailableSemaphore{};
			InFlightResource<vk::Semaphore>     _renderFinishedSemaphore{};
			InFlightResource<vk::Fence>         _inFlightFence{};

			vk::DescriptorPool _imGuiDescriptorPool{};

			void CreateInstance(SDL_Window* sdlWindow);
			void CreateCommandBuffers();
			void CreateSyncObjects();
			void InitializeImGui();
	};
}
