#include "core/device.h"
#include "core/buffer.h"
#include "core/texture.h"
#include "core/shader.h"
#include "core/frame_pacing.h"
#include "core/swapchain.h"
#include "core/pipeline.h"
#include "core/pass.h"
#include "core/query.h"

#include "window.h"
#include "camera.h"
#include "shaders/shader_interop.h"
#include "geometry.h"
#include "draw.h"
#include "gui.h"
#include "gpu_profiler.h"
#include "utils.h"

#include <string.h>
#include <chrono>

#ifdef DEBUG_
const bool kbEnableValidationLayers = true;
#else
const bool kbEnableValidationLayers = false;
#endif // DEBUG_

const bool kbEnableMeshShadingPipeline = true;

const u32 kPreferredSwapchainImageCount = 2;
const bool kbEnableVSync = false;

const u32 kWindowWidth = 2048;
const u32 kWindowHeight = 1080;

const u32 kMaxDrawCount = 1'000'000;
const f32 kSpawnCubeSize = 150.0f;

struct alignas(16) PerPassData
{
	int32_t bPrepass = 0;
};

static Texture createDepthTexture(
	Device& _rDevice,
	u32 _width,
	u32 _height)
{
	return createTexture(_rDevice, {
		.width = _width,
		.height = _height,
		.format = VK_FORMAT_D32_SFLOAT,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.sampler = {
			.filterMode = VK_FILTER_LINEAR,
			.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN } });
}

static Texture createHzbTexture(
	Device& _rDevice,
	u32 _size)
{
	return createTexture(_rDevice, {
		.width = _size,
		.height = _size,
		.mipCount = u32(glm::log2(f32(_size))) + 1,
		.format = VK_FORMAT_R16_SFLOAT,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.access = VK_ACCESS_SHADER_READ_BIT,
		.sampler = {
			.filterMode = VK_FILTER_LINEAR,
			.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN } });
}

i32 main(
	i32 _argc,
	const char** _argv)
{
	EASY_MAIN_THREAD;
	EASY_PROFILER_ENABLE;

	const char** meshPaths = _argv + 1;
	u32 meshCount = u32(_argc - 1);
	if (meshCount == 0)
	{
		printf("Provide mesh paths as command arguments.\n");
		return EXIT_FAILURE;
	}

	GLFWwindow* pWindow = createWindow({
		.width = kWindowWidth,
		.height = kWindowHeight,
		.title = "vulkanizer" });

	Device device = createDevice(pWindow, {
		.bEnableValidationLayers = kbEnableValidationLayers,
		.bEnableMeshShadingPipeline = kbEnableMeshShadingPipeline });

	Swapchain swapchain{};

	Texture depthTexture{};

	u32 hzbSize = 0;
	Texture hzb{};
	std::vector<Texture> hzbMips;

	auto initializeSwapchainResources = [&]()
	{
		EASY_BLOCK("InitializeSwapchainResources");

		{
			Swapchain oldSwapchain = swapchain;

			swapchain = createSwapchain(pWindow, device, {
				.bEnableVSync = kbEnableVSync,
				.preferredSwapchainImageCount = kPreferredSwapchainImageCount,
				.oldSwapchain = oldSwapchain.swapchain });

			if (oldSwapchain.swapchain != VK_NULL_HANDLE)
			{
				destroySwapchain(device, oldSwapchain);
			}
		}

		{
			if (depthTexture.resource != VK_NULL_HANDLE)
			{
				destroyTexture(device, depthTexture);
			}

			depthTexture = createDepthTexture(device, swapchain.extent.width, swapchain.extent.height);
		}

		{
			if (hzb.resource != VK_NULL_HANDLE)
			{
				destroyTexture(device, hzb);
			}

			hzbSize = roundUpToPowerOfTwo(glm::max(swapchain.extent.width, swapchain.extent.height));
			hzb = createHzbTexture(device, hzbSize);

			for (Texture& rHzbMip : hzbMips)
			{
				if (rHzbMip.resource != VK_NULL_HANDLE)
				{
					destroyTextureView(device, rHzbMip);
				}
			}

			hzbMips.clear();
			hzbMips.reserve(hzb.mipCount);

			for (u32 mipIndex = 0; mipIndex < hzb.mipCount; ++mipIndex)
			{
				hzbMips.push_back(createTextureView(device, {
					.mipIndex = mipIndex,
					.mipCount = 1,
					.sampler = {
						.filterMode = VK_FILTER_LINEAR,
						.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN },
					.pParent = &hzb }));
			}
		}
	};

	initializeSwapchainResources();

	Camera camera = {
		.fov = 45.0f,
		.aspect = f32(swapchain.extent.width) / f32(swapchain.extent.height),
		.near = 0.01f,
		.moveSpeed = 1.0f,
		.boostMoveSpeed = 3.0f,
		.sensitivity = 100.0f };

	Shader generateDrawsShader = createShader(device, {
		.pPath = "shaders/generate_draws.comp.spv",
		.pEntry = "main" });

	Shader taskShader = device.bMeshShadingPipelineAllowed ?
		createShader(device, {
			.pPath = "shaders/geometry.task.spv",
			.pEntry = "main" }) : Shader();

	Shader meshShader = device.bMeshShadingPipelineAllowed ?
		createShader(device, {
			.pPath = "shaders/geometry.mesh.spv",
			.pEntry = "main" }) : Shader();

	Shader vertShader = createShader(device, {
		.pPath = "shaders/geometry.vert.spv",
		.pEntry = "main" });

	Shader fragShader = createShader(device, {
		.pPath = "shaders/color.frag.spv",
		.pEntry = "main" });

	Shader hzbDownsampleShader = createShader(device, {
		.pPath = "shaders/hzb_downsample.comp.spv",
		.pEntry = "main" });

	Pipeline generateDrawsPipeline = createComputePipeline(device, generateDrawsShader);

	Pipeline geometryPipeline = createGraphicsPipeline(device, {
		.shaders = { vertShader, fragShader },
		.attachmentLayout = {
			.colorAttachments = { {
				.format = swapchain.format,
				.bBlendEnable = true } },
			.depthStencilFormat = { depthTexture.format }},
		.rasterization = {
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE },
		.depthStencil = {
			.bDepthTestEnable = true,
			.bDepthWriteEnable = true,
			.depthCompareOp = VK_COMPARE_OP_GREATER } });

	Pipeline geometryMeshletPipeline = device.bMeshShadingPipelineAllowed ?
		createGraphicsPipeline(device, {
			.shaders = { taskShader, meshShader, fragShader },
			.attachmentLayout = {
				.colorAttachments = { {
					.format = swapchain.format,
					.bBlendEnable = true } },
				.depthStencilFormat = { depthTexture.format }},
			.rasterization = {
				.cullMode = VK_CULL_MODE_BACK_BIT,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE },
			.depthStencil = {
				.bDepthTestEnable = true,
				.bDepthWriteEnable = true,
				.depthCompareOp = VK_COMPARE_OP_GREATER } }) : Pipeline();

	Pipeline hzbDownsamplePipeline = createComputePipeline(device, hzbDownsampleShader);

	destroyShader(device, generateDrawsShader);

	if (device.bMeshShadingPipelineAllowed)
	{
		destroyShader(device, meshShader);
		destroyShader(device, taskShader);
	}

	destroyShader(device, fragShader);
	destroyShader(device, vertShader);
	destroyShader(device, hzbDownsampleShader);

	GeometryBuffers geometryBuffers;
	DrawBuffers drawBuffers;
	{
		Geometry geometry{};

		for (u32 meshIndex = 0; meshIndex < meshCount; ++meshIndex)
		{
			const char* meshPath = meshPaths[meshIndex];
			loadMesh(geometry, meshPath, device.bMeshShadingPipelineAllowed);
		}

		geometryBuffers = createGeometryBuffers(device, geometry);
		drawBuffers = createDrawBuffers(device, geometry, kMaxDrawCount, kSpawnCubeSize);
	}

	std::array<VkCommandBuffer, kMaxFramesInFlightCount> commandBuffers;
	for (VkCommandBuffer& rCommandBuffer : commandBuffers)
	{
		rCommandBuffer = createCommandBuffer(device);
	}

	std::array<FramePacingState, kMaxFramesInFlightCount> framePacingStates;
	for (FramePacingState& rFramePacingState : framePacingStates)
	{
		rFramePacingState = createFramePacingState(device);
	}

	gpu::profiler::initialize(device);

	struct alignas(16)
	{
		m4 view;
		m4 freezeView;
		m4 projection;
		v4 freezeFrustumPlanes[kFrustumPlaneCount];
		v4 cameraPosition;
		v4 freezeCameraPosition;
		u32 maxDrawCount;
		f32 lodTransitionBase;
		f32 lodTransitionStep;
		i32 forcedLod;
		u32 hzbSize;
		i32 bMeshShadingPipelineEnabled;
		i32 bMeshFrustumCullingEnabled;
		i32 bMeshOcclusionCullingEnabled;
		i32 bMeshletConeCullingEnabled;
		i32 bMeshletFrustumCullingEnabled;
		i32 bMeshletOcclusionCullingEnabled;
	} perFrameData = {};

	std::array<Buffer, kMaxFramesInFlightCount> perFrameDataBuffers;
	for (Buffer& rPerFrameDataBuffer : perFrameDataBuffers)
	{
		rPerFrameDataBuffer = createBuffer(device, {
			.byteSize = sizeof(perFrameData),
			.access = MemoryAccess::Host,
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT });
	}

	gui::initialize(device, swapchain.format, depthTexture.format, (f32)kWindowWidth, (f32)kWindowHeight);

	Settings settings = {
		.bEnableMeshFrustumCulling = true,
		.bEnableMeshOcclusionCulling = true };

	bool bMeshShadingPipelineEnabled =
		settings.bEnableMeshletConeCulling =
		settings.bEnableMeshletFrustumCulling =
		settings.bEnableMeshletOcclusionCulling =
		settings.bEnableMeshShadingPipeline =
		settings.bMeshShadingPipelineSupported =
		device.bMeshShadingPipelineAllowed;

	u32 frameIndex = 0;

	auto generateDrawsPass = [&](
		VkCommandBuffer _commandBuffer,
		bool _bPrepass)
	{
		PerPassData perPassData = { .bPrepass = _bPrepass ? 1 : 0 };

		executePass(_commandBuffer, {
			.pipeline = generateDrawsPipeline,
			.bindings = {
				Binding(perFrameDataBuffers[frameIndex]),
				Binding(geometryBuffers.meshesBuffer),
				Binding(drawBuffers.drawsBuffer),
				Binding(drawBuffers.drawCommandsBuffer),
				Binding(drawBuffers.drawCountBuffer),
				Binding(drawBuffers.meshVisibilityBuffer),
				Binding(hzb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) },
			.pushConstants = {
				.byteSize = sizeof(PerPassData),
				.pData = &perPassData } },
				[&]()
			{
				i32 groupCount = ceil(f32(kMaxDrawCount) / kShaderGroupSizeNV);
				vkCmdDispatch(_commandBuffer, groupCount, 1, 1);
			});
	};

	auto geometryPass = [&](
		VkCommandBuffer _commandBuffer,
		u32 _currentSwapchainImageIndex,
		bool _bPrepass)
	{
		PerPassData perPassData = { .bPrepass = _bPrepass ? 1 : 0 };

		executePass(_commandBuffer, {
			.pipeline = bMeshShadingPipelineEnabled ? geometryMeshletPipeline : geometryPipeline,
			.viewport = {
				.offset = { 0.0f, 0.0f },
				.extent = { swapchain.extent.width, swapchain.extent.height }},
			.scissor = {
				.offset = { 0, 0 },
				.extent = { swapchain.extent.width, swapchain.extent.height }},
			.colorAttachments = {{
				.texture = swapchain.textures[_currentSwapchainImageIndex],
				.loadOp = _bPrepass ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
				.clear = { { 34.0f / 255.0f, 34.0f / 255.0f, 29.0f / 255.0f, 1.0f } } }},
			.depthStencilAttachment = {
				.texture = depthTexture,
				.loadOp = _bPrepass ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
				.clear = { 0.0f, 0 } },
			.bindings = bMeshShadingPipelineEnabled ?
				Bindings({
					Binding(perFrameDataBuffers[frameIndex]),
					Binding(drawBuffers.drawsBuffer),
					Binding(drawBuffers.drawCommandsBuffer),
					Binding(geometryBuffers.meshletBuffer),
					Binding(geometryBuffers.meshesBuffer),
					Binding(geometryBuffers.meshletVerticesBuffer),
					Binding(geometryBuffers.meshletTrianglesBuffer),
					Binding(geometryBuffers.vertexBuffer),
					Binding(drawBuffers.meshletVisibilityBuffer),
					Binding(hzb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) }) :
				Bindings({
					Binding(perFrameDataBuffers[frameIndex]),
					Binding(geometryBuffers.vertexBuffer),
					Binding(drawBuffers.drawsBuffer),
					Binding(drawBuffers.drawCommandsBuffer) }),
			.pushConstants = bMeshShadingPipelineEnabled ?
				PushConstants({
					.byteSize = sizeof(PerPassData),
					.pData = &perPassData }) :
				PushConstants() },
			[&]()
			{
				if (bMeshShadingPipelineEnabled)
				{
					vkCmdDrawMeshTasksIndirectCountNV(_commandBuffer, drawBuffers.drawCommandsBuffer.resource,
						offsetof(DrawCommand, taskCount), drawBuffers.drawCountBuffer.resource, 0, kMaxDrawCount, sizeof(DrawCommand));
				}
				else
				{
					vkCmdBindIndexBuffer(_commandBuffer, geometryBuffers.indexBuffer.resource, 0, VK_INDEX_TYPE_UINT32);

					vkCmdDrawIndexedIndirectCount(_commandBuffer, drawBuffers.drawCommandsBuffer.resource,
						offsetof(DrawCommand, indexCount), drawBuffers.drawCountBuffer.resource, 0, kMaxDrawCount, sizeof(DrawCommand));
				}
			});
	};

	auto buildHzbPass = [&](
		VkCommandBuffer _commandBuffer)
	{
		for (u32 mipIndex = 0; mipIndex < hzb.mipCount; ++mipIndex)
		{
			u32 hzbMipSize = hzbSize >> mipIndex;
			Texture& rInputTexture = mipIndex == 0 ? depthTexture : hzbMips[mipIndex - 1];

			executePass(_commandBuffer, {
				.pipeline = hzbDownsamplePipeline,
				.bindings = {
					Binding(rInputTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
					Binding(hzbMips[mipIndex], VK_IMAGE_LAYOUT_GENERAL) },
				.pushConstants = {
					.byteSize = sizeof(hzbMipSize),
					.pData = &hzbMipSize } },
					[&]()
				{
					iv2 groupCount = iv2(ceil(f32(hzbMipSize) / kShaderGroupSizeNV));
					vkCmdDispatch(_commandBuffer, groupCount.x, groupCount.y, 1);
				});

			textureBarrier(_commandBuffer, hzbMips[mipIndex],
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
	};

	while (!glfwWindowShouldClose(pWindow))
	{
		EASY_BLOCK("Frame");

		glfwPollEvents();

		gui::newFrame(pWindow, settings);

		bMeshShadingPipelineEnabled = settings.bEnableMeshShadingPipeline;

		VkCommandBuffer commandBuffer = commandBuffers[frameIndex];
		FramePacingState framePacingState = framePacingStates[frameIndex];

		{
			EASY_BLOCK("WaitForFences");
			VK_CALL(vkWaitForFences(device.device, 1, &framePacingState.inFlightFence, VK_TRUE, UINT64_MAX));
		}

		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, device.surface, &surfaceCapabilities));

		VkExtent2D currentExtent = surfaceCapabilities.currentExtent;

		if (currentExtent.width == 0 || currentExtent.height == 0)
		{
			continue;
		}

		if (swapchain.extent.width != currentExtent.width ||
			swapchain.extent.height != currentExtent.height)
		{
			VK_CALL(vkDeviceWaitIdle(device.device));
			initializeSwapchainResources();

			continue;
		}

		VK_CALL(vkResetFences(device.device, 1, &framePacingState.inFlightFence));

		u32 currentSwapchainImageIndex;
		VK_CALL(vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX,
			framePacingState.imageAvailableSemaphore, VK_NULL_HANDLE, &currentSwapchainImageIndex));

		{
			EASY_BLOCK("UpdateUniformData");

			static auto previousTime = std::chrono::high_resolution_clock::now();
			auto currentTime = std::chrono::high_resolution_clock::now();

			f32 deltaTime = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - previousTime).count();
			previousTime = currentTime;

			updateCamera(pWindow, deltaTime, camera);

			perFrameData.view = camera.view;
			perFrameData.projection = camera.projection;
			perFrameData.cameraPosition = v4(camera.position, 0.0f);
			perFrameData.maxDrawCount = kMaxDrawCount;
			perFrameData.lodTransitionBase = 4.0f;
			perFrameData.lodTransitionStep = 1.25f;
			perFrameData.forcedLod = settings.bEnableForceMeshLod ? settings.forcedLod : -1;
			perFrameData.hzbSize = hzbSize;
			perFrameData.bMeshShadingPipelineEnabled = settings.bEnableMeshShadingPipeline ? 1 : 0;
			perFrameData.bMeshFrustumCullingEnabled = settings.bEnableMeshFrustumCulling ? 1 : 0;
			perFrameData.bMeshOcclusionCullingEnabled = settings.bEnableMeshOcclusionCulling ? 1 : 0;
			perFrameData.bMeshletConeCullingEnabled = settings.bEnableMeshletConeCulling ? 1 : 0;
			perFrameData.bMeshletFrustumCullingEnabled = settings.bEnableMeshletFrustumCulling ? 1 : 0;
			perFrameData.bMeshletOcclusionCullingEnabled = settings.bEnableMeshletOcclusionCulling ? 1 : 0;

			if (!settings.bEnableFreezeCamera)
			{
				perFrameData.freezeView = perFrameData.view;
				perFrameData.freezeCameraPosition = perFrameData.cameraPosition;
				getFrustumPlanes(camera, perFrameData.freezeFrustumPlanes);
			}

			memcpy(perFrameDataBuffers[frameIndex].pMappedData, &perFrameData, sizeof(perFrameData));
		}

		{
			EASY_BLOCK("Frame");

			vkResetCommandBuffer(commandBuffer, 0);
			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

			VK_CALL(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

			gpu::profiler::beginFrame(commandBuffer);

			{
				GPU_STATS(commandBuffer, "Frame");

				textureBarrier(commandBuffer, swapchain.textures[currentSwapchainImageIndex],
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_ACCESS_NONE, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

				{
					GPU_BLOCK(commandBuffer, "GenerateDrawsPrepass");

					fillBuffer(commandBuffer, device, drawBuffers.drawCountBuffer, 0,
						VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
						VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

					bufferBarrier(commandBuffer, device, drawBuffers.drawCommandsBuffer,
						VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
						VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

					generateDrawsPass(commandBuffer, /*bPrepass*/ true);
				}

				{
					GPU_BLOCK(commandBuffer, "GeometryPrepass");

					bufferBarrier(commandBuffer, device, drawBuffers.drawCountBuffer,
						VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

					bufferBarrier(commandBuffer, device, drawBuffers.drawCommandsBuffer,
						VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

					geometryPass(commandBuffer, currentSwapchainImageIndex, /*bPrepass*/ true);
				}

				{
					GPU_BLOCK(commandBuffer, "BuildHzbPass");

					if (!settings.bEnableFreezeCamera)
					{
						textureBarrier(commandBuffer, hzb,
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

						textureBarrier(commandBuffer, depthTexture,
							VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
							VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

						buildHzbPass(commandBuffer);

						textureBarrier(commandBuffer, depthTexture,
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
					}
				}

				{
					GPU_BLOCK(commandBuffer, "GenerateDrawsPass");

					fillBuffer(commandBuffer, device, drawBuffers.drawCountBuffer, 0,
						VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
						VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

					bufferBarrier(commandBuffer, device, drawBuffers.drawCommandsBuffer,
						VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
						VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

					generateDrawsPass(commandBuffer, /*bPrepass*/ false);
				}

				{
					GPU_BLOCK(commandBuffer, "GeometryPass");

					bufferBarrier(commandBuffer, device, drawBuffers.drawCountBuffer,
						VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

					bufferBarrier(commandBuffer, device, drawBuffers.drawCommandsBuffer,
						VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

					geometryPass(commandBuffer, currentSwapchainImageIndex, /*bPrepass*/ false);
				}

				gui::drawFrame(commandBuffer, frameIndex, swapchain.textures[currentSwapchainImageIndex]);

				textureBarrier(commandBuffer, swapchain.textures[currentSwapchainImageIndex],
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_NONE,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
			}

			VK_CALL(vkEndCommandBuffer(commandBuffer));

			submitAndPresent(commandBuffer, device, swapchain, currentSwapchainImageIndex, framePacingState);

			gpu::profiler::endFrame(device);
		}

		gui::updateGpuInfo(device, settings);

		frameIndex = (frameIndex + 1) % kMaxFramesInFlightCount;
	}

	{
		EASY_BLOCK("DeviceWaitIdle");
		VK_CALL(vkDeviceWaitIdle(device.device));
	}

	{
		EASY_BLOCK("Cleanup");

		gui::terminate();
		gpu::profiler::terminate(device);

		for (Buffer& rPerFrameDataBuffer : perFrameDataBuffers)
		{
			destroyBuffer(device, rPerFrameDataBuffer);
		}

		for (FramePacingState& rFramePacingState : framePacingStates)
		{
			destroyFramePacingState(device, rFramePacingState);
		}

		destroyTexture(device, hzb);

		for (Texture& rHzbMip : hzbMips)
		{
			destroyTextureView(device, rHzbMip);
		}

		{ // TODO-MILKRU: Move to destroyGeometryBuffers
			if (device.bMeshShadingPipelineAllowed)
			{
				destroyBuffer(device, geometryBuffers.meshletBuffer);
				destroyBuffer(device, geometryBuffers.meshletVerticesBuffer);
				destroyBuffer(device, geometryBuffers.meshletTrianglesBuffer);
			}

			destroyBuffer(device, geometryBuffers.vertexBuffer);
			destroyBuffer(device, geometryBuffers.indexBuffer);
			destroyBuffer(device, geometryBuffers.meshesBuffer);
		}

		{ // TODO-MILKRU: Move to destroyDrawBuffers
			destroyBuffer(device, drawBuffers.drawsBuffer);
			destroyBuffer(device, drawBuffers.drawCommandsBuffer);
			destroyBuffer(device, drawBuffers.drawCountBuffer);
			destroyBuffer(device, drawBuffers.meshVisibilityBuffer);
			destroyBuffer(device, drawBuffers.meshletVisibilityBuffer);
		}

		destroyPipeline(device, hzbDownsamplePipeline);

		if (device.bMeshShadingPipelineAllowed)
		{
			destroyPipeline(device, geometryMeshletPipeline);
		}

		destroyPipeline(device, geometryPipeline);
		destroyPipeline(device, generateDrawsPipeline);

		destroyTexture(device, depthTexture);
		destroySwapchain(device, swapchain);
		destroyDevice(device);
		destroyWindow(pWindow);
	}

	{
		const char* profileCaptureFileName = "cpu_profile_capture.prof";
		profiler::dumpBlocksToFile(profileCaptureFileName);
		printf("CPU profile capture saved to %s file.\n", profileCaptureFileName);
	}

	return EXIT_SUCCESS;
}
