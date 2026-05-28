#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

class Helper {
    public:
        static std::vector<char>                                readFile(const std::string& filename);
        static std::vector<const char *>                        getRequiredInstanceExtensions(bool enableValidationLayers);
        static VKAPI_ATTR vk::Bool32 VKAPI_CALL                 debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *);
        static uint32_t                                         chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities);
        static vk::SurfaceFormatKHR                             chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats);
        static vk::PresentModeKHR                               chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes);
        static vk::Extent2D                                     chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities,GLFWwindow *window);
        static bool                                             isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice,std::vector<const char *> requiredDeviceExtension);
};  