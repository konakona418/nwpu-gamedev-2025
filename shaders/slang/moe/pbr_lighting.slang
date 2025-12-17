import light;
import pbr_forward;
import light_utils;

float3 calculateLight(Light light, float3 fragPos, float3 n, float3 v, float3 l,
                      float3 diffuseColor, float roughness, float metallic, float3 f0, float occlusion) {
    float3 h = normalize(v + l);
    float NoL = clamp(dot(n, l), 0.0, 1.0);

    float atten = 1.0;
    // Point lights - attenuation by distance
    // Directional lights - no attenuation
    // Spot lights - attenuation by angle and distance
    if (light.type != LightType.Directional) {
        atten = calculateAttenuation(fragPos, l, light);
    }

    float3 fr = pbrBRDF(
            diffuseColor, roughness, metallic, f0,
            n, v, l, h, NoL);

    return (fr * light.color) * (light.intensity * atten * NoL * occlusion);
}
