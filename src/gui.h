#pragma once

// TODO-MILKRU: Should this stay here?
struct Settings
{
	const char* deviceName = "Unknown Device";
	std::map<std::string, f64> gpuTimes;
	u64 clippingInvocations = 0;
	u64 deviceMemoryUsage = 0;
	i32 forcedLod = 0;
	bool bEnableForceMeshLod = false;
	bool bEnableFreezeCamera = false;
	bool bEnableMeshFrustumCulling = false;
	bool bEnableMeshOcclusionCulling = false;
	bool bEnableMeshletConeCulling = false;
	bool bEnableMeshletFrustumCulling = false;
	bool bEnableMeshletOcclusionCulling = false;
	bool bEnableSmallTriangleCulling = false;
	bool bEnableTriangleBackfaceCulling = false;
};

namespace gui
{
	void initialize(
		Device& _rDevice,
		VkFormat _colorFormat,
		VkFormat _depthFormat,
		f32 _width,
		f32 _height);

	void terminate();

	void newFrame(
		GLFWwindow* _pWindow,
		Settings& _rSettings);

	void drawFrame(
		VkCommandBuffer _commandBuffer,
		u32 _frameIndex,
		Texture& _rAttachment);

	void updateGpuInfo(
		Device& _rDevice,
		Settings& _rSettings);
}
