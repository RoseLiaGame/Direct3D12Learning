#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <stdio.h>


ID3D12Resource* CreateBufferObject(ID3D12GraphicsCommandList* inCommandList,
	void* inData, int inDataLen, D3D12_RESOURCE_STATES inFinalResourceState);

