struct VertexData{
    float4 position:POSITION;
    float4 texcoord:TEXCOORD0;
    float4 normal:NORMAL;
    float4 tangent:TANGENT;
};

struct VSOut{
    float4 position:SV_POSITION;
    float4 normal:NORMAL;
    float4 texcoord:TEXCOORD0;
    float4 positionWS:TEXCOORD1;
};

static const float PI=3.141592f;
cbuffer globalConstants:register(b0){
    float4 misc;
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
    vo.normal = mul(IT_ModelMatrix,inVertexData.normal);
    float3 positionMS = inVertexData.position.xyz + vo.normal * sin(misc.x);
    float4 positionWS = mul(ModelMatrix,float4(positionMS,1.0));
    float4 positionVS = mul(ViewMatrix,positionWS);
    vo.position = mul(ProjectionMatrix,positionVS);
    vo.normal = mul(IT_ModelMatrix,inVertexData.normal);
    vo.positionWS = positionWS;
    vo.texcoord = inVertexData.texcoord;
    return vo;
}

[maxvertexcount(4)]
void MainGS(point VSOut inPoint[1], uint inPrimitiveID:SV_PRIMITIVEID, inout PointStream<VSOut> outPointStream){ 
    inPoint[0].texcoord = float4(0.0f,0.0f,0.0f,1.0f);
    outPointStream.Append(inPoint[0]);
}


float4 MainPS(VSOut inPSInput):SV_TARGET{
    float3 N=normalize(inPSInput.normal.xyz);
    float3 bottomColor = float3(0.1f,0.4f,0.6f);
    float3 topColor = float3(0.7f,0.7f,0.7f);
    float theta = asin(N.y);// -PI/2~PI/2
    theta/=PI;
    theta+=0.5f;//0~1
    float ambientStrength = 1.0f;
    float3 ambientColor = lerp(bottomColor,topColor,theta)* ambientStrength;
    float3 sufaceColor = ambientColor+inPSInput.texcoord.rgb;
    return float4(sufaceColor,1.0f);
}