#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#define DEF_FUNC(x) PFN_vk##x x

struct ow_vk_inst_funcs {
	DEF_FUNC(GetInstanceProcAddr);
	DEF_FUNC(DestroyInstance);
	DEF_FUNC(CreateWin32SurfaceKHR);
	DEF_FUNC(GetPhysicalDeviceMemoryProperties);
	DEF_FUNC(GetPhysicalDeviceImageFormatProperties2);
	DEF_FUNC(EnumeratePhysicalDevices);
	DEF_FUNC(GetPhysicalDeviceQueueFamilyProperties);
	DEF_FUNC(GetPhysicalDeviceFormatProperties);
};

struct ow_vk_device_funcs {
	DEF_FUNC(GetDeviceProcAddr);
	DEF_FUNC(DestroyDevice);
	DEF_FUNC(CreateSwapchainKHR);
	DEF_FUNC(DestroySwapchainKHR);
	DEF_FUNC(QueuePresentKHR);
	DEF_FUNC(AllocateMemory);
	DEF_FUNC(FreeMemory);
	DEF_FUNC(BindImageMemory);
	DEF_FUNC(BindImageMemory2);
	DEF_FUNC(GetSwapchainImagesKHR);
	DEF_FUNC(CreateImage);
	DEF_FUNC(DestroyImage);
	DEF_FUNC(GetImageMemoryRequirements);
	DEF_FUNC(GetImageMemoryRequirements2);
	DEF_FUNC(BeginCommandBuffer);
	DEF_FUNC(EndCommandBuffer);
	DEF_FUNC(CmdCopyImage);
	DEF_FUNC(CmdPipelineBarrier);
	DEF_FUNC(GetDeviceQueue);
	DEF_FUNC(QueueSubmit);
	DEF_FUNC(CreateCommandPool);
	DEF_FUNC(DestroyCommandPool);
	DEF_FUNC(AllocateCommandBuffers);
	DEF_FUNC(CreateFence);
	DEF_FUNC(DestroyFence);
	DEF_FUNC(WaitForFences);
	DEF_FUNC(ResetFences);
	DEF_FUNC(DestroyImageView);
	DEF_FUNC(DestroyFramebuffer);
	DEF_FUNC(DestroyRenderPass);
	DEF_FUNC(DestroyBuffer);
	DEF_FUNC(FreeCommandBuffers);
	DEF_FUNC(DestroySemaphore);
	DEF_FUNC(DestroyPipeline);
	DEF_FUNC(DestroyPipelineLayout);
	DEF_FUNC(FreeDescriptorSets);
	DEF_FUNC(DestroyDescriptorSetLayout);
	DEF_FUNC(DestroyDescriptorPool);
	DEF_FUNC(DestroySampler);
	DEF_FUNC(ResetCommandBuffer);
	DEF_FUNC(CmdSetScissor);
	DEF_FUNC(CmdDrawIndexed);
	DEF_FUNC(CmdEndRenderPass);
	DEF_FUNC(CreateRenderPass);
	DEF_FUNC(CreateImageView);
	DEF_FUNC(CreateFramebuffer);
	DEF_FUNC(CreateShaderModule);
	DEF_FUNC(CreateSampler);
	DEF_FUNC(CreateDescriptorPool);
	DEF_FUNC(AllocateDescriptorSets);
	DEF_FUNC(CreatePipelineLayout);
	DEF_FUNC(CreateGraphicsPipelines);
	DEF_FUNC(DestroyShaderModule);
	DEF_FUNC(UpdateDescriptorSets);
	DEF_FUNC(CreateDescriptorSetLayout);
	DEF_FUNC(CreateBuffer);
	DEF_FUNC(GetBufferMemoryRequirements);
	DEF_FUNC(BindBufferMemory);
	DEF_FUNC(CmdBeginRenderPass);
	DEF_FUNC(MapMemory);
	DEF_FUNC(UnmapMemory);
	DEF_FUNC(FlushMappedMemoryRanges);
	DEF_FUNC(CmdBindPipeline);
	DEF_FUNC(CmdBindDescriptorSets);
	DEF_FUNC(CmdBindVertexBuffers);
	DEF_FUNC(CmdBindIndexBuffer);
	DEF_FUNC(CreateSemaphore);
	DEF_FUNC(CmdSetViewport);
	DEF_FUNC(CmdPushConstants);
	DEF_FUNC(CmdCopyBufferToImage);
	DEF_FUNC(GetFenceStatus);
	DEF_FUNC(CmdBlitImage);
	DEF_FUNC(GetImageSubresourceLayout);
	DEF_FUNC(QueueWaitIdle);
	DEF_FUNC(DeviceWaitIdle);
};
#undef DEF_FUNC

struct ow_vk_device_data {
	struct ow_vk_device_funcs funcs;
	PFN_vkSetDeviceLoaderData set_device_loader_data;
	VkPhysicalDevice phy_device;
	VkDevice device;
	VkExternalMemoryProperties external_mem_props;
	struct ow_vk_queue_data *graphic_queue;
};

struct ow_vk_queue_data {
	VkQueue queue;
	VkDevice device;
	uint32_t family_index;
	VkQueueFlags flags;
	uint64_t timestamp_mask;
};

struct ow_vk_native_swapchain_data {
	VkSwapchainKHR swapchain;
	VkSurfaceKHR surface;
	VkFormat format;
	VkExtent2D extent; //width hight

	VkImageUsageFlags usage;
	HWND window_handle;
	uint32_t n_images;
};

