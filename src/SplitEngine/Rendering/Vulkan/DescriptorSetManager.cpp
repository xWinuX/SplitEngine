#include "SplitEngine/Rendering/Vulkan/DescriptorSetManager.hpp"
#include "SplitEngine/Rendering/Vulkan/Buffer.hpp"
#include "SplitEngine/Rendering/Vulkan/Device.hpp"
#include "SplitEngine/Rendering/Vulkan/Utility.hpp"

#include <numeric>
#include <utility>
#include <vector>

namespace SplitEngine::Rendering::Vulkan
{
	DescriptorSetManager::DescriptorSetManager(Device*                                     device,
											   std::vector<vk::DescriptorSetLayoutBinding> descriptorLayoutBindings,
											   std::vector<vk::DescriptorPoolSize>         descriptorPoolSizes,
											   std::vector<vk::WriteDescriptorSet>         writeDescriptorSets,
											   uint32_t                                    maxSetsPerPool) :
		DeviceObject(device),
		_descriptorPoolSizes(std::move(descriptorPoolSizes)),
		_writeDescriptorSets(std::move(writeDescriptorSets)),
		_maxSetsPerPool(maxSetsPerPool)
	{
		vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo({}, descriptorLayoutBindings);
		_descriptorSetLayout                                            = GetDevice()->GetVkDevice().createDescriptorSetLayout(descriptorSetLayoutCreateInfo);

		AllocateNewDescriptorPool();
	}

	DescriptorSetManager::DescriptorSetAllocation DescriptorSetManager::AllocateDescriptorSet()
	{
		// Find pool with available space
		DescriptorPoolInstance* validDescriptorPoolInstance;
		uint32_t                descriptorPoolIndex = -1;
		for (uint32_t i = 0; i < _descriptorPoolInstances.size(); i++)
		{
			if (!_descriptorPoolInstances[i].Available.IsEmpty())
			{
				validDescriptorPoolInstance = &_descriptorPoolInstances[i];
				descriptorPoolIndex         = i;
				break;
			}
		}

		// Allocate new pool if existing are full
		if (descriptorPoolIndex == -1)
		{
			AllocateNewDescriptorPool();
			validDescriptorPoolInstance = &_descriptorPoolInstances.back();
			descriptorPoolIndex         = _descriptorPoolInstances.size() - 1;
		}

		// Allocate descriptor set
		vk::DescriptorSet             descriptorSet;
		vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo = vk::DescriptorSetAllocateInfo(validDescriptorPoolInstance->DescriptorPool,
																								_descriptorSetLayout);


		GetDevice()->GetVkDevice().allocateDescriptorSets(&descriptorSetAllocateInfo, &descriptorSet);


		uint32_t insertionIndex                                     = validDescriptorPoolInstance->Available.Pop();
		validDescriptorPoolInstance->DescriptorSets[insertionIndex] = descriptorSet;

		// Create descriptor resources
		std::vector<Buffer>                               shaderBuffers            = std::vector<Buffer>();
		std::vector<vk::DescriptorBufferInfo*>            bufferInfos              = std::vector<vk::DescriptorBufferInfo*>();
		std::vector<vk::WriteDescriptorSet>               imageWriteDescriptorSets = std::vector<vk::WriteDescriptorSet>();
		std::vector<std::vector<vk::DescriptorImageInfo>> descriptorImageInfos;

		Buffer buffer;
		for (vk::WriteDescriptorSet& writeDescriptorSet : _writeDescriptorSets)
		{
			std::vector<vk::DescriptorImageInfo> imageInfos = std::vector<vk::DescriptorImageInfo>(0);

			writeDescriptorSet.dstSet = descriptorSet;
			switch (writeDescriptorSet.descriptorType)
			{
				case vk::DescriptorType::eStorageBuffer:
					shaderBuffers.push_back(std::move(Buffer::CreateStorageBuffer(GetDevice(), writeDescriptorSet.pBufferInfo->range)));
					bufferInfos.push_back(new vk::DescriptorBufferInfo(*writeDescriptorSet.pBufferInfo));
					bufferInfos.back()->buffer = shaderBuffers.back().GetVkBuffer();
					delete writeDescriptorSet.pBufferInfo;
					writeDescriptorSet.pBufferInfo = bufferInfos.back();
					break;

				case vk::DescriptorType::eUniformBuffer:
					shaderBuffers.push_back(std::move(Buffer::CreateUniformBuffer(GetDevice(), writeDescriptorSet.pBufferInfo->range)));
					bufferInfos.push_back(new vk::DescriptorBufferInfo(*writeDescriptorSet.pBufferInfo));
					bufferInfos.back()->buffer = shaderBuffers.back().GetVkBuffer();
					delete writeDescriptorSet.pBufferInfo;
					writeDescriptorSet.pBufferInfo = bufferInfos.back();
					break;

				case vk::DescriptorType::eCombinedImageSampler:
					for (int i = 0; i < writeDescriptorSet.descriptorCount; ++i)
					{
						imageInfos.emplace_back(*GetDevice()->GetDefaultSampler(),
												GetDevice()->GetDefaultImage().GetView(),
												GetDevice()->GetDefaultImage().GetLayout());
					}

					writeDescriptorSet.pImageInfo = imageInfos.data();
					imageWriteDescriptorSets.push_back(writeDescriptorSet);
					break;
			}

			descriptorImageInfos.push_back(std::move(imageInfos));
		}

		GetDevice()->GetVkDevice().updateDescriptorSets(_writeDescriptorSets, nullptr);

		// Create allocation object
		DescriptorSetAllocation descriptorSetAllocation;
		descriptorSetAllocation.DescriptorSet            = validDescriptorPoolInstance->DescriptorSets[insertionIndex];
		descriptorSetAllocation._descriptorPoolIndex     = descriptorPoolIndex;
		descriptorSetAllocation._descriptorSetIndex      = insertionIndex;
		descriptorSetAllocation.ShaderBuffers            = std::move(shaderBuffers);
		descriptorSetAllocation.ImageWriteDescriptorSets = std::move(imageWriteDescriptorSets);
		descriptorSetAllocation.ImageInfos               = std::move(descriptorImageInfos);

		return descriptorSetAllocation;
	}

	void DescriptorSetManager::AllocateNewDescriptorPool()
	{
		vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
																							 _maxSetsPerPool,
																							 _descriptorPoolSizes);

		AvailableStack<uint32_t> availableStack = AvailableStack<uint32_t>(_maxSetsPerPool);

		// Fill from 0 to size - 1
		std::iota(availableStack.begin(), availableStack.end(), 0);

		DescriptorPoolInstance descriptorPoolInstance = {GetDevice()->GetVkDevice().createDescriptorPool(descriptorPoolCreateInfo),
														 std::vector<vk::DescriptorSet>(_maxSetsPerPool, VK_NULL_HANDLE),
														 std::move(availableStack)};

		_descriptorPoolInstances.push_back(std::move(descriptorPoolInstance));
	}

	void DescriptorSetManager::Destroy()
	{

		Utility::DeleteDeviceHandle(GetDevice(), _descriptorSetLayout);
		for (const DescriptorPoolInstance& descriptorPoolInstance : _descriptorPoolInstances)
		{
			Utility::DeleteDeviceHandle(GetDevice(), descriptorPoolInstance.DescriptorPool);
		}

		for (const vk::WriteDescriptorSet& writeDescriptorSet : _writeDescriptorSets)
		{
			delete writeDescriptorSet.pBufferInfo;
			//delete writeDescriptorSet.pImageInfo;
		}
	}

	void DescriptorSetManager::DeallocateDescriptorSet(DescriptorSetManager::DescriptorSetAllocation& descriptorSetAllocation)
	{
		DescriptorPoolInstance& descriptorPoolInstance = _descriptorPoolInstances[descriptorSetAllocation._descriptorPoolIndex];
		GetDevice()->GetVkDevice().freeDescriptorSets(descriptorPoolInstance.DescriptorPool,
													  descriptorPoolInstance.DescriptorSets[descriptorSetAllocation._descriptorSetIndex]);


		descriptorPoolInstance.DescriptorSets[descriptorSetAllocation._descriptorSetIndex] = VK_NULL_HANDLE;
		descriptorPoolInstance.Available.Push(descriptorSetAllocation._descriptorSetIndex);

		for (Buffer& buffer : descriptorSetAllocation.ShaderBuffers) { buffer.Destroy(); }
	}

	const vk::DescriptorSetLayout& DescriptorSetManager::GetDescriptorSetLayout() const { return _descriptorSetLayout; }

}