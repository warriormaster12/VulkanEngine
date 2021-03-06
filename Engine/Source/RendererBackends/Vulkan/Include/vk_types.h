#pragma once

#define VK_NO_PROTOTYPES
#include "volk.h"
#include <vector>
#include "vk_mem_alloc.h"


struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    VkBufferCreateFlags bufferUsage;
    size_t dataSize;
    size_t dataRange;
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation;
	VkImageView defaultView = VK_NULL_HANDLE;
	int mipLevels;
};



