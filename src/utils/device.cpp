#include <vulkan/vulkan_raii.hpp>
#include "device.hpp"

void 		Device::copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size,vk::raii::CommandPool const& commandPool, vk::raii::Device const& device, vk::raii::Queue queue)
	{
		vk::CommandBufferAllocateInfo allocInfo{};                                  	// Structure describing how command buffers should be allocated
		allocInfo.commandPool = commandPool;                                         	// Command pool from which the command buffer will be allocated
		allocInfo.level = vk::CommandBufferLevel::ePrimary;                          	// Primary command buffer (can be submitted directly to a queue)
		allocInfo.commandBufferCount = 1;                                            	// Allocate exactly one command buffer

		vk::raii::CommandBuffer commandCopyBuffer =                                  	// RAII wrapper for a single command buffer used for copy operations
			std::move(device.allocateCommandBuffers(allocInfo).front());             	// Allocate and take ownership of the first (and only) command buffer

		vk::CommandBufferBeginInfo beginInfo{};                                      	// Structure specifying how the command buffer recording will begin
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;             	// Hint that this command buffer will be recorded and submitted only once

		commandCopyBuffer.begin(beginInfo);                                          	// Begin recording commands into the command buffer

		commandCopyBuffer.copyBuffer(                                                	// Record a buffer-to-buffer copy command
			*srcBuffer,                                                              	// Source buffer (staging buffer, CPU-visible)
			*dstBuffer,                                                              	// Destination buffer (device-local GPU buffer)
			vk::BufferCopy(0, 0, size)                                               	// Copy region: from offset 0 → offset 0, for 'size' bytes
		);

		commandCopyBuffer.end();                                                     	// Finish recording commands

		vk::SubmitInfo SubmitInfo{};                                                 	// Structure describing how to submit work to the queue
		SubmitInfo.commandBufferCount = 1;                                           	// Submitting one command buffer
		SubmitInfo.pCommandBuffers = &*commandCopyBuffer;                            	// Pointer to the raw VkCommandBuffer handle from RAII wrapper

		queue.submit(SubmitInfo, nullptr);                                           	// Submit the copy command buffer to the queue (no fence used)

		queue.waitIdle();                                                            	// Block CPU until the queue has finished executing all submitted work
	}

uint32_t 	Device::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties,vk::raii::PhysicalDevice const& physicalDevice)
	{
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
    	if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
    		{
       		return i;
    		}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Device::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,vk::raii::Device const& device, vk::raii::PhysicalDevice const& physicalDevice)
	{
		vk::BufferCreateInfo bufferInfo{};
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;

		vk::raii::Buffer       buffer          = vk::raii::Buffer(device, bufferInfo);
		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

		vk::MemoryAllocateInfo allocInfo{};
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties, physicalDevice);

		vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
		buffer.bindMemory(*bufferMemory, 0);
		return {std::move(buffer), std::move(bufferMemory)};
	}

[[nodiscard]] vk::raii::ShaderModule 				Device::createShaderModule(const std::vector<char>& code,vk::raii::Device const& device)
	{
		vk::ShaderModuleCreateInfo createInfo{}; 
		createInfo.codeSize = code.size() * sizeof(char); 
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		vk::raii::ShaderModule shaderModule{ device, createInfo };
		return shaderModule;
	}