#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct VKContext {
	vk::raii::Context                   context;
	vk::raii::Instance                  instance       			= nullptr;
	vk::raii::DebugUtilsMessengerEXT    debugMessenger 			= nullptr;
	vk::raii::SurfaceKHR 				surface 				= nullptr;
	vk::raii::PhysicalDevice            physicalDevice 			= nullptr;
	vk::raii::Device                    device         			= nullptr;

	uint32_t                         	queueIndex     			= ~0;
	vk::raii::Queue 					queue 					= nullptr;

	vk::raii::SwapchainKHR           	swapChain      			= nullptr;
	std::vector<vk::Image>           	swapChainImages;
	vk::SurfaceFormatKHR             	swapChainSurfaceFormat;
	vk::Extent2D                     	swapChainExtent;
	std::vector<vk::raii::ImageView> 	swapChainImageViews;

	vk::raii::RenderPass 				renderpass{nullptr};
	vk::raii::DescriptorSetLayout 		descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout 			pipelineLayout{nullptr};
	vk::raii::Pipeline 					graphicsPipeline{nullptr};
	std::vector<vk::raii::Framebuffer> swapChainFramebuffers;

	vk::raii::CommandPool 				commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> commandBuffers;

	std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Fence> inFlightFences;

	vk::raii::Buffer vertexBuffer 				= nullptr;
	vk::raii::DeviceMemory vertexBufferMemory 	= nullptr;
	vk::raii::Buffer       indexBuffer        = nullptr;
	vk::raii::DeviceMemory indexBufferMemory  = nullptr;

	std::vector<vk::raii::Buffer>       uniformBuffers;
	std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
	std::vector<void *>                 uniformBuffersMapped;

	vk::raii::DescriptorPool descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptorSets;

	const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
	std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName,vk::KHRSynchronization2ExtensionName};

	const int MAX_FRAMES_IN_FLIGHT = 2;
	uint32_t currentFrame = 0;

	bool framebufferResized = false;
};

struct GLFWContext {
	GLFWwindow *window = nullptr;
	uint16_t WIDTH  = 1000;
	uint16_t HEIGHT = 1000;
};

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

	static vk::VertexInputBindingDescription getBindingDescription()
    {
		vk::VertexInputBindingDescription VIBD{};
		VIBD.binding = 0;
		VIBD.stride = sizeof(Vertex);
		VIBD.inputRate = vk::VertexInputRate::eVertex;
      	return VIBD; 
    }

	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
	{
		std::array<vk::VertexInputAttributeDescription, 2> attributes{};

		attributes[0].location = 0;
		attributes[0].binding  = 0;
		attributes[0].format   = vk::Format::eR32G32Sfloat;
		attributes[0].offset   = offsetof(Vertex, pos);

		attributes[1].location = 1;
		attributes[1].binding  = 0;
		attributes[1].format   = vk::Format::eR32G32B32Sfloat;
		attributes[1].offset   = offsetof(Vertex, color);

		return attributes;
	}
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};