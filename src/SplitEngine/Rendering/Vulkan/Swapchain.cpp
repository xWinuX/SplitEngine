#include "SplitEngine/Rendering/Vulkan/Swapchain.hpp"

#include "SplitEngine/Rendering/Vulkan/PhysicalDevice.hpp"
#include "SplitEngine/Rendering/Vulkan/Utility.hpp"

namespace SplitEngine::Rendering::Vulkan
{
	Swapchain::Swapchain(Device* device, const vk::SurfaceKHR& surface, vk::Extent2D size) :
		DeviceObject(device)
	{
		const PhysicalDevice& physicalDevice = GetDevice()->GetPhysicalDevice();

		// Select surface format
		const PhysicalDevice::SwapchainSupportDetails& swapchainSupportDetails = physicalDevice.GetSwapchainSupportDetails();

		// Select present mode
		vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
		for (const auto& availablePresentMode: swapchainSupportDetails.PresentModes)
		{
			if (availablePresentMode == vk::PresentModeKHR::eMailbox)
			{
				presentMode = vk::PresentModeKHR::eMailbox;
				break;
			}
		}

		// Select swap extend
		const vk::SurfaceCapabilitiesKHR& capabilities = swapchainSupportDetails.Capabilities;
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) { _extend = capabilities.currentExtent; }
		else
		{
			_extend.width  = std::clamp(size.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			_extend.height = std::clamp(size.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		// Select image count
		uint32_t imageCount = swapchainSupportDetails.Capabilities.minImageCount + 1;
		if (swapchainSupportDetails.Capabilities.maxImageCount > 0 && imageCount > swapchainSupportDetails.Capabilities.maxImageCount)
		{
			imageCount = swapchainSupportDetails.Capabilities.maxImageCount;
		}

		// Create swap chain
		vk::SwapchainCreateInfoKHR swapChainCreateInfo = vk::SwapchainCreateInfoKHR({},
		                                                                            surface,
		                                                                            imageCount,
		                                                                            physicalDevice.GetImageFormat().format,
		                                                                            physicalDevice.GetImageFormat().colorSpace,
		                                                                            size,
		                                                                            1,
		                                                                            vk::ImageUsageFlagBits::eColorAttachment);

		const PhysicalDevice::QueueFamilyInfos& queueFamilyIndices = physicalDevice.GetQueueFamilyInfos();
		std::vector<uint32_t> queueFamilies = {
			queueFamilyIndices[static_cast<size_t>(QueueType::Graphics)].Index,
			queueFamilyIndices[static_cast<size_t>(QueueType::Present)].Index
		};

		if (queueFamilyIndices[static_cast<size_t>(QueueType::Graphics)].Index != queueFamilyIndices[static_cast<size_t>(QueueType::Present)].Index)
		{
			swapChainCreateInfo.setImageSharingMode(vk::SharingMode::eConcurrent);
			swapChainCreateInfo.setQueueFamilyIndices(queueFamilies);
		}
		else { swapChainCreateInfo.setImageSharingMode(vk::SharingMode::eExclusive); }

		swapChainCreateInfo.setPreTransform(swapchainSupportDetails.Capabilities.currentTransform);
		swapChainCreateInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
		swapChainCreateInfo.setPresentMode(presentMode);
		swapChainCreateInfo.setClipped(vk::True);
		swapChainCreateInfo.setOldSwapchain(VK_NULL_HANDLE);

		_vkSwapchain = GetDevice()->GetVkDevice().createSwapchainKHR(swapChainCreateInfo);
		_images      = GetDevice()->GetVkDevice().getSwapchainImagesKHR(_vkSwapchain);

		// Create image views
		_imageViews.resize(_images.size());

		for (int i = 0; i < _images.size(); ++i)
		{
			vk::ImageViewCreateInfo imageViewCreateInfo = vk::ImageViewCreateInfo({},
			                                                                      _images[i],
			                                                                      vk::ImageViewType::e2D,
			                                                                      physicalDevice.GetImageFormat().format,
			                                                                      {
				                                                                      vk::ComponentSwizzle::eIdentity,
				                                                                      vk::ComponentSwizzle::eIdentity,
				                                                                      vk::ComponentSwizzle::eIdentity,
				                                                                      vk::ComponentSwizzle::eIdentity
			                                                                      },
			                                                                      { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

			_imageViews[i] = GetDevice()->GetVkDevice().createImageView(imageViewCreateInfo);
		}


		// Create depth image
		Image::CreateInfo imageCreateInfo{};
		imageCreateInfo.Usage            = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		imageCreateInfo.TransitionLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		imageCreateInfo.AspectMask       = vk::ImageAspectFlagBits::eDepth;
		imageCreateInfo.Format           = GetDevice()->GetPhysicalDevice().GetDepthImageFormat();
		imageCreateInfo.RequiredFlags    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		_depthImage.reset(new Image(GetDevice(), nullptr, 0, { _extend.width, _extend.height, 1 }, imageCreateInfo));

		// Create frame buffers
		_frameBuffers.resize(_imageViews.size());
		for (size_t i = 0; i < _imageViews.size(); i++)
		{
			std::vector<vk::ImageView> attachments = { _imageViews[i], _depthImage->GetView() };

			vk::FramebufferCreateInfo framebufferInfo = vk::FramebufferCreateInfo({}, device->GetRenderPass().GetVkRenderPass(), attachments, _extend.width, _extend.height, 1);
			_frameBuffers[i]                          = device->GetVkDevice().createFramebuffer(framebufferInfo);
		}
	}

	const vk::SwapchainKHR& Swapchain::GetVkSwapchain() const { return _vkSwapchain; }

	const std::vector<vk::Image>& Swapchain::GetImages() const { return _images; }

	const std::vector<vk::ImageView>& Swapchain::GetImageViews() const { return _imageViews; }

	const vk::Extent2D& Swapchain::GetExtend() const { return _extend; }

	const std::vector<vk::Framebuffer>& Swapchain::GetFrameBuffers() const { return _frameBuffers; }

	void Swapchain::Destroy()
	{
		Utility::DeleteDeviceHandle(GetDevice(), _vkSwapchain);

		for (const vk::ImageView& imageView: _imageViews) { Utility::DeleteDeviceHandle(GetDevice(), imageView); }
		for (const vk::Framebuffer& frameBuffer: _frameBuffers) { Utility::DeleteDeviceHandle(GetDevice(), frameBuffer); }

		if (_depthImage) { _depthImage->Destroy(); }
	}
}
