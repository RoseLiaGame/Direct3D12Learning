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
    float4 positionWS = mul(ModelMatrix,inVertexData.position);
    float4 positionvs = mul(ViewMatrix,positionWS);
    vo.position=mul(ProjectionMatrix,positionvs);
    vo.color=float4(inVertexData.normal.xyz,1.0);
    return vo;
}

float4 MainPS(VSOut inPSInput):SV_TARGET{
    float3 ambientColor = float3(0.1f,0.1f,0.1f);
    float3 diffuseColor = float3(0.0f,0.0f,0.0f);
    float3 specularColor = float3(0.0f,0.0f,0.0f);
    float3 sufaceColor = ambientColor + diffuseColor + specularColor;
    return float4(sufaceColor,1.0f);
}