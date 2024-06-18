#pragma once

struct Queue
{
	VkQueue queue = VK_NULL_HANDLE;
	u32 index = ~0u;
};

struct Device
{
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	Queue graphicsQueue{};
	VmaAllocator allocator = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
};

struct DeviceDesc
{
	bool bEnableValidationLayers = false;  // Enable Vulkan's validation layer
	bool bEnableSyncValidation = false;    // Enable synchronization validations
};

Device createDevice(
	GLFWwindow* _pWindow,
	DeviceDesc _desc);

void destroyDevice(
	_Out_ Device& _rDevice);

VkCommandBuffer createCommandBuffer(
	Device& _rDevice);

void immediateSubmit(
	Device& _rDevice,
	LAMBDA(VkCommandBuffer) _callback);
