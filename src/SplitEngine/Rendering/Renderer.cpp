#include "SplitEngine/Rendering/Renderer.hpp"

#include "SplitEngine/ErrorHandler.hpp"
#include "SplitEngine/Debug/Performance.hpp"

#include <chrono>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <SplitEngine/RenderingSettings.hpp>

#include "SplitEngine/Window.hpp"

namespace SplitEngine::Rendering
{
	Renderer::Renderer(ApplicationInfo& applicationInfo, RenderingSettings&& renderingSettings):
		_window(Window(applicationInfo.Name, 500, 500)),
		_vulkanInstance(Vulkan::Instance(_window, applicationInfo, std::move(renderingSettings)))
	{
		_window.OnResize.Add([this](int width, int height) { _frameBufferResized = true; });

		StartImGuiFrame();
	}

	Renderer::~Renderer()
	{
		LOG("Shutting down Renderer...");
		_vulkanInstance.Destroy();
		_window.Close();
	}

	void Renderer::StartImGuiFrame() const
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(_window.GetSDLWindow());

		ImGui::NewFrame();
	}

	void Renderer::HandleEvents(SDL_Event event)
	{
		ImGui_ImplSDL2_ProcessEvent(&event);

		_window.HandleEvents(event);
	}

	Vulkan::Instance& Renderer::GetVulkanInstance() { return _vulkanInstance; }

	Window& Renderer::GetWindow() { return _window; }

	void Renderer::BeginRender()
	{
		if (_window.IsMinimized())
		{
			_wasSkipped = true;
			return;
		}

		Vulkan::Device& device = _vulkanInstance.GetPhysicalDevice().GetDevice();

		device.GetVkDevice().waitForFences(device.GetInFlightFence(), vk::True, UINT64_MAX);

		vk::ResultValue<uint32_t> imageIndexResult        = vk::ResultValue<uint32_t>(vk::Result::eIncomplete, 0);
		bool                      needToRecreateSwapChain = false;
		try
		{
			imageIndexResult = device.GetVkDevice().acquireNextImageKHR(device.GetSwapchain().GetVkSwapchain(), UINT64_MAX, device.GetImageAvailableSemaphore(), VK_NULL_HANDLE);
		}
		catch (vk::OutOfDateKHRError& outOfDateKhrError) { needToRecreateSwapChain = true; } catch (vk::SystemError& systemError)
		{
			ErrorHandler::ThrowRuntimeError(std::format("failed to present swap chain image! {0}", systemError.what()));
		}

		_latestImageIndexResult = imageIndexResult.value;

		if (needToRecreateSwapChain)
		{
			device.GetVkDevice().waitIdle();
			device.CreateSwapchain(_vulkanInstance.GetVkSurface(), _window.GetSize());

			_wasSkipped = true;

			return;
		}
		//else if (imageIndexResult.result != vk::Result::eSuccess) { ErrorHandler::ThrowRuntimeError("failed to acquire swap chain image!"); }

		device.GetVkDevice().resetFences(device.GetInFlightFence());

		const vk::CommandBuffer& commandBuffer = device.GetCommandBuffer();

		commandBuffer.reset({});

		constexpr vk::CommandBufferBeginInfo commandBufferBeginInfo = vk::CommandBufferBeginInfo({}, nullptr);

		commandBuffer.begin(commandBufferBeginInfo);

		constexpr vk::ClearValue clearColor      = vk::ClearValue({ 0.0f, 0.2f, 0.5f, 1.0f });
		constexpr vk::ClearValue depthClearColor = vk::ClearValue({ 1.0f, 0 });

		std::vector<vk::ClearValue> clearValues = { clearColor, depthClearColor };

		const vk::RenderPassBeginInfo renderPassBeginInfo = vk::RenderPassBeginInfo(device.GetRenderPass().GetVkRenderPass(),
		                                                                            device.GetSwapchain().GetFrameBuffers()[imageIndexResult.value],
		                                                                            { { 0, 0 }, device.GetSwapchain().GetExtend() },
		                                                                            clearValues);

		const vk::Viewport viewport = vk::Viewport(0,
		                                           static_cast<float>(device.GetSwapchain().GetExtend().height),
		                                           static_cast<float>(device.GetSwapchain().GetExtend().width),
		                                           -static_cast<float>(device.GetSwapchain().GetExtend().height),
		                                           0.0f,
		                                           1.0f);

		commandBuffer.setViewport(0, 1, &viewport);

		const vk::Rect2D scissor = vk::Rect2D({ 0, 0 }, device.GetSwapchain().GetExtend());
		commandBuffer.setScissor(0, 1, &scissor);

		commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
	}

	void Renderer::EndRender()
	{
		Vulkan::Device&          device        = _vulkanInstance.GetPhysicalDevice().GetDevice();
		const vk::CommandBuffer& commandBuffer = device.GetCommandBuffer();

		if (_wasSkipped)
		{
			_wasSkipped = false;
			return;
		}

		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

		commandBuffer.endRenderPass();
		commandBuffer.end();

		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		const vk::SubmitInfo   submitInfo   = vk::SubmitInfo(device.GetImageAvailableSemaphore(), waitStages, device.GetCommandBuffer(), device.GetRenderFinishedSemaphore());

		device.GetGraphicsQueue().submit(submitInfo, device.GetInFlightFence());

		const vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR(device.GetRenderFinishedSemaphore(), device.GetSwapchain().GetVkSwapchain(), _latestImageIndexResult, nullptr);

		bool       needToRecreateSwapChain = false;
		vk::Result presentResult           = vk::Result::eIncomplete;


		try { presentResult = device.GetPresentQueue().presentKHR(presentInfo); }
		catch (vk::OutOfDateKHRError& outOfDateKhrError) { needToRecreateSwapChain = true; } catch (vk::SystemError& systemError)
		{
			ErrorHandler::ThrowRuntimeError(std::format("failed to present swap chain image! {0}", systemError.what()));
		}

		if (needToRecreateSwapChain || presentResult == vk::Result::eSuboptimalKHR || _frameBufferResized)
		{
			device.GetVkDevice().waitIdle();
			_frameBufferResized = false;
			device.CreateSwapchain(_vulkanInstance.GetVkSurface(), _window.GetSize());
		}

		device.AdvanceFrame();

		StartImGuiFrame();
	}
}
