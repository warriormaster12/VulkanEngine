#pragma once 

#include "vk_types.h"
#include "vk_utils.h"
#include "function_queuer.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>

constexpr unsigned int FRAME_OVERLAP = 2;

struct FrameData {
	VkSemaphore presentSemaphore, renderSemaphore;
	VkFence renderFence;

	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

    std::unordered_map<std::string, DescriptorSetInfo> descriptorSets;
    std::unordered_map<std::string, AllocatedBuffer> allocatedBuffer;
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
    static void BeginCommands(std::function<void()> recreateSwapchain);
    static void EndCommands(std::function<void()> recreateSwapchain);

    static FrameData& GetCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }
    static int& GetFrameNumber() {return frameNumber;}
    static FrameData& GetFrames(const int& index) {return frames[index];}

    static uint32_t& GetImageIndex(){return imageIndex;}
    static VkCommandBuffer& GetCommandBuffer() {return cmd;}
    
private:
    static inline FrameData frames[FRAME_OVERLAP];
    static inline UploadContext uploadContext;
    static inline VkCommandBuffer cmd;
    static inline uint32_t imageIndex;

    static inline int frameNumber;
};