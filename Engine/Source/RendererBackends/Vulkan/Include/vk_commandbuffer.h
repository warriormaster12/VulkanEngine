#pragma once 

#include "vk_types.h"
#include <functional>
#include <iostream>
#include <memory>

constexpr unsigned int FRAME_OVERLAP = 2;
static int frameNumber;

struct FrameData {
	VkSemaphore presentSemaphore, renderSemaphore;
	VkFence renderFence;

	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;
};
struct UploadContext {
	VkFence uploadFence;
	VkCommandPool commandPool;	
};

class VkCommandbufferManager
{
public:
    static void InitCommands();
    static void InitSyncStructures();
    static void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    static void CleanUpCommands();

    //these functions are used to start drawing loop
    static void BeginCommands(VkCommandBuffer& cmd, uint32_t& imageIndex, std::function<void()>&& recreateSwapchain);
    static void EndCommands(std::function<void()>&& recreateSwapchain);
    
private:
    inline static FrameData frames[FRAME_OVERLAP];
    inline static UploadContext uploadContext;

    static FrameData& GetCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }
};