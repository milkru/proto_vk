#pragma once

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
