#pragma once

#include "SplitEngine/AssetDatabase.hpp"
#include "SplitEngine/Rendering/Shader.hpp"
#include "SplitEngine/Rendering/Vulkan/Pipeline.hpp"

namespace SplitEngine::Rendering
{
	class Material
	{
		public:
			struct CreateInfo
			{
				public:
					AssetHandle<Shader> _shader;
			};

		public:
			Material() = default;

			explicit Material(const CreateInfo& createInfo);

			~Material();

			Shader::Properties& GetProperties();

			void Update();

			void Bind(vk::CommandBuffer& commandBuffer);

			[[nodiscard]] AssetHandle<Shader> GetShader() const;

		private:
			AssetHandle<Shader> _shader;

			Vulkan::DescriptorSetAllocator::Allocation _instanceDescriptorSetAllocation;

			Shader::Properties _instanceProperties{};
	};
}
