#include "core/device.h"
#include "core/buffer.h"

#include "shaders/shader_interop.h"
#include "geometry.h"
#include "draw.h"

DrawBuffers createDrawBuffers(
	Device& _rDevice,
	Geometry& _rGeometry,
	u32 _maxDrawCount,
	u32 _spawnCubeSize)
{
	std::vector<PerDrawData> perDrawDataVector;
	perDrawDataVector.reserve(_maxDrawCount);

	u32 meshletVisibilityBufferAllocator = 0;
	for (u32 drawIndex = 0; drawIndex < _maxDrawCount; ++drawIndex)
	{
		u32 meshIndex = drawIndex % _rGeometry.meshes.size();
		Mesh& mesh = _rGeometry.meshes[meshIndex];

		auto randomFloat = []()
		{
			return f32(rand()) / RAND_MAX;
		};

		PerDrawData perDrawData = {
			.meshIndex = meshIndex,
			.meshletVisibilityOffset = meshletVisibilityBufferAllocator };

		perDrawData.model = glm::scale(m4(1.0f), v3(1.0f));
		perDrawData.model = glm::rotate(perDrawData.model,
			glm::radians(360.0f * randomFloat()), v3(0.0, 1.0, 0.0));

		perDrawData.model = glm::translate(perDrawData.model, {
			_spawnCubeSize * (randomFloat() - 0.5f),
			_spawnCubeSize * (randomFloat() - 0.5f),
			_spawnCubeSize * (randomFloat() - 0.5f) });

		perDrawDataVector.push_back(perDrawData);
		meshletVisibilityBufferAllocator += mesh.lods[0].meshletCount;
	}

	DrawBuffers drawBuffers = {
		.drawsBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(PerDrawData) * perDrawDataVector.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = perDrawDataVector.data() }),

		.drawCommandsBuffer = createBuffer(_rDevice, {
			.byteSize = 2 * sizeof(DrawCommand) * perDrawDataVector.size(),
			.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT }),

		.drawCountBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(u32),
			.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT }),

		.meshVisibilityBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(u32) * perDrawDataVector.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT }),

		.meshletVisibilityBuffer = createBuffer(_rDevice, {
				.byteSize = sizeof(u32) * u64(ceil(f32(meshletVisibilityBufferAllocator) / 32)), // Bit packing
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT }) };

	immediateSubmit(_rDevice, [&](VkCommandBuffer _commandBuffer)
		{
			fillBuffer(_commandBuffer, _rDevice, drawBuffers.drawCountBuffer, 0,
				VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		});

	return drawBuffers;
}

void destroyDrawBuffers(
	Device& _rDevice,
	DrawBuffers& _rDrawBuffers)
{
	destroyBuffer(_rDevice, _rDrawBuffers.drawsBuffer);
	destroyBuffer(_rDevice, _rDrawBuffers.drawCommandsBuffer);
	destroyBuffer(_rDevice, _rDrawBuffers.drawCountBuffer);
	destroyBuffer(_rDevice, _rDrawBuffers.meshVisibilityBuffer);
	destroyBuffer(_rDevice, _rDrawBuffers.meshletVisibilityBuffer);
}
