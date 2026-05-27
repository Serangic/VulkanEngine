#pragma once

#include <vulkan/vulkan_raii.hpp>

class Device {
    public:
        static void                                                    copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size,vk::raii::CommandPool const& commandPool, vk::raii::Device const& device, vk::raii::Queue queue);
        static uint32_t                                                findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties,vk::raii::PhysicalDevice const& physicalDevice);
        static std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>     createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,vk::raii::Device const& device, vk::raii::PhysicalDevice const& physicalDevice);
        [[nodiscard]] static vk::raii::ShaderModule                    createShaderModule(const std::vector<char>& code,vk::raii::Device const& device);
};