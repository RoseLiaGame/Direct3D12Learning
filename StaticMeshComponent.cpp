#include "StaticMeshComponent.h"

void StaticMeshComponent::SetVertexCount(int inVertexCount) {
	mVertexCount = inVertexCount;
	mVertexData = new StaticMeshComponentVertexData[inVertexCount];
	memset(mVertexData, 0, sizeof(StaticMeshComponentVertexData) * inVertexCount);
}
void StaticMeshComponent::SetVertexPosition(int inIndex, float inX, float inY, float inZ, float inW /* = 1.0f */) {
	mVertexData[inIndex].mPosition[0] = inX;
	mVertexData[inIndex].mPosition[1] = inY;
	mVertexData[inIndex].mPosition[2] = inZ;
	mVertexData[inIndex].mPosition[3] = inW;
}
void StaticMeshComponent::SetVertexTexcoord(int inIndex, float inX, float inY, float inZ, float inW /* = 1.0f */) {
	mVertexData[inIndex].mTexcoord[0] = inX;
	mVertexData[inIndex].mTexcoord[1] = inY;
	mVertexData[inIndex].mTexcoord[2] = inZ;
	mVertexData[inIndex].mTexcoord[3] = inW;
}
void StaticMeshComponent::SetVertexNormal(int inIndex, float inX, float inY, float inZ, float inW /* = 1.0f */) {
	mVertexData[inIndex].mNormal[0] = inX;
	mVertexData[inIndex].mNormal[1] = inY;
	mVertexData[inIndex].mNormal[2] = inZ;
	mVertexData[inIndex].mNormal[3] = inW;
}
void StaticMeshComponent::SetVertexTangent(int inIndex, float inX, float inY, float inZ, float inW /* = 1.0f */) {
	mVertexData[inIndex].mTangent[0] = inX;
	mVertexData[inIndex].mTangent[1] = inY;
	mVertexData[inIndex].mTangent[2] = inZ;
	mVertexData[inIndex].mTangent[3] = inW;
}
void StaticMeshComponent::InitFromFile(ID3D12GraphicsCommandList* inCommandList, const char* inFilePath) {
	mVBOView.BufferLocation = mVBO->GetGPUVirtualAddress();
	mVBOView.SizeInBytes = sizeof(StaticMeshComponentVertexData) * mVertexCount;
	mVBOView.StrideInBytes = sizeof(StaticMeshComponentVertexData);
}