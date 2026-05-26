#include <vulkan/vulkan_raii.hpp>
#include "helper.hpp"

#include <iostream>
#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

    std::vector<char> Helper::readFile(const std::string& filename)
	{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("OPENING_FILE_ERR");
    }

	std::vector<char> buffer(file.tellg());

	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

	file.close();

	return buffer;
	}

    std::vector<const char *> Helper::getRequiredInstanceExtensions(bool enableValidationLayers)
	{
		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers)
		{
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

    VKAPI_ATTR vk::Bool32 VKAPI_CALL Helper::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
	{
		const char* severityStr = "INFO";
    if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        severityStr = "ERROR";
    else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        severityStr = "WARNING";
    else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
        severityStr = "INFO";
    else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
        severityStr = "VERBOSE";

    const char* typeStr = "UNKNOWN";
    if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
        typeStr = "VALIDATION";
    else if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        typeStr = "PERFORMANCE";
    else if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral)
        typeStr = "GENERAL";

    std::cerr
        << "ValidationLayer: "
        << "[" << severityStr << " / " << typeStr << "]: "
        << pCallbackData->pMessage
        << "\n";

    return vk::False;
	}

    uint32_t Helper::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
	{
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

    vk::SurfaceFormatKHR Helper::chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats)
	{
		assert(!availableFormats.empty());
		const auto formatIt = std::ranges::find_if(
		    availableFormats,
		    [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
		return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
	}

    vk::PresentModeKHR Helper::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
	{
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
		return std::ranges::any_of(availablePresentModes,
		                           [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
		           vk::PresentModeKHR::eMailbox :
		           vk::PresentModeKHR::eFifo;
	}

    vk::Extent2D Helper::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities,GLFWwindow *window)
        {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            {
                return capabilities.currentExtent;
            }
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            return {
                std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
        }

