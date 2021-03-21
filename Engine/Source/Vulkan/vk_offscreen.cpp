#include "Include/vk_offscreen.h"
#include "Include/vk_renderer.h"
#include "vk_pipelinebuilder.h"
#include "vk_utils.h"
#include <glm/gtx/matrix_decompose.hpp>

vkcomponent::PipelineBuilder offscreenBuilder;
FrameData frames[FRAME_OVERLAP];
FrameData& GetCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }

VkFormat depthFormat;



float cascadeSplitLambda = 0.95f;



namespace 
{
	
	void UploadLightData(const VmaAllocator& allocator, DepthPass& depthPass, const std::array<Cascade, SHADOW_MAP_CASCADE_COUNT>& cascades)
    {
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
			depthPass.ubo.cascadeViewProjMat[i] = cascades[i].viewProjMatrix;
		}
		UploadSingleData(allocator, depthPass.uboBuffer.allocation, depthPass.ubo);
		
    }
	void UploadDrawCalls(const VmaAllocator& allocator, const VmaAllocation& allocation, const std::vector<RenderObject>& objects)
    {
        std::vector<VkDrawIndirectCommand> commands;
        commands.reserve(objects.size());
        for (size_t i = 0; i < objects.size(); ++i)
        {
            VkDrawIndirectCommand cmd;
            cmd.vertexCount = objects[i].p_mesh->vertices.size();
            cmd.instanceCount = 1;
            cmd.firstVertex = 0;
            cmd.firstInstance = i;
            commands.push_back(cmd);
        }

		UploadVectorData(allocator, allocation, commands);
    }
	void UploadObjectData(const VmaAllocator& allocator, const VmaAllocation& allocation, const std::vector<RenderObject>& objects)
    {
        std::vector<GPUObjectData> data;
        data.reserve(objects.size());
        for (const RenderObject& obj : objects)
        {
            GPUObjectData objData;
            objData.modelMatrix = obj.transformMatrix;
            data.push_back(objData);
        }

		UploadVectorData(allocator, allocation, data);
    }
	std::vector<DrawCall> BatchDrawCalls(const std::vector<RenderObject>& objects, const DescriptorSetData& descriptorSets)
    {
        std::vector<DrawCall> batch;

		Mesh* pLastMesh = nullptr;
		Material* pLastMaterial = nullptr;

        for (size_t i = 0; i < objects.size(); ++i)
        {
			bool isSameMesh = objects[i].p_mesh == pLastMesh;
			bool isSameMaterial = objects[i].p_material == pLastMaterial;

			if (i == 0 || !isSameMesh || !isSameMaterial) {
                DrawCall dc;
                dc.pMesh = objects[i].p_mesh;
                dc.pMaterial = objects[i].p_material;
				dc.position = objects[i].position;
				
               	dc.descriptorSets = descriptorSets;
				
	
            	
                dc.index = i;
                dc.count = 1;
                batch.push_back(dc);

				pLastMesh = objects[i].p_mesh;
				pLastMaterial = objects[i].p_material;
			} else {
				++batch.back().count;
			}
        }

        return batch;
    }

	void IssueDrawCalls(const VkCommandBuffer& cmd, const VkBuffer& drawCommandBuffer, const std::vector<DrawCall>& drawCalls, const VkPipeline& pipeline, const VkPipelineLayout& layout, const VkDescriptorSet& currentDescriptor, uint32_t& index)
	{
		PushConstBlock pushConstBlock = {};
		for (const DrawCall& dc : drawCalls)
		{
		
			pushConstBlock.position = glm::vec4(glm::vec3(dc.position),0.0f);
			pushConstBlock.cascadeIndex = index;
			vkCmdPushConstants(cmd,layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);
			
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &currentDescriptor, 0, nullptr);
			

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			

			VkDeviceSize vertexOffset = 0;
        	vkCmdBindVertexBuffers(cmd, 0, 1, &dc.pMesh->vertexBuffer.buffer, &vertexOffset);

			uint32_t stride = sizeof(VkDrawIndirectCommand);
			uint32_t offset = dc.index * stride;

			vkCmdDrawIndirect(cmd, drawCommandBuffer, offset, dc.count, stride);
		}
	}
}

void VulkanOffscreen::InitOffscreen(VulkanRenderer& renderer)
{
   p_renderer = &renderer;

   InitRenderpass();
   InitFramebuffer();
   InitDescriptors();
   InitPipelines();
   BuildImage();

}

void VulkanOffscreen::InitRenderpass()
{
    depthFormat = vkinit::GetSupportedDepthFormat(true, p_renderer->GetPhysicalDevice());

	/*
		Depth map renderpass
	*/

	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = depthFormat;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &attachmentDescription;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassCreateInfo.pDependencies = dependencies.data();

	VK_CHECK(vkCreateRenderPass(p_renderer->GetDevice(), &renderPassCreateInfo, nullptr, &depthPass.renderPass));


	p_renderer->EnqueueCleanup([=]() {
		vkDestroyRenderPass(p_renderer->GetDevice(), depthPass.renderPass, nullptr);
	});
}


void VulkanOffscreen::InitFramebuffer()
{
    /*
		Layered depth image and views
	*/
	VkExtent3D shadowExtent3D = {};
	shadowExtent3D.width =depth.imageSize.width;
	shadowExtent3D.height = depth.imageSize.height;
	shadowExtent3D.depth = 1;
	VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,shadowExtent3D);
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = SHADOW_MAP_CASCADE_COUNT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	VK_CHECK(vmaCreateImage(p_renderer->GetAllocator(),&imageInfo, &dimg_allocinfo, &depth.depthImage.image.image, &depth.depthImage.image.allocation, nullptr));
	// Full depth map view (all layers)
	VkImageViewCreateInfo viewInfo = vkinit::ImageViewCreateInfo(depthFormat, depth.depthImage.image.image, VK_IMAGE_ASPECT_DEPTH_BIT);
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = SHADOW_MAP_CASCADE_COUNT;
	VK_CHECK(vkCreateImageView(p_renderer->GetDevice(), &viewInfo, nullptr, &depth.depthImage.imageView));

	// One image and framebuffer per cascade
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		// Image view for this cascade's layer (inside the depth map)
		// This view is used to render to that specific depth image layer
		VkImageViewCreateInfo viewInfo = vkinit::ImageViewCreateInfo(depthFormat, depth.depthImage.image.image, VK_IMAGE_ASPECT_DEPTH_BIT);
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = i;
		viewInfo.subresourceRange.layerCount = 1;
		VK_CHECK(vkCreateImageView(p_renderer->GetDevice(), &viewInfo, nullptr, &cascades[i].view));
		// Framebuffer
		VkFramebufferCreateInfo framebufferInfo = vkinit::FramebufferCreateInfo(depthPass.renderPass, depth.imageSize);
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &cascades[i].view;
		framebufferInfo.layers = 1;
		VK_CHECK(vkCreateFramebuffer(p_renderer->GetDevice(), &framebufferInfo, nullptr, &cascades[i].frameBuffer));
		p_renderer->EnqueueCleanup([=]() {
			cascades[i].destroy(p_renderer->GetDevice());
		});
	}

    p_renderer->EnqueueCleanup([=]() {
		vmaDestroyImage(p_renderer->GetAllocator(), depth.depthImage.image.image, depth.depthImage.image.allocation);
		vkDestroyImageView(p_renderer->GetDevice(), depth.depthImage.imageView, nullptr);
	});

}

void VulkanOffscreen::InitDescriptors()
{
	VkDescriptorSetLayoutBinding debugTexBind = vkinit::DescriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	
	std::vector<VkDescriptorSetLayoutBinding> bindings = {debugTexBind};
	VkDescriptorSetLayoutCreateInfo _set1 = vkinit::DescriptorLayoutInfo(bindings);
	debugSetLayout = p_renderer->GetDescriptorLayoutCache()->CreateDescriptorLayout(&_set1);

	VkDescriptorSetLayoutBinding depthBind = vkinit::DescriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0);
	VkDescriptorSetLayoutBinding depthImageBind = vkinit::DescriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,1);

	std::vector<VkDescriptorSetLayoutBinding> depthBindings = {depthBind};
	VkDescriptorSetLayoutCreateInfo _set2 = vkinit::DescriptorLayoutInfo(depthBindings);
	depthSetLayout = p_renderer->GetDescriptorLayoutCache()->CreateDescriptorLayout(&_set2);

	
	{
		CreateBufferInfo info;
		info.allocSize = sizeof(DepthPass::UniformBlock);
		info.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		info.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		CreateBuffer(p_renderer->GetAllocator(), &depthPass.uboBuffer, info);
	}
	VkDescriptorBufferInfo cascadeInfo = {};
	cascadeInfo.buffer = depthPass.uboBuffer.buffer;
	cascadeInfo.offset = 0;
	cascadeInfo.range = sizeof(DepthPass::UniformBlock);

	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
	 	p_renderer->GetDescriptorAllocator()->Allocate(&cascades[i].descriptorSet, depthSetLayout);
		vkcomponent::DescriptorBuilder::Begin(p_renderer->GetDescriptorLayoutCache(), p_renderer->GetDescriptorAllocator())
		.BindBuffer(0, &cascadeInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
		.Build(cascades[i].descriptorSet);
	}
		
	p_renderer->EnqueueCleanup([=]()
	{
		vmaDestroyBuffer(p_renderer->GetAllocator(), depthPass.uboBuffer.buffer, depthPass.uboBuffer.allocation);
	});
	
}

void VulkanOffscreen::InitPipelines()
{
    //VkShaderModule debugVertexShader;
	vkcomponent::ShaderModule debugVertShader;
	vkcomponent::LoadShaderModule(vkcomponent::CompileGLSL(".Shaders/debug.vert").c_str(), &debugVertShader, p_renderer->GetDevice());

	//VkShaderModule debugFragmentShader;
	vkcomponent::ShaderModule debugFragShader;
	vkcomponent::LoadShaderModule(vkcomponent::CompileGLSL(".Shaders/debug.frag").c_str(), &debugFragShader, p_renderer->GetDevice());

	vkcomponent::ShaderEffect* shadowDebugEffect = new vkcomponent::ShaderEffect();
	std::vector<vkcomponent::ShaderModule> shaderModules = {debugVertShader, debugFragShader};

    
	std::array<VkDescriptorSetLayout, 1> layouts= {debugSetLayout};
	VkPipelineLayoutCreateInfo debugpipInfo = vkinit::PipelineLayoutCreateInfo();
	VkPushConstantRange pushConstant;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(PushConstBlock);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	debugpipInfo.pSetLayouts = layouts.data();
	debugpipInfo.setLayoutCount = layouts.size();
	debugpipInfo.pushConstantRangeCount = 1;
	debugpipInfo.pPushConstantRanges = &pushConstant;
	shadowDebugEffect = vkcomponent::BuildEffect(p_renderer->GetDevice(), shaderModules, debugpipInfo);

	//hook the push constants layout
	offscreenBuilder.pipelineLayout = shadowDebugEffect->builtLayout;
	//we have copied layout to builder so now we can flush old one
	shadowDebugEffect->FlushLayout();

	//vertex input controls how to read vertices from vertex buffers. We arent using it yet
	offscreenBuilder.vertexInputInfo = vkinit::VertexInputStateCreateInfo();

	offscreenBuilder.inputAssembly = vkinit::InputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	//configure the rasterizer to draw filled triangles
	offscreenBuilder.rasterizer = vkinit::RasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT);

	offscreenBuilder.multisampling = vkinit::MultisamplingStateCreateInfo();
	
	offscreenBuilder.colorBlendAttachment = vkinit::ColorBlendAttachmentState();

	offscreenBuilder.depthStencil = vkinit::DepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	//build the debug pipeline
	vkcomponent::ShaderPass* debugPass = vkcomponent::BuildShader(p_renderer->GetDevice(), p_renderer->GetRenderPass(), offscreenBuilder, shadowDebugEffect);
	shadowDebug = debugPass->pipeline;
	shadowDebugLayout = debugPass->layout;


	vkcomponent::ShaderModule depthVertShader;
	vkcomponent::LoadShaderModule(vkcomponent::CompileGLSL(".Shaders/depthpass.vert").c_str(), & depthVertShader, p_renderer->GetDevice());
	

	vkcomponent::ShaderEffect* offscreenEffect = new vkcomponent::ShaderEffect();
	shaderModules.clear();
	shaderModules = {depthVertShader};

    
	std::array<VkDescriptorSetLayout, 1> offscreenLayouts= {depthSetLayout};
	VkPipelineLayoutCreateInfo offscreenpipInfo = vkinit::PipelineLayoutCreateInfo();
	offscreenpipInfo.pSetLayouts = offscreenLayouts.data();
	offscreenpipInfo.setLayoutCount = offscreenLayouts.size();
	offscreenpipInfo.pushConstantRangeCount = 1;
	offscreenpipInfo.pPushConstantRanges = &pushConstant;
	offscreenEffect = vkcomponent::BuildEffect(p_renderer->GetDevice(), shaderModules, offscreenpipInfo);

	std::vector <LocationInfo> locations = {{VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex, position)}
	};
	VertexInputDescription vertexDescription = Vertex::GetVertexDescription(locations);


	//connect the pipeline builder vertex input info to the one we get from Vertex
	offscreenBuilder.vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	offscreenBuilder.vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

	offscreenBuilder.vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	offscreenBuilder.vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

	offscreenBuilder.colorBlendAttachment = {};
	offscreenBuilder.rasterizer.cullMode = VK_CULL_MODE_NONE;

	offscreenBuilder.rasterizer.depthClampEnable = true;


	vkcomponent::ShaderPass* offscreenPass = vkcomponent::BuildShader(p_renderer->GetDevice(), depthPass.renderPass, offscreenBuilder, offscreenEffect);
	depthPass.pipeline = offscreenPass->pipeline;
	depthPass.pipelineLayout = offscreenPass->layout;


    p_renderer->EnqueueCleanup([=]()
    {
        debugPass->FlushPass(p_renderer->GetDevice());
		offscreenPass->FlushPass(p_renderer->GetDevice());
    });
	
}

void VulkanOffscreen::BuildImage()
{
	// Shared sampler for cascade depth reads
	VkSamplerCreateInfo sampler = vkinit::SamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(p_renderer->GetDevice(), &sampler, nullptr, &depth.sampler));

	VkDescriptorImageInfo imageBufferInfo = {};
	imageBufferInfo.sampler = depth.sampler;
	imageBufferInfo.imageView = depth.depthImage.imageView;
	imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	

	p_renderer->GetDescriptorAllocator()->Allocate(&depthSet, debugSetLayout);

	
	vkcomponent::DescriptorBuilder::Begin(p_renderer->GetDescriptorLayoutCache(), p_renderer->GetDescriptorAllocator())
	.BindImage(0, &imageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
	.Build(depthSet);

	 p_renderer->EnqueueCleanup([=]() {
		vkDestroySampler(p_renderer->GetDevice(), depth.sampler, nullptr);
	});
}

void VulkanOffscreen::BeginOffscreenRenderpass(const uint32_t& count)
{
	VkClearValue clearValues[1];
	clearValues[0].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vkinit::RenderpassBeginInfo(depthPass.renderPass, depth.imageSize, VK_NULL_HANDLE);
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues;

	VkViewport viewport = {};
	viewport.width = (float)depth.imageSize.width;
	viewport.height = (float)depth.imageSize.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(p_renderer->GetCommandBuffer(), 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset = {0,0};
	scissor.extent = depth.imageSize;
	vkCmdSetScissor(p_renderer->GetCommandBuffer(), 0, 1, &scissor);

	// One pass per cascade
	// The layer that this pass renders to is defined by the cascade's image view (selected via the cascade's descriptor set)	
	renderPassBeginInfo.framebuffer = cascades[count].frameBuffer;
	vkCmdBeginRenderPass(p_renderer->GetCommandBuffer(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	
}

void VulkanOffscreen::debugShadows(bool debug /*= false*/)
{
	if(debug == true)
	{
		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)p_renderer->GetWidth();
		viewport.height = (float)p_renderer->GetHeight();
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkExtent2D imageExtent;
		imageExtent.width = (float)p_renderer->GetWidth();
		imageExtent.height = (float)p_renderer->GetHeight();
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = imageExtent;

		vkCmdSetViewport(p_renderer->GetCommandBuffer(), 0, 1, &viewport);
		vkCmdSetScissor(p_renderer->GetCommandBuffer(), 0, 1, &scissor);
		vkCmdBindDescriptorSets(p_renderer->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, shadowDebugLayout, 0, 1, &depthSet, 0, nullptr);
		vkCmdBindPipeline(p_renderer->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, shadowDebug);
		PushConstBlock pushConstBlock = {};
		pushConstBlock.cascadeIndex = displayIndex;
		pushConstBlock.position = glm::vec4(0.0f);
		vkCmdPushConstants(p_renderer->GetCommandBuffer(), shadowDebugLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);
		vkCmdDraw(p_renderer->GetCommandBuffer(), 3, 1, 0, 0);
	}

}

void VulkanOffscreen::drawOffscreenShadows(const std::vector<RenderObject>& objects, uint32_t count)
{
	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)depth.imageSize.width;
	viewport.height = (float)depth.imageSize.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset = { 0, 0 };
	scissor.extent = depth.imageSize;

	vkCmdSetViewport(p_renderer->GetCommandBuffer(), 0, 1, &viewport);
	vkCmdSetScissor(p_renderer->GetCommandBuffer(), 0, 1, &scissor);
	DescriptorSetData descriptorSets;
	descriptorSets.cascadeSets = cascades[count].descriptorSet;

	UploadLightData(p_renderer->GetAllocator(), depthPass, cascades);
	UploadDrawCalls(p_renderer->GetAllocator(), p_renderer->GetCurrentFrame().indirectDrawBuffer.allocation, objects);
	std::vector<DrawCall> drawCalls = BatchDrawCalls(objects, descriptorSets);	
	IssueDrawCalls(p_renderer->GetCommandBuffer(), p_renderer->GetCurrentFrame().indirectDrawBuffer.buffer,drawCalls, depthPass.pipeline, depthPass.pipelineLayout,cascades[count].descriptorSet,count);
}

void VulkanOffscreen::EndOffscreenRenderpass()
{
	vkCmdEndRenderPass(p_renderer->GetCommandBuffer());
}

void VulkanOffscreen::calculateCascades(Camera& camera)
{
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = camera.zNear;
	float farClip = camera.zFar;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f,  1.0f, -1.0f),
			glm::vec3( 1.0f,  1.0f, -1.0f),
			glm::vec3( 1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3( 1.0f,  1.0f,  1.0f),
			glm::vec3( 1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(camera.GetProjectionMatrix(false) * camera.GetOffscreenViewMatrix());
		for (uint32_t i = 0; i < 8; i++) {
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
			frustumCorners[i] = invCorner / invCorner.w;
		}

		for (uint32_t i = 0; i < 4; i++) {
			glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
			frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
			frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t i = 0; i < 8; i++) {
			frustumCenter += frustumCorners[i];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t i = 0; i < 8; i++) {
			float distance = glm::length(frustumCorners[i] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::vec3 lightDir = normalize(-lightPos);
		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * minExtents.z, frustumCenter, glm::vec3(0.0f, -1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

		// Store split distance and matrix in cascade
		cascades[i].splitDepth = (nearClip + splitDist * clipRange) * -1.0f;
		cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
	
}

void VulkanOffscreen::updateLight(float dt)
{
	float angle = glm::radians(dt * 360.0f);
	float radius = 20.0f;
	lightPos = glm::vec3(cos(angle) * radius, -radius,sin(angle) * radius);
}