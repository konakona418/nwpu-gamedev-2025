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

struct GaussianBlurPCS {
    ImageId inputTextureId;
    float2 direction;
    float2 invResolution;
    float radius;
    uint32_t extractLuminance;
}

[vk::push_constant]
GaussianBlurPCS pcs;

const static float offsets[3] = { 1.41176470588, 3.29411764705, 5.17647058823 };
const static float weights[3] = { 0.29690696467, 0.09447039785, 0.01038136240 };
const static float centerWeight = 0.19648255015;

float4 applyPrefilter(float4 color) {
    if (pcs.extractLuminance == 0) return color;

    float luma = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    float threshold = 0.5;

    float knee = 0.1;

    float weight = clamp((luma - threshold + knee) / (2.0 * knee), 0.0, 1.0);
    weight = weight * weight;

    return float4(color.rgb * weight, color.a * weight);
}

[shader("fragment")]
float4 fragmentMain(float2 inUV: TEXCOORD0) : SV_Target {
    float4 baseColor = sampleTextureLinear(pcs.inputTextureId, inUV);
    float4 result = applyPrefilter(baseColor);
    result *= centerWeight;

    float2 step = pcs.direction * pcs.radius * pcs.invResolution;

    for (int i = 0; i < 3; i++) {
        float2 uvOffset = step * offsets[i];

        float4 s1 = sampleTextureLinear(pcs.inputTextureId, inUV + uvOffset);
        float4 s2 = sampleTextureLinear(pcs.inputTextureId, inUV - uvOffset);

        s1 = applyPrefilter(s1);
        s2 = applyPrefilter(s2);

        result += (s1 + s2) * weights[i];
    }

    return result;
}
