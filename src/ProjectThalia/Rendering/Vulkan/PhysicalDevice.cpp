#include "ProjectThalia/Rendering/Vulkan/PhysicalDevice.hpp"
#include "ProjectThalia/Debug/Log.hpp"
#include "ProjectThalia/ErrorHandler.hpp"

#include <set>

namespace ProjectThalia::Rendering::Vulkan
{
	PhysicalDevice::PhysicalDevice(const vk::Instance&      instance,
								   const vk::SurfaceKHR&    surface,
								   std::vector<const char*> _requiredExtensions,
								   std::vector<const char*> _requiredValidationLayers) :
		_extensions(_requiredExtensions),
		_validationLayers(_requiredValidationLayers)
	{
		std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
		for (const vk::PhysicalDevice& physicalDevice : physicalDevices)
		{
			// Check device type
			vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();
			if (deviceProperties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu) { continue; }

			// Check Extensions
			std::vector<vk::ExtensionProperties> availableExtensions = physicalDevice.enumerateDeviceExtensionProperties();

			std::set<std::string> requiredExtensions = std::set<std::string>(_requiredExtensions.begin(), _requiredExtensions.end());

			for (const auto& extension : availableExtensions) { requiredExtensions.erase(extension.extensionName); }

			if (!requiredExtensions.empty()) { continue; }

			// Check queues
			_queueFamilyIndices = QueueFamilyIndices();

			auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
			int  i                     = 0;
			for (const vk::QueueFamilyProperties& queueFamily : queueFamilyProperties)
			{
				unsigned int presentSupport = physicalDevice.getSurfaceSupportKHR(i, surface);

				if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics && !_queueFamilyIndices.GraphicsFamily.has_value())
				{
					LOG("graphics index: {0}", i);
					_queueFamilyIndices.GraphicsFamily = i;
				}

				if (presentSupport && !_queueFamilyIndices.PresentFamily.has_value())
				{
					LOG("present index: {0}", i);
					_queueFamilyIndices.PresentFamily = i;
				}

				if (_queueFamilyIndices.isComplete()) { break; }

				i++;
			}
			if (!_queueFamilyIndices.isComplete()) { continue; }

			// Check swap chain support
			_swapchainSupportDetails = SwapchainSupportDetails();

			_swapchainSupportDetails.Capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
			_swapchainSupportDetails.Formats      = physicalDevice.getSurfaceFormatsKHR(surface);
			_swapchainSupportDetails.PresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

			if (_swapchainSupportDetails.Formats.empty() || _swapchainSupportDetails.PresentModes.empty()) { continue; }

			// Select device
			_vkPhysicalDevice = physicalDevice;
			LOG("Selected physical device: {0}", deviceProperties.deviceName.data());
			break;
		}

		// Check if we found a compatible GPU
		if (_vkPhysicalDevice == VK_NULL_HANDLE)
		{
			ErrorHandler::ThrowRuntimeError("This device does not have any gpus meeting the applications requirements");
		}

		// Select Image format
		_imageFormat = _swapchainSupportDetails.Formats[0];
		for (const auto& availableFormat : _swapchainSupportDetails.Formats)
		{
			if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			{
				_imageFormat = availableFormat;
			}
		}
	}

	const vk::PhysicalDevice& PhysicalDevice::GetVkPhysicalDevice() const { return _vkPhysicalDevice; }

	const PhysicalDevice::QueueFamilyIndices& PhysicalDevice::GetQueueFamilyIndices() const { return _queueFamilyIndices; }

	const PhysicalDevice::SwapchainSupportDetails& PhysicalDevice::GetSwapchainSupportDetails() const { return _swapchainSupportDetails; }

	const std::vector<const char*>& PhysicalDevice::GetExtensions() const { return _extensions; }

	const std::vector<const char*>& PhysicalDevice::GetValidationLayers() const { return _validationLayers; }

	const vk::SurfaceFormatKHR& PhysicalDevice::GetImageFormat() const { return _imageFormat; }
}
