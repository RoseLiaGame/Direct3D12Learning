#pragma once
#include <d3d12.h>
struct StaticMeshComponentVertexData {
	float mPosition[4];
	float mTexcoord[4];
	float mNormal[4];
	float mTangent[4];
};
class StaticMeshComponent {
public:
	ID3D12Resource* mVBO;
	D3D12_VERTEX_BUFFER_VIEW mVBOView;
	StaticMeshComponentVertexData* mVertexData;
	int mVertexCount;
	void SetVertexCount(int inVertexCount);
	void SetVertexPosition(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);
	void SetVertexTexcoord(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);
	void SetVertexNormal(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);
	void SetVertexTangent(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);
	void InitFromFile(ID3D12GraphicsCommandList* inCommandList, const char* inFilePath);
};

