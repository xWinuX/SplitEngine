#include "SplitEngine/Rendering/Vulkan/Buffer.hpp"
#include "SplitEngine/ErrorHandler.hpp"
#include "SplitEngine/Rendering/Vulkan/Device.hpp"

namespace SplitEngine::Rendering::Vulkan
{
	const char* Buffer::EMPTY_DATA[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, };

	Buffer::Buffer(Device*                                     device,
	               const vk::Flags<vk::BufferUsageFlagBits>    usage,
	               const vk::SharingMode                       sharingMode,
	               const Allocator::MemoryAllocationCreateInfo allocationCreateInfo,
	               vk::DeviceSize                              bufferSizeInBytes,
	               const vk::DeviceSize                        numSubBuffers,
	               const char**                                data,
	               const vk::DeviceSize*                       bufferSizesInBytes,
	               const vk::DeviceSize*                       dataSizesInBytes,
	               const vk::DeviceSize*                       dataElementSizesInBytes) :
		DeviceObject(device),
		_allocationCreateInfo(allocationCreateInfo)
	{
		InitializeSubBuffers(numSubBuffers, bufferSizesInBytes, dataSizesInBytes, dataElementSizesInBytes);

		CreateBuffer(usage, sharingMode, allocationCreateInfo, data);
	}

	Buffer::Buffer(Device*                                     device,
	               const vk::Flags<vk::BufferUsageFlagBits>    usage,
	               const vk::SharingMode                       sharingMode,
	               const Allocator::MemoryAllocationCreateInfo allocationCreateInfo,
	               const char**                                data,
	               const Buffer&                               buffer) :
		DeviceObject(device),
		_bufferSize(buffer._bufferSize)
	{
		// Copy sub buffers
		_subBuffers = buffer._subBuffers;

		CreateBuffer(usage, sharingMode, allocationCreateInfo, data);
	}

	Buffer::Buffer(Device*                                     device,
	               const vk::Flags<vk::BufferUsageFlagBits>    usage,
	               const vk::SharingMode                       sharingMode,
	               const Allocator::MemoryAllocationCreateInfo allocationCreateInfo,
	               const uint32_t                              numSubBuffers,
	               const vk::DeviceSize                        subBufferSizeInBytes) :
		DeviceObject(device)
	{
		const std::vector<vk::DeviceSize> subBufferSizes = std::vector<vk::DeviceSize>(numSubBuffers, subBufferSizeInBytes);
		InitializeSubBuffers(numSubBuffers, subBufferSizes.data(), subBufferSizes.data(), subBufferSizes.data());

		CreateBuffer(usage, sharingMode, allocationCreateInfo, EMPTY_DATA);
	}

	Buffer::Buffer(Device*                                     device,
	               const vk::Flags<vk::BufferUsageFlagBits>    usage,
	               const vk::SharingMode                       sharingMode,
	               const Allocator::MemoryAllocationCreateInfo allocationCreateInfo,
	               const vk::DeviceSize                        numSubBuffers,
	               const char**                                data,
	               const vk::DeviceSize*                       bufferSizesInBytes,
	               const size_t*                               dataSizesInBytes,
	               const vk::DeviceSize*                       dataElementSizesInBytes) :
		Buffer(device, usage, sharingMode, allocationCreateInfo, 0, numSubBuffers, data, bufferSizesInBytes, dataSizesInBytes, dataElementSizesInBytes) {}

	Buffer::Buffer(Device*                                     device,
	               const vk::Flags<vk::BufferUsageFlagBits>    usage,
	               const vk::SharingMode                       sharingMode,
	               const Allocator::MemoryAllocationCreateInfo allocationCreateInfo,
	               const vk::DeviceSize                        bufferSizeInBytes,
	               const char*                                 data,
	               const vk::DeviceSize                        dataSizeInBytes,
	               const vk::DeviceSize                        dataElementSizeInBytes) :
		Buffer(device, usage, sharingMode, allocationCreateInfo, bufferSizeInBytes, 1, &data, &dataSizeInBytes, &bufferSizeInBytes, &dataElementSizeInBytes) {}

	Buffer::Buffer(Device*                                     device,
	               const vk::Flags<vk::BufferUsageFlagBits>    usage,
	               const vk::SharingMode                       sharingMode,
	               const Allocator::MemoryAllocationCreateInfo allocationCreateInfo,
	               const char*                                 data,
	               const vk::DeviceSize                        bufferSizeInBytes,
	               const vk::DeviceSize                        dataElementSizeInBytes) :
		Buffer(device, usage, sharingMode, allocationCreateInfo, bufferSizeInBytes, 1, &data, &bufferSizeInBytes, &bufferSizeInBytes, &dataElementSizeInBytes) {}

	auto Buffer::CreateBuffer(const vk::Flags<vk::BufferUsageFlagBits>    usage,
	                          const vk::SharingMode                       sharingMode,
	                          const Allocator::MemoryAllocationCreateInfo allocationCreateInfo,
	                          const char* const*                          data) -> void
	{
		const vk::BufferCreateInfo bufferCreateInfo = vk::BufferCreateInfo({}, _bufferSize, usage, sharingMode);

		_bufferAllocation = GetDevice()->GetAllocator().CreateBuffer(bufferCreateInfo, allocationCreateInfo);

		// Copy data
		if (data != nullptr)
		{
			char*  mappedData = nullptr;
			size_t offsetInBytes = 0;
			bool   mappedBuffer  = false;
			for (int i = 0; i < _subBuffers.size(); i++)
			{
				if (data[i] != nullptr)
				{
					if (!mappedBuffer)
					{
						mappedData   = Map<char>();
						mappedBuffer = true;
					}
					memcpy(mappedData + offsetInBytes, data[i], _subBuffers[i].SizeInBytes);
				}
				offsetInBytes += _subBuffers[i].SizeInBytes;
			}

			if (mappedBuffer) { Unmap(); }
		}
	}

	void Buffer::InitializeSubBuffers(unsigned long long int numSubBuffers,
	                                  const vk::DeviceSize*  bufferSizesInBytes,
	                                  const vk::DeviceSize*  dataSizesInBytes,
	                                  const vk::DeviceSize*  dataElementSizesInBytes)
	{
		if (numSubBuffers > 12) { ErrorHandler::ThrowRuntimeError("Can't have more than 12 sub buffers!"); }

		const bool bufferSizeFromSubBuffers = _bufferSize == 0;

		_subBuffers.resize(numSubBuffers);
		for (int i = 0; i < numSubBuffers; i++)
		{
			_subBuffers[i] = { bufferSizesInBytes[i], dataSizesInBytes[i] / dataElementSizesInBytes[i], dataElementSizesInBytes[i], _bufferSize };
			if (bufferSizeFromSubBuffers) { _bufferSize += bufferSizesInBytes[i]; }
		}
	}

	const vk::Buffer& Buffer::GetVkBuffer() const { return _bufferAllocation.Buffer; }

	void Buffer::Copy(const Buffer& destinationBuffer) const
	{
		const vk::CommandBuffer commandBuffer = GetDevice()->BeginOneshotCommands();

		const vk::BufferCopy copyRegion = vk::BufferCopy(0, 0, _bufferSize);
		commandBuffer.copyBuffer(GetVkBuffer(), destinationBuffer.GetVkBuffer(), 1, &copyRegion);

		GetDevice()->EndOneshotCommands(commandBuffer);
	}

	void Buffer::Destroy() { GetDevice()->GetAllocator().DestroyBuffer(_bufferAllocation); }

	size_t Buffer::GetDataElementNum(const size_t index) const { return _subBuffers[index].NumElements; }

	size_t Buffer::GetNumSubBuffers() const { return _subBuffers.size(); }

	size_t Buffer::GetBufferElementNum(const size_t index) const { return _subBuffers[index].NumElements; }

	vk::DeviceSize Buffer::GetSizeInBytes(const size_t index) const { return _subBuffers[index].SizeInBytes; }

	void* Buffer::Map() const { return GetDevice()->GetAllocator().MapMemory(_bufferAllocation); }

	void Buffer::Unmap() const { GetDevice()->GetAllocator().UnmapMemory(_bufferAllocation); }

	void Buffer::Stage(const char** data) const
	{
		Buffer stagingBuffer = Buffer(GetDevice(), vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive, { Allocator::CpuOnly, }, data, *this);

		stagingBuffer.Copy(*this);
		stagingBuffer.Destroy();
	}

	void* Buffer::GetMappedData() const { return _bufferAllocation.AllocationInfo.MappedData; }

	void Buffer::Invalidate() const { GetDevice()->GetAllocator().InvalidateBuffer(_bufferAllocation); }

	void Buffer::Flush() const { GetDevice()->GetAllocator().FlushBuffer(_bufferAllocation); }
}
