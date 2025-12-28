// [moe("vertex", "fragment")]

import moe.sampler;
import moe.common;

struct SpritePCS {
    float4x4 transform;
    float4 color;
    float2 size;
    float2 texRegionOffset;
    float2 texRegionSize;
    float2 texSize;
    ImageId texId;
    uint isTextSprite;
}

[vk::push_constant]
SpritePCS pcs;

struct VertexOutput {
    float2 uv;
    float4 pos : SV_POSITION;
}

float2 vertexIdxToUV(uint idx) {
    return float2((idx == 1 || idx == 2) ? 1.0 : 0.0,
                  (idx == 2 || idx == 3) ? 1.0 : 0.0);
}

[shader("vertex")]
VertexOutput vertexMain(uint vertexId: SV_VulkanVertexID) {
    VertexOutput output;

    float2 uv = vertexIdxToUV(vertexId);
    float2 uvMin = pcs.texRegionOffset / pcs.texSize;
    float2 uvMax = (pcs.texRegionOffset + pcs.texRegionSize) / pcs.texSize;

    output.uv = lerp(uvMin, uvMax, uv);
    output.pos = mul(pcs.transform, float4(uv.xy * pcs.size, 0.0, 1.0));
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_TARGET {
    float4 outColor;
    if (pcs.texId != INVALID_TEXTURE_ID) {
        if (pcs.isTextSprite != 0) {
            outColor = pcs.color;
            // outColor.a *= sampleTextureNearest(pcs.texId, input.uv).r;
            outColor.a *= sampleTextureLinear(pcs.texId, input.uv).r;
        } else {
            outColor = sampleTextureNearest(pcs.texId, input.uv) * pcs.color;
        }
    } else {
        outColor = pcs.color;
    }

    if (outColor.a < 0.01) {
        discard;
    }

    return outColor;
}
