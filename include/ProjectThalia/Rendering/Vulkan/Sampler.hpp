#pragma once

#include "DeviceObject.hpp"
#include "glm/vec3.hpp"

#include <vulkan/vulkan.hpp>

namespace ProjectThalia::Rendering::Vulkan
{
	class Device;

	class Sampler final : DeviceObject
	{
		public:
			enum class Filter
			{
				Nearest = VkFilter::VK_FILTER_NEAREST,
				Linear  = VkFilter::VK_FILTER_LINEAR,
			};

			enum class MipmapMode
			{
				Nearest = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST,
				Linear  = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
			};

			enum class WrapMode
			{
				Repeat              = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
				MirroredRepeat      = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
				ClampToEdge         = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				ClampToBorder       = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
				MirroredClampToEdge = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
			};

			struct CreateInfo
			{
				public:
					Filter                MagnificationFilter = Filter::Nearest;
					Filter                MinificationFilter  = Filter::Nearest;
					MipmapMode            MipmapMode          = MipmapMode::Nearest;
					glm::vec<3, WrapMode> WrapMode            = {WrapMode::Repeat, WrapMode::Repeat, WrapMode::Repeat};
					float                 MipLodBias          = 0;
					float                 MinLod              = 0.0f;
					float                 MaxLod              = 0.0f;
					float                 MaxAnisotropy       = 0.0f;
			};

		public:
			Sampler() = default;
			Sampler(Device* device, const CreateInfo& createInfo);

			void Destroy() override;

			[[nodiscard]] const vk::Sampler& GetVkSampler() const;

		private:
			vk::Sampler _vkSampler;
	};

}