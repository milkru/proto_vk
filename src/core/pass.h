#pragma once

struct Viewport
{
	v2 offset{};
	v2 extent{};
};

struct Scissor
{
	iv2 offset{};
	uv2 extent{};
};

struct Attachment
{
	Texture texture{};
	VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	VkClearValue clear = { 0 };
};

typedef std::initializer_list<Attachment> Attachments;

union Binding
{
	Binding(Texture& _rTexture, VkImageLayout _layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	Binding(Buffer& _rBuffer);

	VkDescriptorImageInfo imageInfo;
	VkDescriptorBufferInfo bufferInfo;
};

typedef std::initializer_list<Binding> Bindings;

struct PushConstants
{
	u32 byteSize = 0;
	void* pData = nullptr;
};

struct PassDesc
{
	Pipeline pipeline{};                  // [Graphics/Compute] Pipeline to execute
	Viewport viewport{};                  // [Graphics] Screen viewport
	Scissor scissor{};                    // [Graphics] Screen scissor
	Attachments colorAttachments;         // [Graphics] Color render targets for graphics pipeline
	Attachment depthStencilAttachment{};  // [Graphics] Depth buffer for graphics pipeline
	Bindings bindings;                    // [Optional] Resource bindings
	PushConstants pushConstants;          // [Optional] Uniform data
};

void executePass(
	VkCommandBuffer _commandBuffer,
	PassDesc _desc,
	LAMBDA() _callback);
