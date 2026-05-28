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
#include "utils/device.hpp"

#include "core/globalvar.hpp"

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

Vertex vertex;
UniformBufferObject uniformBufferObject;

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

	// var
	VKContext VKcxt;
	GLFWContext GLFWcxt;


	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		GLFWcxt.window = glfwCreateWindow(GLFWcxt.WIDTH, GLFWcxt.HEIGHT, "VulkanApplication", nullptr, nullptr);
		glfwSetWindowUserPointer(GLFWcxt.window, this);
		glfwSetFramebufferSizeCallback(GLFWcxt.window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    	app->VKcxt.framebufferResized = true;
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
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(GLFWcxt.window))
		{
			glfwPollEvents();
			drawFrame();
		}
		VKcxt.device.waitIdle();
	}

	void cleanup()
	{
		glfwDestroyWindow(GLFWcxt.window);
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
			requiredLayers.assign(VKcxt.validationLayers.begin(), VKcxt.validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties    = VKcxt.context.enumerateInstanceLayerProperties();
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
		auto extensionProperties = VKcxt.context.enumerateInstanceExtensionProperties();
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
		VKcxt.instance = vk::raii::Instance(VKcxt.context, createInfo);
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
		VKcxt.debugMessenger = VKcxt.instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	}

	void createSurface() {
		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*VKcxt.instance, GLFWcxt.window, nullptr, &_surface) != 0)
		{
			throw std::runtime_error("WINDOWSURFACE_CREATION_ERR");
		}
		VKcxt.surface = vk::raii::SurfaceKHR(VKcxt.instance, _surface);
	}

	void pickPhysicalDevice()
	{
		std::vector<vk::raii::PhysicalDevice> physicalDevices = VKcxt.instance.enumeratePhysicalDevices();
		auto const                            devIter         = std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) { return Helper::isDeviceSuitable(physicalDevice,VKcxt.requiredDeviceExtension); });
		if (devIter == physicalDevices.end())
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
		VKcxt.physicalDevice = *devIter;
	}

	void createLogicalDevice()
	{
		// find the index of the first queue family that supports graphics
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = VKcxt.physicalDevice.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports both graphics and present
		VKcxt.queueIndex = ~0;
		for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
		{
			if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			    VKcxt.physicalDevice.getSurfaceSupportKHR(qfpIndex, *VKcxt.surface))
			{
				// found a queue family that supports both graphics and present
				VKcxt.queueIndex = qfpIndex;
				break;
			}
		}
		if (VKcxt.queueIndex == ~0)
		{
			throw std::runtime_error("Queue for graphics & presentation not found, terminating...");
		}

		// create a Device
		float                     queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};
		deviceQueueCreateInfo.queueFamilyIndex = VKcxt.queueIndex;
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
		deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(VKcxt.requiredDeviceExtension.size());
		deviceCreateInfo.ppEnabledExtensionNames = VKcxt.requiredDeviceExtension.data();

		VKcxt.device        	= vk::raii::Device(VKcxt.physicalDevice, deviceCreateInfo);
		VKcxt.queue 			= vk::raii::Queue(VKcxt.device, VKcxt.queueIndex, 0);
	}

	void createSwapChain()
	{
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = VKcxt.physicalDevice.getSurfaceCapabilitiesKHR(*VKcxt.surface);
		VKcxt.swapChainExtent                                		= Helper::chooseSwapExtent(surfaceCapabilities,GLFWcxt.window);
		uint32_t minImageCount                         				= Helper::chooseSwapMinImageCount(surfaceCapabilities);

		std::vector<vk::SurfaceFormatKHR> availableFormats = VKcxt.physicalDevice.getSurfaceFormatsKHR(*VKcxt.surface);
		VKcxt.swapChainSurfaceFormat                            	= Helper::chooseSwapSurfaceFormat(availableFormats);

		std::vector<vk::PresentModeKHR> availablePresentModes 		= VKcxt.physicalDevice.getSurfacePresentModesKHR(*VKcxt.surface);
		vk::PresentModeKHR              presentMode           		= Helper::chooseSwapPresentMode(availablePresentModes);

		vk::SwapchainCreateInfoKHR swapChainCreateInfo{};
		swapChainCreateInfo.surface          = *VKcxt.surface;
		swapChainCreateInfo.minImageCount    = minImageCount;
		swapChainCreateInfo.imageFormat      = VKcxt.swapChainSurfaceFormat.format;
		swapChainCreateInfo.imageColorSpace  = VKcxt.swapChainSurfaceFormat.colorSpace;
		swapChainCreateInfo.imageExtent      = VKcxt.swapChainExtent;
		swapChainCreateInfo.imageArrayLayers = 1;
		swapChainCreateInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
		swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
		swapChainCreateInfo.preTransform     = surfaceCapabilities.currentTransform;
		swapChainCreateInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapChainCreateInfo.presentMode      = presentMode;
		swapChainCreateInfo.clipped          = true;

		VKcxt.swapChain       = vk::raii::SwapchainKHR(VKcxt.device, swapChainCreateInfo);
		VKcxt.swapChainImages = VKcxt.swapChain.getImages();
	}

	void createImageViews()
	{
		assert(VKcxt.swapChainImageViews.empty());

    	vk::ImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.viewType         = vk::ImageViewType::e2D;
        imageViewCreateInfo.format           = VKcxt.swapChainSurfaceFormat.format;
        imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

		for (auto &image : VKcxt.swapChainImages)
		{
			imageViewCreateInfo.image = image;
			VKcxt.swapChainImageViews.emplace_back(VKcxt.device, imageViewCreateInfo);
		}
	}
	// SOME FUNCTIONS IN CREATERENDERPASS HAVE BEEN REPLACED WITH THEIR 1 VERSION, AND NOT 2
	void createRenderPass()
	{
		vk::AttachmentDescription colorAttachmentDescription = {};                    	// Describe a framebuffer attachment (here: the color buffer)

		vk::Format swapchainFormat = VKcxt.swapChainSurfaceFormat.format;             		// Retrieve the image format used by the swapchain
    	
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

		VKcxt.renderpass = vk::raii::RenderPass(VKcxt.device, renderPassInfo);              		// Create the render pass using RAII wrapper (auto-destroyed)

	}

	void createGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = Device::createShaderModule(Helper::readFile("shaders/slang.spv"),VKcxt.device);

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

		vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(VKcxt.swapChainExtent.width), static_cast<float>(VKcxt.swapChainExtent.height), 0.0f, 1.0f};
		vk::Rect2D scissor{vk::Offset2D{ 0, 0 }, VKcxt.swapChainExtent};

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
        rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
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
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &*VKcxt.descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 0;

		VKcxt.pipelineLayout = vk::raii::PipelineLayout(VKcxt.device, pipelineLayoutInfo);

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
		pipelineInfo.layout = *VKcxt.pipelineLayout;
		pipelineInfo.renderPass = *VKcxt.renderpass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		VKcxt.graphicsPipeline = vk::raii::Pipeline(VKcxt.device, nullptr, pipelineInfo);
	}

	void createFramebuffers() {
    VKcxt.swapChainFramebuffers.clear();
    VKcxt.swapChainFramebuffers.reserve(VKcxt.swapChainImageViews.size());

    for (size_t i = 0; i < VKcxt.swapChainImageViews.size(); i++) {
        vk::ImageView attachments[] = {
            *VKcxt.swapChainImageViews[i]
        };

        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = *VKcxt.renderpass; // note: dereference if RAII
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = VKcxt.swapChainExtent.width;
        framebufferInfo.height = VKcxt.swapChainExtent.height;
        framebufferInfo.layers = 1;

        VKcxt.swapChainFramebuffers.emplace_back(VKcxt.device, framebufferInfo);
    	}
	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{};
		poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = VKcxt.queueIndex;

		VKcxt.commandPool = vk::raii::CommandPool(VKcxt.device, poolInfo);
	}

	void createCommandBuffers()
	{
		vk::CommandBufferAllocateInfo allocInfo{};
			allocInfo.commandPool = VKcxt.commandPool; 												// What pool does it belong to.
			allocInfo.level = vk::CommandBufferLevel::ePrimary; 								// Can be submitted to a queue for execution, but cannot be called from other command buffers
			allocInfo.commandBufferCount = VKcxt.MAX_FRAMES_IN_FLIGHT;					// How many

		vk::raii::CommandBuffers temp(VKcxt.device, allocInfo);

		VKcxt.commandBuffers.clear();
		VKcxt.commandBuffers.reserve(temp.size());

		for (size_t i = 0; i < temp.size(); ++i) {VKcxt.commandBuffers.emplace_back(std::move(temp[i]));}
		  
	}

	void recordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex)
	{
		// Begin command buffer
		vk::CommandBufferBeginInfo beginInfo{};
		commandBuffer.begin(beginInfo);

		// Render pass begin info
		vk::RenderPassBeginInfo renderPassInfo{};
		renderPassInfo.renderPass = *VKcxt.renderpass; // note: RAII handle unwrap
		renderPassInfo.framebuffer = *VKcxt.swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
		renderPassInfo.renderArea.extent = VKcxt.swapChainExtent;

		vk::ClearValue clearColor = vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f});
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		// Begin render pass
		commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

		// Bind pipeline
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *VKcxt.graphicsPipeline);
		

		// Viewport
		vk::Viewport viewport{
    		0.0f,
    		0.0f,
    		static_cast<float>(VKcxt.swapChainExtent.width),
    		static_cast<float>(VKcxt.swapChainExtent.height),
   			0.0f,
    		1.0f
		};
		commandBuffer.setViewport(0, viewport);

		// Scissor
		vk::Rect2D scissor{{0, 0}, VKcxt.swapChainExtent};
		commandBuffer.setScissor(0, scissor);

		commandBuffer.bindVertexBuffers(0, *VKcxt.vertexBuffer, {0});
		commandBuffer.bindIndexBuffer(*VKcxt.indexBuffer, 0, vk::IndexTypeValue<decltype(indices)::value_type>::value);

		VKcxt.commandBuffers[VKcxt.currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, VKcxt.pipelineLayout, 0, *VKcxt.descriptorSets[VKcxt.currentFrame], nullptr);
		commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		// End render pass
		commandBuffer.endRenderPass();

		// End command buffer
		commandBuffer.end();

	}

	void drawFrame(){
		auto fenceResult = VKcxt.device.waitForFences(*VKcxt.inFlightFences[VKcxt.currentFrame], vk::True, UINT64_MAX); 			// Wait indefinitely until the GPU signals that the previous frame (associated with this fence) has finished execution
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!"); 												// If waiting fails, abort with an error
		}
																
		auto [result, imageIndex] = VKcxt.swapChain.acquireNextImage(UINT64_MAX, *VKcxt.imageAvailableSemaphores[VKcxt.currentFrame], nullptr); 	// Acquire the next image from the swapchain, signaling a semaphore when it's ready

		if (result == vk::Result::eErrorOutOfDateKHR || VKcxt.framebufferResized)
		{
		VKcxt.framebufferResized = false;
		recreateSwapChain();
		return;
		}
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		{
		assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
		throw std::runtime_error("Failed to acquire swap chain image!");
		}
		
		updateUniformBuffer(VKcxt.currentFrame);

		VKcxt.device.resetFences(*VKcxt.inFlightFences[VKcxt.currentFrame]); 														// Reset the fence so it can be used again for the next frame
		recordCommandBuffer(VKcxt.commandBuffers[VKcxt.currentFrame], imageIndex); 											// Record GPU commands into the command buffer for the acquired image

		VKcxt.queue.waitIdle();        																				// NOTE: for simplicity, wait for the queue to be idle before starting the frame
		                         																				// In the next chapter you see how to use multiple frames in flight and fences to sync
		

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput); 	// Specify the pipeline stage where the semaphore wait will occur (color attachment output stage)
		vk::SubmitInfo   submitInfo{}; 																			// Structure describing how command buffers are submitted to the queue
		submitInfo.waitSemaphoreCount   = 1; 																	// Number of semaphores to wait on before execution
		submitInfo.pWaitSemaphores      = &*VKcxt.imageAvailableSemaphores[VKcxt.currentFrame]; 							// Semaphore that signals when the image is ready for rendering
		submitInfo.pWaitDstStageMask    = &waitDestinationStageMask; 											// Pipeline stage at which to wait for the semaphore
		submitInfo.commandBufferCount   = 1; 																	// Number of command buffers to submit
		submitInfo.pCommandBuffers      = &*VKcxt.commandBuffers[VKcxt.currentFrame]; 														// Pointer to the command buffer to execute
		submitInfo.signalSemaphoreCount = 1; 																	// Number of semaphores to signal after execution
		submitInfo.pSignalSemaphores    = &*VKcxt.renderFinishedSemaphores[VKcxt.currentFrame]; 							// Semaphore to signal when rendering is finished
		VKcxt.queue.submit(submitInfo, *VKcxt.inFlightFences[VKcxt.currentFrame]); 												// Submit the command buffer to the GPU queue, associating it with a fence

		vk::PresentInfoKHR presentInfoKHR{}; 																	// Structure describing how the rendered image is presented to the screen
		presentInfoKHR.waitSemaphoreCount = 1; 																	// Number of semaphores to wait on before presentation
		presentInfoKHR.pWaitSemaphores = &*VKcxt.renderFinishedSemaphores[VKcxt.currentFrame]; 											// Wait until rendering is complete before presenting
		presentInfoKHR.swapchainCount = 1; 																		// Number of swapchains involved
		presentInfoKHR.pSwapchains = &*VKcxt.swapChain; 																// Pointer to the swapchain
		presentInfoKHR.pImageIndices = &imageIndex; 															// Index of the image in the swapchain to present

		result = VKcxt.queue.presentKHR(presentInfoKHR); 																// Present the rendered image to the screen
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
		VKcxt.currentFrame = (VKcxt.currentFrame + 1) % VKcxt.MAX_FRAMES_IN_FLIGHT;
	}

	void createSyncObjects()
	{
		vk::SemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = vk::StructureType::eSemaphoreCreateInfo;

		vk::FenceCreateInfo fenceInfo{};
		fenceInfo.sType = vk::StructureType::eFenceCreateInfo;
		fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

		VKcxt.imageAvailableSemaphores.clear();
		VKcxt.renderFinishedSemaphores.clear();
		VKcxt.inFlightFences.clear();

		VKcxt.imageAvailableSemaphores.reserve(VKcxt.MAX_FRAMES_IN_FLIGHT);
		VKcxt.renderFinishedSemaphores.reserve(VKcxt.MAX_FRAMES_IN_FLIGHT);
		VKcxt.inFlightFences.reserve(VKcxt.MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < VKcxt.MAX_FRAMES_IN_FLIGHT; i++) {
    		VKcxt.imageAvailableSemaphores.emplace_back(VKcxt.device, semaphoreInfo);
    		VKcxt.renderFinishedSemaphores.emplace_back(VKcxt.device, semaphoreInfo);
    		VKcxt.inFlightFences.emplace_back(VKcxt.device, fenceInfo);
		}
	}

	void cleanupSwapChain()
	{
		VKcxt.swapChainImageViews.clear();
    	VKcxt.swapChain = nullptr;
	}

	void recreateSwapChain()
	{
	int width = 0, height = 0;
    glfwGetFramebufferSize(GLFWcxt.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(GLFWcxt.window, &width, &height);
        glfwWaitEvents();
    }

    VKcxt.device.waitIdle();

	cleanupSwapChain();

    createSwapChain();
    createImageViews();
	createFramebuffers();
	}	

	void createVertexBuffer()
	{
		vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();          	// Total size in bytes of the vertex data (size of one vertex × number of vertices)

		auto [stagingBuffer, stagingBufferMemory] =                                  	// Create a temporary (staging) buffer and its associated memory
			Device::createBuffer(
				bufferSize,                                                          	// Size of the buffer
				vk::BufferUsageFlagBits::eTransferSrc,                               	// Buffer will be used as a source in a transfer operation
				vk::MemoryPropertyFlagBits::eHostVisible |                           	// Memory is accessible from the CPU
				vk::MemoryPropertyFlagBits::eHostCoherent,                            	// CPU writes are automatically visible to the GPU (no manual flush needed)
				VKcxt.device,
				VKcxt.physicalDevice
			);

		void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);            	// Map the staging buffer memory so CPU can write into it
		memcpy(dataStaging, vertices.data(), bufferSize);                            	// Copy vertex data from CPU memory into the mapped staging buffer
		stagingBufferMemory.unmapMemory();                                           	// Unmap memory after writing (flush not needed due to HOST_COHERENT)

		std::tie(VKcxt.vertexBuffer, VKcxt.vertexBufferMemory) =                                 	// Create the final GPU vertex buffer and its memory
			Device::createBuffer(
				bufferSize,                                                          	// Same size as staging buffer
				vk::BufferUsageFlagBits::eVertexBuffer |                             	// Buffer will be used as a vertex buffer in rendering
				vk::BufferUsageFlagBits::eTransferDst,                               	// Buffer will receive data from a transfer operation
				vk::MemoryPropertyFlagBits::eDeviceLocal,                             	// Memory is device-local (fast GPU memory, not CPU accessible)
				VKcxt.device,
				VKcxt.physicalDevice
			);

		Device::copyBuffer(stagingBuffer, VKcxt.vertexBuffer, bufferSize,VKcxt.commandPool,VKcxt.device,VKcxt.queue);                         	// Copy data from staging buffer → device-local vertex buffer
	}

	void createIndexBuffer() {
		vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		auto [stagingBuffer, stagingBufferMemory] =
		    Device::createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,VKcxt.device,VKcxt.physicalDevice);

		void *data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, indices.data(), (size_t) bufferSize);
		stagingBufferMemory.unmapMemory();

		std::tie(VKcxt.indexBuffer, VKcxt.indexBufferMemory) =
		    Device::createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal,VKcxt.device,VKcxt.physicalDevice);

		Device::copyBuffer(stagingBuffer, VKcxt.indexBuffer, bufferSize,VKcxt.commandPool,VKcxt.device,VKcxt.queue);
	}

	void createDescriptorSetLayout()
	{
		vk::DescriptorSetLayoutBinding uboLayoutBinding{};
		    uboLayoutBinding.binding = 0;
			uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboLayoutBinding.descriptorCount = 1;
			uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

			vk::DescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.bindingCount = 1; 
			layoutInfo.pBindings = &uboLayoutBinding;
			VKcxt.descriptorSetLayout = vk::raii::DescriptorSetLayout(VKcxt.device, layoutInfo);
	}

	void createUniformBuffers()
	{
		for (size_t i = 0; i < VKcxt.MAX_FRAMES_IN_FLIGHT; i++)
    		{
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
        auto [buffer, bufferMem]  = Device::createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,VKcxt.device,VKcxt.physicalDevice);
        VKcxt.uniformBuffers.emplace_back(std::move(buffer));
        VKcxt.uniformBuffersMemory.emplace_back(std::move(bufferMem));
       	VKcxt.uniformBuffersMapped.emplace_back(VKcxt.uniformBuffersMemory.back().mapMemory(0, bufferSize));
    		}
	}

	void updateUniformBuffer(uint32_t currentImage)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

    	auto currentTime = std::chrono::high_resolution_clock::now();
    	float time       = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(VKcxt.swapChainExtent.width) / static_cast<float>(VKcxt.swapChainExtent.height), 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		memcpy(VKcxt.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void createDescriptorPool()
	{
		vk::DescriptorPoolSize poolSize{};
		poolSize.type = vk::DescriptorType::eUniformBuffer;
		poolSize.descriptorCount = VKcxt.MAX_FRAMES_IN_FLIGHT;

		vk::DescriptorPoolCreateInfo poolInfo{};
		poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
		poolInfo.maxSets = VKcxt.MAX_FRAMES_IN_FLIGHT;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;

		VKcxt.descriptorPool = vk::raii::DescriptorPool(VKcxt.device, poolInfo);
	}

	void createDescriptorSets()
	{
		std::vector<vk::DescriptorSetLayout> layouts(VKcxt.MAX_FRAMES_IN_FLIGHT, *VKcxt.descriptorSetLayout);
		vk::DescriptorSetAllocateInfo        allocInfo{};
		allocInfo.descriptorPool     = VKcxt.descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts        = layouts.data();

		VKcxt.descriptorSets = VKcxt.device.allocateDescriptorSets(allocInfo);

		for (size_t i = 0; i < VKcxt.MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = VKcxt.uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			vk::WriteDescriptorSet   descriptorWrite{};
			descriptorWrite.dstSet          = VKcxt.descriptorSets[i];
            descriptorWrite.dstBinding      = 0;
            descriptorWrite.dstArrayElement = 0;
        	descriptorWrite.descriptorCount = 1;
            descriptorWrite.descriptorType  = vk::DescriptorType::eUniformBuffer;
            descriptorWrite.pBufferInfo     = &bufferInfo;

			VKcxt.device.updateDescriptorSets(descriptorWrite, {});
		}
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