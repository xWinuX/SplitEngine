#include "ProjectThalia/Rendering/Vulkan/Context.hpp"

#include "ProjectThalia/Debug/Log.hpp"
#include "ProjectThalia/ErrorHandler.hpp"
#include "ProjectThalia/IO/ImageFile.hpp"
#include "ProjectThalia/IO/Stream.hpp"
#include "ProjectThalia/Rendering/Vertex.hpp"

#include <SDL2/SDL_vulkan.h>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace ProjectThalia::Rendering::Vulkan
{
	void Context::Initialize(Window* window)
	{
		_window = window; // TODO: Move to renderer

		CreateInstance(_window->GetSDLWindow());

		_physicalDevice = PhysicalDevice(_instance.GetVkInstance(), _instance.GetVkSurface(), _deviceExtensions, _validationLayers);

		_device = std::make_unique<Device>(Device(_physicalDevice));

		_device->CreateRenderPass();
		_device->CreateSwapchain(_instance.GetVkSurface(), _window->GetSize());

		// Create descriptor layout set
		vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding = vk::DescriptorSetLayoutBinding(0,
																								   vk::DescriptorType::eUniformBuffer,
																								   1,
																								   vk::ShaderStageFlagBits::eVertex,
																								   nullptr);

		vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo({}, 1, &descriptorSetLayoutBinding);
		_descriptorSetLayout                                            = _device->GetVkDevice().createDescriptorSetLayout(descriptorSetLayoutCreateInfo);

		std::vector<vk::DescriptorSetLayout> uniformBuffers = {_descriptorSetLayout};

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			_uniformBuffers[i]    = Buffer::CreateUniformBuffer<UniformBufferObject>(_device.get(), nullptr);
			_uniformBufferData[i] = _uniformBuffers[i].FullMap<UniformBufferObject>();
		}

		_device->CreatePipeline("main",
								{{"res/shaders/Debug.vert.spv", vk::ShaderStageFlagBits::eVertex},
								 {"res/shaders/Debug.frag.spv", vk::ShaderStageFlagBits::eFragment}},
								&uniformBuffers);
		_device->CreateGraphicsCommandPool();


		// Create descriptor pool
		vk::DescriptorPoolSize descriptorPoolSize = vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT));
		vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo({},
																							 static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
																							 descriptorPoolSize);

		_descriptorPool = _device->GetVkDevice().createDescriptorPool(descriptorPoolCreateInfo);


		// Create descriptor sets
		std::vector<vk::DescriptorSetLayout> descriptorSetLayouts      = std::vector<vk::DescriptorSetLayout>(MAX_FRAMES_IN_FLIGHT, _descriptorSetLayout);
		vk::DescriptorSetAllocateInfo        descriptorSetAllocateInfo = vk::DescriptorSetAllocateInfo(_descriptorPool, descriptorSetLayouts);

		_device->GetVkDevice().allocateDescriptorSets(&descriptorSetAllocateInfo, _descriptorSets.data());


		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorBufferInfo descriptorBufferInfo = vk::DescriptorBufferInfo(_uniformBuffers[i].GetVkBuffer(), 0, sizeof(UniformBufferObject));

			vk::WriteDescriptorSet writeDescriptorSet = vk::WriteDescriptorSet(_descriptorSets[i],
																			   0,
																			   0,
																			   vk::DescriptorType::eUniformBuffer,
																			   nullptr,
																			   descriptorBufferInfo,
																			   nullptr);

			_device->GetVkDevice().updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
		}

		CreateCommandBuffers();

		CreateSyncObjects();

		const std::vector<VertexPosition2DColor> vertices = {{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
															 {{0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
															 {{-0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
															 {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}};

		const std::vector<uint16_t> indices = {0, 1, 2, 2, 1, 3};

		_quadModelBuffer = Buffer::CreateStagedModelBuffer(_device.get(), vertices, indices);

		IO::ImageFile textureImage = IO::ImageFile("res/textures/floppa.png", IO::ImageFile::RGBA);

		_window->OnResize.Add([this](int width, int height) {
			_frameBufferResized = true;
		});
	}

	void Context::CreateSyncObjects()
	{
		vk::SemaphoreCreateInfo semaphoreCreateInfo = vk::SemaphoreCreateInfo();
		vk::FenceCreateInfo     fenceCreateInfo     = vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			_imageAvailableSemaphore[i] = _device->GetVkDevice().createSemaphore(semaphoreCreateInfo);
			_renderFinishedSemaphore[i] = _device->GetVkDevice().createSemaphore(semaphoreCreateInfo);
			_inFlightFence[i]           = _device->GetVkDevice().createFence(fenceCreateInfo);
		}
	}

	void Context::RecordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
	{
		vk::CommandBufferBeginInfo commandBufferBeginInfo = vk::CommandBufferBeginInfo({}, nullptr);

		commandBuffer.begin(commandBufferBeginInfo);

		vk::ClearValue          clearColor          = vk::ClearValue({0.0f, 0.2f, 0.5f, 1.0f});
		vk::RenderPassBeginInfo renderPassBeginInfo = vk::RenderPassBeginInfo(_device->GetRenderPass().GetVkRenderPass(),
																			  _device->GetSwapchain().GetFrameBuffers()[imageIndex],
																			  {{0, 0}, _device->GetSwapchain().GetExtend()},
																			  1,
																			  &clearColor);

		commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _device->GetPipeline().GetVkPipeline());

		vk::Buffer     vertexBuffers[] = {_quadModelBuffer.GetVkBuffer()};
		vk::DeviceSize offsets[]       = {0};

		commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
		commandBuffer.bindIndexBuffer(_quadModelBuffer.GetVkBuffer(), _quadModelBuffer.GetSizeInBytes(0), vk::IndexType::eUint16);

		vk::Viewport viewport = vk::Viewport(0,
											 static_cast<float>(_device->GetSwapchain().GetExtend().height),
											 static_cast<float>(_device->GetSwapchain().GetExtend().width),
											 -static_cast<float>(_device->GetSwapchain().GetExtend().height),
											 0.0f,
											 1.0f);
		commandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor = vk::Rect2D({0, 0}, _device->GetSwapchain().GetExtend());
		commandBuffer.setScissor(0, 1, &scissor);

		commandBuffer
				.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _device->GetPipeline().GetLayout(), 0, 1, &_descriptorSets[_currentFrame], 0, nullptr);
		commandBuffer.drawIndexed(_quadModelBuffer.GetBufferElementNum(1), 1, 0, 0, 0);

		commandBuffer.endRenderPass();
		commandBuffer.end();
	}

	void Context::CreateCommandBuffers()
	{
		vk::CommandBufferAllocateInfo commandBufferAllocateInfo = vk::CommandBufferAllocateInfo(_device->GetGraphicsCommandPool(),
																								vk::CommandBufferLevel::ePrimary,
																								_commandBuffer.size());

		_commandBuffer = _device->GetVkDevice().allocateCommandBuffers(commandBufferAllocateInfo);
	}

	void Context::Destroy()
	{
		_device->GetVkDevice().waitIdle();

		for (const vk::Semaphore& semaphore : _imageAvailableSemaphore) { _device->GetVkDevice().destroy(semaphore); }
		for (const vk::Semaphore& semaphore : _renderFinishedSemaphore) { _device->GetVkDevice().destroy(semaphore); }
		for (const vk::Fence& fence : _inFlightFence) { _device->GetVkDevice().destroy(fence); }

		_quadModelBuffer.Destroy();

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) { _uniformBuffers[i].Destroy(); }

		_device->GetVkDevice().destroy(_descriptorSetLayout);
		_device->GetVkDevice().destroy(_descriptorPool);

		_device->Destroy();
		_instance.Destroy();
	}

	void Context::DrawFrame()
	{
		_device->GetVkDevice().waitForFences(1, &_inFlightFence[_currentFrame], vk::True, UINT64_MAX);

		vk::ResultValue<uint32_t> imageIndexResult = _device->GetVkDevice().acquireNextImageKHR(_device->GetSwapchain().GetVkSwapchain(),
																								UINT64_MAX,
																								_imageAvailableSemaphore[_currentFrame],
																								VK_NULL_HANDLE);

		if (imageIndexResult.result == vk::Result::eErrorOutOfDateKHR)
		{
			_device->GetVkDevice().waitIdle();
			_device->CreateSwapchain(_instance.GetVkSurface(), _window->GetSize());

			return;
		}
		else if (imageIndexResult.result != vk::Result::eSuccess && imageIndexResult.result != vk::Result::eSuboptimalKHR)
		{
			ErrorHandler::ThrowRuntimeError("failed to acquire swap chain image!");
		}

		_device->GetVkDevice().resetFences(1, &_inFlightFence[_currentFrame]);

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto  currentTime = std::chrono::high_resolution_clock::now();
		float time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		_uniformBufferData[_currentFrame]->model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		_uniformBufferData[_currentFrame]->view  = glm::lookAt(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		_uniformBufferData[_currentFrame]->proj  = glm::perspective(glm::radians(45.0f),
                                                                   static_cast<float>(_device->GetSwapchain().GetExtend().width) /
                                                                           -static_cast<float>(_device->GetSwapchain().GetExtend().height),
                                                                   0.1f,
                                                                   10.0f);

		_commandBuffer[_currentFrame].reset({});
		RecordCommandBuffer(_commandBuffer[_currentFrame], imageIndexResult.value);

		vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
		vk::SubmitInfo         submitInfo   = vk::SubmitInfo(_imageAvailableSemaphore[_currentFrame],
                                                   waitStages,
                                                   _commandBuffer[_currentFrame],
                                                   _renderFinishedSemaphore[_currentFrame]);

		_device->GetGraphicsQueue().submit(submitInfo, _inFlightFence[_currentFrame]);

		vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR(_renderFinishedSemaphore[_currentFrame],
															_device->GetSwapchain().GetVkSwapchain(),
															imageIndexResult.value,
															nullptr);

		vk::Result presentResult = _device->GetPresentQueue().presentKHR(presentInfo);

		if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR || _frameBufferResized)
		{
			_device->GetVkDevice().waitIdle();
			_frameBufferResized = false;
			_device->CreateSwapchain(_instance.GetVkSurface(), _window->GetSize());
		}
		else if (presentResult != vk::Result::eSuccess) { ErrorHandler::ThrowRuntimeError("failed to present swap chain image!"); }

		_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void Context::CreateInstance(SDL_Window* sdlWindow)
	{
		uint32_t extensionCount;
		SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, nullptr);

		std::vector<const char*> extensionNames = std::vector<const char*>(extensionCount, nullptr);
		SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, extensionNames.data());

		vk::ApplicationInfo applicationInfo = vk::ApplicationInfo("Project Thalia",
																  VK_MAKE_VERSION(1, 0, 0),
																  "No Engine",
																  VK_MAKE_VERSION(1, 0, 0),
																  VK_API_VERSION_1_3);

		_instance = Instance(extensionNames, _validationLayers, applicationInfo);

		// Create surface from sdl
		VkSurfaceKHR surfaceHandle         = VK_NULL_HANDLE;
		SDL_bool     surfaceCreationResult = SDL_Vulkan_CreateSurface(sdlWindow, static_cast<VkInstance>(_instance.GetVkInstance()), &surfaceHandle);
		if (surfaceCreationResult == SDL_FALSE) { ErrorHandler::ThrowRuntimeError("Failed to create SDL Vulkan surface!"); }

		_instance.SetVkSurface(surfaceHandle);
	}
}