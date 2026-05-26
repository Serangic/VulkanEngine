#pragma once

#include <vulkan/vulkan_raii.hpp>

class Device {
    public:
        void                                                    copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size);
        std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>     createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
        uint32_t                                                findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
        [[nodiscard]] vk::raii::ShaderModule                    createShaderModule(const std::vector<char>& code) const;
};