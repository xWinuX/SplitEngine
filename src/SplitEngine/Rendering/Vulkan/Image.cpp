#include "SplitEngine/Rendering/Vulkan/Image.hpp"
#include "SplitEngine/ErrorHandler.hpp"
#include "SplitEngine/Rendering/Vulkan/BufferFactory.hpp"
#include "SplitEngine/Rendering/Vulkan/Device.hpp"
#include "SplitEngine/Rendering/Vulkan/Instance.hpp"
#include "SplitEngine/Rendering/Vulkan/PhysicalDevice.hpp"
#include "SplitEngine/Rendering/Vulkan/Utility.hpp"

namespace SplitEngine::Rendering::Vulkan
{
	Image::Image(Device* device, const std::byte* pixels, vk::DeviceSize pixelsSizeInBytes, vk::Extent3D extend, CreateInfo createInfo) :
		DeviceObject(device)
	{
		vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo({},
		                                                          vk::ImageType::e2D,
		                                                          createInfo.Format,
		                                                          extend,
		                                                          1,
		                                                          1,
		                                                          vk::SampleCountFlagBits::e1,
		                                                          vk::ImageTiling::eOptimal,
		                                                          createInfo.Usage);

		Allocator::MemoryAllocationCreateInfo allocationCreateInfo{};
		allocationCreateInfo.Usage         = Allocator::GpuOnly;
		allocationCreateInfo.RequiredFlags = Allocator::LocalDevice;

		_imageAllocation = GetDevice()->GetPhysicalDevice().GetInstance().GetAllocator().CreateImage(imageCreateInfo, allocationCreateInfo);

		vk::CommandBuffer commandBuffer;
		if (createInfo.TransitionLayout != vk::ImageLayout::eDepthStencilAttachmentOptimal)
		{
			commandBuffer = GetDevice()->GetQueueFamily(QueueType::Graphics).BeginOneshotCommands();
		}

		// Copy pixel data to image
		Buffer transferBuffer;
		if (pixels)
		{
			transferBuffer = BufferFactory::CreateTransferBuffer(device, pixels, pixelsSizeInBytes);
			transferBuffer.CopyData(pixels, pixelsSizeInBytes, 0);

			TransitionLayout(commandBuffer, vk::ImageLayout::eTransferDstOptimal);

			vk::ImageSubresourceLayers imageSubresourceLayers = vk::ImageSubresourceLayers(createInfo.AspectMask, 0, 0, 1);
			vk::BufferImageCopy        bufferImageCopy        = vk::BufferImageCopy(0, 0, 0, imageSubresourceLayers, { 0, 0, 0 }, extend);

			commandBuffer.copyBufferToImage(transferBuffer.GetVkBuffer(), GetVkImage(), _layout, bufferImageCopy);
		}

		// TODO: Fix this hack
		if (createInfo.TransitionLayout != vk::ImageLayout::eDepthStencilAttachmentOptimal)
		{
			TransitionLayout(commandBuffer, createInfo.TransitionLayout);

			GetDevice()->GetQueueFamily(QueueType::Graphics).EndOneshotCommands();
			transferBuffer.Destroy();
		}

		// Create image view
		vk::ImageSubresourceRange imageSubresourceRange = vk::ImageSubresourceRange(createInfo.AspectMask, 0, 1, 0, 1);
		vk::ImageViewCreateInfo   imageViewCreateInfo   = vk::ImageViewCreateInfo({}, GetVkImage(), vk::ImageViewType::e2D, createInfo.Format, {}, imageSubresourceRange);

		_view = GetDevice()->GetVkDevice().createImageView(imageViewCreateInfo);
	}

	void Image::TransitionLayout(const vk::ImageLayout newLayout)
	{
		const vk::CommandBuffer commandBuffer = GetDevice()->GetQueueFamily(QueueType::Graphics).BeginOneshotCommands();

		TransitionLayout(commandBuffer, newLayout);

		GetDevice()->GetQueueFamily(QueueType::Graphics).EndOneshotCommands();
	}

	void Image::TransitionLayout(const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout)
	{
		if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) { return; }

		constexpr vk::ImageSubresourceRange subresourceRange   = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
		vk::ImageMemoryBarrier              imageMemoryBarrier = vk::ImageMemoryBarrier({},
		                                                                                {},
		                                                                                _layout,
		                                                                                newLayout,
		                                                                                vk::QueueFamilyIgnored,
		                                                                                vk::QueueFamilyIgnored,
		                                                                                GetVkImage(),
		                                                                                subresourceRange);

		vk::PipelineStageFlagBits sourceStage{};
		vk::PipelineStageFlagBits destinationStage{};

		if (_layout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNone;
			imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (_layout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage      = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else if (_layout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
		{
			imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNone;
			imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

			sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
		}
		else { ErrorHandler::ThrowRuntimeError("Unsupported image layout transition"); }

		commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);

		_layout = newLayout;
	}

	void Image::Destroy()
	{
		Utility::DeleteDeviceHandle(GetDevice(), _view);
		GetDevice()->GetPhysicalDevice().GetInstance().GetAllocator().DestroyImage(_imageAllocation);
	}

	const vk::Image& Image::GetVkImage() const { return _imageAllocation.Image; }

	const vk::ImageView& Image::GetView() const { return _view; }

	const vk::ImageLayout& Image::GetLayout() const { return _layout; }
}
