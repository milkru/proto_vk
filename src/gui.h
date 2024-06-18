#pragma once

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
		_Out_ Settings& _rSettings);

	void drawFrame(
		VkCommandBuffer _commandBuffer,
		u32 _frameIndex,
		Texture& _rAttachment);

	void updateGpuInfo(
		Device& _rDevice,
		_Out_ Settings& _rSettings);
}
