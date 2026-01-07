// [moe("vertex", "fragment")]

import moe.sampler;
import moe.common;

struct VertexOutput {
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex: SV_VulkanVertexID) {
    VertexOutput output;
    output.uv = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

struct BlendTwoPCS {
    ImageId inputImageUp;
    ImageId inputImageBloom;
    ImageId inputImageDown;
    float alpha;
}

[vk::push_constant]
BlendTwoPCS pcs;

/*[shader("fragment")]
float4 fragmentMain(float2 inUV: TEXCOORD0) {
    float4 colorUp = sampleTextureLinear(pcs.inputImageUp, inUV);
    float4 colorDown = sampleTextureLinear(pcs.inputImageDown, inUV);

    float t = pcs.alpha * colorUp.w;
    float3 blendedColor = lerp(colorDown.rgb, colorUp.rgb, t);

    return float4(blendedColor, 1.0);
}*/

[shader("fragment")]
float4 fragmentMain(float2 inUV: TEXCOORD0) : SV_Target {
    float4 scene = sampleTextureLinear(pcs.inputImageDown, inUV);
    float4 uiSharp = sampleTextureLinear(pcs.inputImageUp, inUV);
    float4 uiGlow = sampleTextureLinear(pcs.inputImageBloom, inUV);

    float3 result = lerp(scene.rgb, uiSharp.rgb, uiSharp.a * pcs.alpha);

    result += uiGlow.rgb * pcs.alpha;

    float exposure = 0.8;
    result = 1.0 - exp(-result * exposure);

    return float4(result, 1.0);
}
