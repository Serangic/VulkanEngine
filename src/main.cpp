#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <limits>
#include <fstream>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

// HELPER FILES:
#include "utils/helper.hpp"

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>

constexpr uint32_t WIDTH  = 1000;
constexpr uint32_t HEIGHT = 800;

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

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

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

class Application
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	GLFWwindow *window = nullptr;

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
	vk::raii::PipelineLayout 			pipelineLayout{nullptr};
	vk::raii::Pipeline 					graphicsPipeline{nullptr};
	std::vector<vk::raii::Framebuffer> swapChainFramebuffers;

	vk::raii::CommandPool commandPool 							= nullptr;
	std::vector<vk::raii::CommandBuffer> commandBuffers;

	std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Fence> inFlightFences;

	vk::raii::Buffer vertexBuffer 				= nullptr;
	vk::raii::DeviceMemory vertexBufferMemory 	= nullptr;
	vk::raii::Buffer       indexBuffer        = nullptr;
	vk::raii::DeviceMemory indexBufferMemory  = nullptr;

	const int MAX_FRAMES_IN_FLIGHT = 2;
	uint32_t currentFrame = 0;

	bool framebufferResized = false;

	std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName,vk::KHRSynchronization2ExtensionName};

	
	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "VulkanApplication", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    	app->framebufferResized = true;
	}

	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createIndexBuffer();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();
		}
		device.waitIdle();
	}

	void cleanup()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void createInstance()
	{
		vk::ApplicationInfo appInfo{};
		appInfo.pApplicationName   = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName        = "No Engine";
		appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion         = vk::ApiVersion12;

		// Get the required layers
		std::vector<char const *> requiredLayers;
		if (enableValidationLayers)
		{
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties    = context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
		                                               [&layerProperties](auto const &requiredLayer) {
			                                               return std::ranges::none_of(layerProperties,
			                                                                           [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		                                               });
		if (unsupportedLayerIt != requiredLayers.end())
		{
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
		}

		// Get the required extensions.
		auto requiredExtensions = Helper::getRequiredInstanceExtensions(enableValidationLayers);

		// Check if the required extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt =
		    std::ranges::find_if(requiredExtensions,
		                         [&extensionProperties](auto const &requiredExtension) {
			                         return std::ranges::none_of(extensionProperties,
			                                                     [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
		                         });
		if (unsupportedPropertyIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
		}

		vk::InstanceCreateInfo createInfo{};
		createInfo.pApplicationInfo        = &appInfo;
		createInfo.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size());
		createInfo.ppEnabledLayerNames     = requiredLayers.data();
		createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();
		instance = vk::raii::Instance(context, createInfo);
	}

	void setupDebugMessenger()
	{
		if (!enableValidationLayers)
			return;

		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		                                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{};
		debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
		debugUtilsMessengerCreateInfoEXT.messageType     = messageTypeFlags;
		debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &Helper::debugCallback;
		debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	}

	void createSurface() {
		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
		{
			throw std::runtime_error("WINDOWSURFACE_CREATION_ERR");
		}
		surface = vk::raii::SurfaceKHR(instance, _surface);
	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice)
	{
		// Check if the physicalDevice supports the Vulkan 1.2 API version
		bool supportsVulkan1_2 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_2;

		// Check if any of the queue families support graphics operations
		auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		// Check if all required physicalDevice extensions are available
		auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions =
		    std::ranges::all_of(requiredDeviceExtension,
		                        [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
			                        return std::ranges::any_of(availableDeviceExtensions,
			                                                   [requiredDeviceExtension](auto const &availableDeviceExtension) { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
		                        });

		// Return true if the physicalDevice meets all the criteria
		return supportsVulkan1_2 && supportsGraphics && supportsAllRequiredExtensions;
	}

	void pickPhysicalDevice()
	{
		std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
		auto const                            devIter         = std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) { return isDeviceSuitable(physicalDevice); });
		if (devIter == physicalDevices.end())
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
		physicalDevice = *devIter;
	}

	void createLogicalDevice()
	{
		// find the index of the first queue family that supports graphics
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports both graphics and present
		queueIndex = ~0;
		for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
		{
			if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			    physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
			{
				// found a queue family that supports both graphics and present
				queueIndex = qfpIndex;
				break;
			}
		}
		if (queueIndex == ~0)
		{
			throw std::runtime_error("Queue for graphics & presentation not found, terminating...");
		}

		// create a Device
		float                     queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};
		deviceQueueCreateInfo.queueFamilyIndex = queueIndex;
		deviceQueueCreateInfo.queueCount = 1;
		deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

		// enables the stupid draw parameters that broke my code for 1 day
		// and also because my gpu is older and needs extra configuring but is still good

		vk::PhysicalDeviceVulkan11Features features11{};
		features11.shaderDrawParameters = VK_TRUE; 

		vk::PhysicalDeviceSynchronization2Features sync2Features{};
    	sync2Features.synchronization2 = VK_TRUE;
    	sync2Features.pNext = &features11;


		vk::PhysicalDeviceFeatures2 features2{};
    	features2.pNext = &sync2Features;

		vk::DeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.pNext                   = &features2;
		deviceCreateInfo.queueCreateInfoCount    = 1;
		deviceCreateInfo.pQueueCreateInfos       = &deviceQueueCreateInfo;
		deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size());
		deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtension.data();

		device        	= vk::raii::Device(physicalDevice, deviceCreateInfo);
		queue 			= vk::raii::Queue(device, queueIndex, 0);
	}

	void createSwapChain()
	{
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
		swapChainExtent                                = Helper::chooseSwapExtent(surfaceCapabilities,window);
		uint32_t minImageCount                         = Helper::chooseSwapMinImageCount(surfaceCapabilities);

		std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
		swapChainSurfaceFormat                             = Helper::chooseSwapSurfaceFormat(availableFormats);

		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
		vk::PresentModeKHR              presentMode           = Helper::chooseSwapPresentMode(availablePresentModes);

		vk::SwapchainCreateInfoKHR swapChainCreateInfo{};
		swapChainCreateInfo.surface          = *surface;
		swapChainCreateInfo.minImageCount    = minImageCount;
		swapChainCreateInfo.imageFormat      = swapChainSurfaceFormat.format;
		swapChainCreateInfo.imageColorSpace  = swapChainSurfaceFormat.colorSpace;
		swapChainCreateInfo.imageExtent      = swapChainExtent;
		swapChainCreateInfo.imageArrayLayers = 1;
		swapChainCreateInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
		swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
		swapChainCreateInfo.preTransform     = surfaceCapabilities.currentTransform;
		swapChainCreateInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapChainCreateInfo.presentMode      = presentMode;
		swapChainCreateInfo.clipped          = true;

		swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
		swapChainImages = swapChain.getImages();
	}

	void createImageViews()
	{
		assert(swapChainImageViews.empty());

    	vk::ImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.viewType         = vk::ImageViewType::e2D;
        imageViewCreateInfo.format           = swapChainSurfaceFormat.format;
        imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

		for (auto &image : swapChainImages)
		{
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back(device, imageViewCreateInfo);
		}
	}
	// SOME FUNCTIONS IN CREATERENDERPASS HAVE BEEN REPLACED WITH THEIR 1 VERSION, AND NOT 2
	void createRenderPass()
	{
		vk::AttachmentDescription colorAttachmentDescription = {};                    	// Describe a framebuffer attachment (here: the color buffer)

		vk::Format swapchainFormat = swapChainSurfaceFormat.format;             		// Retrieve the image format used by the swapchain
    	
		colorAttachmentDescription.format = swapchainFormat;                   			// Set the attachment format to match the swapchain images
    	colorAttachmentDescription.samples = vk::SampleCountFlagBits::e1;      			// No multisampling (1 sample per pixel)
    	colorAttachmentDescription.loadOp = vk::AttachmentLoadOp::eClear;      			// Clear the attachment at the start of the render pass
    	colorAttachmentDescription.storeOp = vk::AttachmentStoreOp::eStore;    			// Store the rendered result so it can be presented
    	colorAttachmentDescription.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;  	// Stencil not used, ignore its initial contents
    	colorAttachmentDescription.stencilStoreOp = vk::AttachmentStoreOp::eDontCare; 	// Stencil not used, do not preserve it
    	colorAttachmentDescription.initialLayout = vk::ImageLayout::eUndefined;       	// Previous layout is irrelevant (contents will be cleared)
    	colorAttachmentDescription.finalLayout = vk::ImageLayout::ePresentSrcKHR;     	// Transition image to presentation layout after rendering

		vk::AttachmentReference colorAttachmentRef{};                           		// Reference describing how the attachment is used in a subpass
		colorAttachmentRef.attachment = 0;                                      		// Index of the attachment in the render pass (first/only one)
		colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;   		// Layout optimal for color rendering
		
		vk::SubpassDescription subpass{};                                       		// Define a subpass (a phase of rendering)
		subpass.pipelineBindPoint =  vk::PipelineBindPoint::eGraphics;           		// This subpass uses the graphics pipeline
		subpass.colorAttachmentCount = 1;                                       		// One color attachment is used
		subpass.pColorAttachments = &colorAttachmentRef;                        		// Attach the color attachment reference
		
		vk::RenderPassCreateInfo renderPassInfo{};                              		// Structure used to create the render pass
		renderPassInfo.sType = vk::StructureType::eRenderPassCreateInfo;         		// Explicitly set structure type (optional in C++ RAII but valid)
		renderPassInfo.attachmentCount = 1;                                     		// Number of attachments in the render pass
		renderPassInfo.pAttachments = &colorAttachmentDescription;              		// Pointer to attachment descriptions
		renderPassInfo.subpassCount = 1;                                        		// Number of subpasses
		renderPassInfo.pSubpasses = &subpass;                                   		// Pointer to subpass descriptions

		vk::SubpassDependency dependency{};                                     		// Define synchronization between subpasses (or external operations)
		dependency.srcSubpass = vk::SubpassExternal;                            		// Source is outside the render pass (previous operations)
		dependency.dstSubpass = 0;                                              		// Destination is this subpass (index 0)
		dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; 	// Wait for color attachment output stage before this subpass
		dependency.srcAccessMask = {};                                          		// No specific access mask required from source
		dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; 	// Synchronize at the same pipeline stage in destination
		dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;   		// Ensure writes to the color attachment are properly synchronized

		renderPassInfo.dependencyCount = 1;                                     		// One dependency defined
		renderPassInfo.pDependencies = &dependency;                             		// Pointer to dependency

		renderpass = vk::raii::RenderPass(device, renderPassInfo);              		// Create the render pass using RAII wrapper (auto-destroyed)

	}

	void createGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(Helper::readFile("shaders/slang.spv"));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex; 
		vertShaderStageInfo.module = shaderModule;
		vertShaderStageInfo.pName = "vertMain";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment; 
		fragShaderStageInfo.module = shaderModule; 
		fragShaderStageInfo.pName = "fragMain";

		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		auto bindingDescription 	= Vertex::getBindingDescription();
		auto attributeDescriptions 	= Vertex::getAttributeDescriptions();
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vertexInputInfo.vertexBindingDescriptionCount   = 1;
    	vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

		vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f};
		vk::Rect2D scissor{vk::Offset2D{ 0, 0 }, swapChainExtent};

		std::array dynamicStates = {
    		vk::DynamicState::eViewport,
    		vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		vk::PipelineViewportStateCreateInfo viewportState{};
		viewportState.pViewports = &viewport;
		viewportState.pScissors = &scissor;
		viewportState.viewportCount = 1;
		viewportState.scissorCount  = 1;		

		vk::PipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.depthClampEnable        = vk::False;
        rasterizer.rasterizerDiscardEnable = vk::False;
    	rasterizer.polygonMode             = vk::PolygonMode::eFill;
        rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace               = vk::FrontFace::eClockwise;
        rasterizer.depthBiasEnable         = vk::False;
        rasterizer.lineWidth               = 1.0f;

		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1; 
		multisampling.sampleShadingEnable = vk::False;

		vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    	colorBlendAttachment.blendEnable    = vk::False;
    	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

		vk::PipelineColorBlendStateCreateInfo colorBlending{};
    	colorBlending.logicOpEnable = vk::False;
		colorBlending.logicOp = vk::LogicOp::eCopy;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		// PIPELINE LAYOUT IS CREATED HERE. I know I could've made another function for it, but it doesn't change much to the actual story.

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 0;

		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		vk::GraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = vk::StructureType::eGraphicsPipelineCreateInfo;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState =  &dynamicState;
		pipelineInfo.layout = *pipelineLayout;
		pipelineInfo.renderPass = *renderpass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
	}

	void createFramebuffers() {
    swapChainFramebuffers.clear();
    swapChainFramebuffers.reserve(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vk::ImageView attachments[] = {
            *swapChainImageViews[i]
        };

        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = *renderpass; // note: dereference if RAII
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        swapChainFramebuffers.emplace_back(device, framebufferInfo);
    	}
	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{};
		poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = queueIndex;

		commandPool = vk::raii::CommandPool(device, poolInfo);
	}

	void createCommandBuffers()
	{
		vk::CommandBufferAllocateInfo allocInfo{};
			allocInfo.commandPool = commandPool; 												// What pool does it belong to.
			allocInfo.level = vk::CommandBufferLevel::ePrimary; 								// Can be submitted to a queue for execution, but cannot be called from other command buffers
			allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;					// How many

		vk::raii::CommandBuffers temp(device, allocInfo);

		commandBuffers.clear();
		commandBuffers.reserve(temp.size());

		for (size_t i = 0; i < temp.size(); ++i) {commandBuffers.emplace_back(std::move(temp[i]));}
		  
	}

	void recordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex)
	{
		// Begin command buffer
		vk::CommandBufferBeginInfo beginInfo{};
		commandBuffer.begin(beginInfo);

		// Render pass begin info
		vk::RenderPassBeginInfo renderPassInfo{};
		renderPassInfo.renderPass = *renderpass; // note: RAII handle unwrap
		renderPassInfo.framebuffer = *swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
		renderPassInfo.renderArea.extent = swapChainExtent;

		vk::ClearValue clearColor = vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f});
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		// Begin render pass
		commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

		// Bind pipeline
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		

		// Viewport
		vk::Viewport viewport{
    		0.0f,
    		0.0f,
    		static_cast<float>(swapChainExtent.width),
    		static_cast<float>(swapChainExtent.height),
   			0.0f,
    		1.0f
		};
		commandBuffer.setViewport(0, viewport);

		// Scissor
		vk::Rect2D scissor{{0, 0}, swapChainExtent};
		commandBuffer.setScissor(0, scissor);

		commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
		commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexTypeValue<decltype(indices)::value_type>::value);

		commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		// End render pass
		commandBuffer.endRenderPass();

		// End command buffer
		commandBuffer.end();

	}

	void drawFrame(){
		auto fenceResult = device.waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX); 			// Wait indefinitely until the GPU signals that the previous frame (associated with this fence) has finished execution
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!"); 												// If waiting fails, abort with an error
		}
																
		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *imageAvailableSemaphores[currentFrame], nullptr); 	// Acquire the next image from the swapchain, signaling a semaphore when it's ready

		if (result == vk::Result::eErrorOutOfDateKHR || framebufferResized)
		{
		framebufferResized = false;
		recreateSwapChain();
		return;
		}
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		{
		assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
		throw std::runtime_error("Failed to acquire swap chain image!");
		}

		device.resetFences(*inFlightFences[currentFrame]); 														// Reset the fence so it can be used again for the next frame
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex); 											// Record GPU commands into the command buffer for the acquired image

		queue.waitIdle();        																				// NOTE: for simplicity, wait for the queue to be idle before starting the frame
		                         																				// In the next chapter you see how to use multiple frames in flight and fences to sync

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput); 	// Specify the pipeline stage where the semaphore wait will occur (color attachment output stage)
		vk::SubmitInfo   submitInfo{}; 																			// Structure describing how command buffers are submitted to the queue
		submitInfo.waitSemaphoreCount   = 1; 																	// Number of semaphores to wait on before execution
		submitInfo.pWaitSemaphores      = &*imageAvailableSemaphores[currentFrame]; 							// Semaphore that signals when the image is ready for rendering
		submitInfo.pWaitDstStageMask    = &waitDestinationStageMask; 											// Pipeline stage at which to wait for the semaphore
		submitInfo.commandBufferCount   = 1; 																	// Number of command buffers to submit
		submitInfo.pCommandBuffers      = &*commandBuffers[currentFrame]; 														// Pointer to the command buffer to execute
		submitInfo.signalSemaphoreCount = 1; 																	// Number of semaphores to signal after execution
		submitInfo.pSignalSemaphores    = &*renderFinishedSemaphores[currentFrame]; 							// Semaphore to signal when rendering is finished
		queue.submit(submitInfo, *inFlightFences[currentFrame]); 												// Submit the command buffer to the GPU queue, associating it with a fence

		vk::PresentInfoKHR presentInfoKHR{}; 																	// Structure describing how the rendered image is presented to the screen
		presentInfoKHR.waitSemaphoreCount = 1; 																	// Number of semaphores to wait on before presentation
		presentInfoKHR.pWaitSemaphores = &*renderFinishedSemaphores[currentFrame]; 											// Wait until rendering is complete before presenting
		presentInfoKHR.swapchainCount = 1; 																		// Number of swapchains involved
		presentInfoKHR.pSwapchains = &*swapChain; 																// Pointer to the swapchain
		presentInfoKHR.pImageIndices = &imageIndex; 															// Index of the image in the swapchain to present

		result = queue.presentKHR(presentInfoKHR); 																// Present the rendered image to the screen
		switch (result)
		{
			case vk::Result::eSuccess:
				break; 																							// Presentation succeeded
			case vk::Result::eSuboptimalKHR:
				std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n"; 					// Swapchain is usable but not optimal (e.g., window resized)
				break;
			default:
				break;        																					// An unexpected result occurred (not explicitly handled)
		}
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void createSyncObjects()
	{
		vk::SemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = vk::StructureType::eSemaphoreCreateInfo;

		vk::FenceCreateInfo fenceInfo{};
		fenceInfo.sType = vk::StructureType::eFenceCreateInfo;
		fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

		imageAvailableSemaphores.clear();
		renderFinishedSemaphores.clear();
		inFlightFences.clear();

		imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    		imageAvailableSemaphores.emplace_back(device, semaphoreInfo);
    		renderFinishedSemaphores.emplace_back(device, semaphoreInfo);
    		inFlightFences.emplace_back(device, fenceInfo);
		}
	}

	void cleanupSwapChain()
	{
		swapChainImageViews.clear();
    	swapChain = nullptr;
	}

	void recreateSwapChain()
	{
	int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    device.waitIdle();

	cleanupSwapChain();

    createSwapChain();
    createImageViews();
	createFramebuffers();
	}	

	void createVertexBuffer()
	{
		vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();          	// Total size in bytes of the vertex data (size of one vertex × number of vertices)

		auto [stagingBuffer, stagingBufferMemory] =                                  	// Create a temporary (staging) buffer and its associated memory
			createBuffer(
				bufferSize,                                                          	// Size of the buffer
				vk::BufferUsageFlagBits::eTransferSrc,                               	// Buffer will be used as a source in a transfer operation
				vk::MemoryPropertyFlagBits::eHostVisible |                           	// Memory is accessible from the CPU
				vk::MemoryPropertyFlagBits::eHostCoherent                            	// CPU writes are automatically visible to the GPU (no manual flush needed)
			);

		void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);            	// Map the staging buffer memory so CPU can write into it
		memcpy(dataStaging, vertices.data(), bufferSize);                            	// Copy vertex data from CPU memory into the mapped staging buffer
		stagingBufferMemory.unmapMemory();                                           	// Unmap memory after writing (flush not needed due to HOST_COHERENT)

		std::tie(vertexBuffer, vertexBufferMemory) =                                 	// Create the final GPU vertex buffer and its memory
			createBuffer(
				bufferSize,                                                          	// Same size as staging buffer
				vk::BufferUsageFlagBits::eVertexBuffer |                             	// Buffer will be used as a vertex buffer in rendering
				vk::BufferUsageFlagBits::eTransferDst,                               	// Buffer will receive data from a transfer operation
				vk::MemoryPropertyFlagBits::eDeviceLocal                             	// Memory is device-local (fast GPU memory, not CPU accessible)
			);

		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);                         	// Copy data from staging buffer → device-local vertex buffer
	}

	void createIndexBuffer() {
		vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		auto [stagingBuffer, stagingBufferMemory] =
		    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		void *data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, indices.data(), (size_t) bufferSize);
		stagingBufferMemory.unmapMemory();

		std::tie(indexBuffer, indexBufferMemory) =
		    createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);
	}

//	Helper functions
	void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size)
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

	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
	{
		vk::BufferCreateInfo bufferInfo{};
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;

		vk::raii::Buffer       buffer          = vk::raii::Buffer(device, bufferInfo);
		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

		vk::MemoryAllocateInfo allocInfo{};
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
		buffer.bindMemory(*bufferMemory, 0);
		return {std::move(buffer), std::move(bufferMemory)};
	}

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
	{
		vk::ShaderModuleCreateInfo createInfo{}; 
		createInfo.codeSize = code.size() * sizeof(char); 
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		vk::raii::ShaderModule shaderModule{ device, createInfo };
		return shaderModule;
	}
};

int main()
{
	std::cout << "ValidationLayers: " << (enableValidationLayers ? "Enabled" : "Disabled") << "\n";
	try
	{
		Application app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}