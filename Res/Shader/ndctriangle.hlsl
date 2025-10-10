struct VertexData{
    float4 position:POSITION;
    float4 texcoord:TEXCOORD0;
    float4 normal:NORMAL;
    float4 tangent:TANGENT;
};

struct VSOut{
    float4 position:SV_POSITION;
    float4 color:TEXCOORD0;
};

cbuffer globalConstants:register(b0){
    float4 color;
};

cbuffer DefaultVertexCB:register(b1){
    float4x4 ProjectionMatrix;
    float4x4 ViewMatrix;
    float4x4 ModelMatrix;
    float4x4 IT_ModelMatrix;
    float4x4 ReservedMemory[1020];
};

VSOut MainVS(VertexData inVertexData){
    VSOut vo;
    vo.position=mul(ProjectionMatrix,inVertexData.position);
    vo.color=inVertexData.texcoord+color;
    return vo;
}

float4 MainPS(VSOut inPSInput):SV_TARGET{
    return inPSInput.color;
}