import light;
import light_utils;

float3 calculateToonLight(Light light, float3 fragPos, float3 n, float3 v, float3 l,
                          float3 diffuseColor, float roughness, float metallic, float3 f0, float occlusion) {

    float NoL = dot(n, l);

    float3 h = normalize(v + l);
    float NoH = clamp(dot(n, h), 0.0, 1.0);

    float toonNoL = smoothstep(0.0, 0.02, NoL);

    float specPow = exp2(10.0 * (1.0 - roughness) + 1.0);

    float spec = pow(NoH, specPow);
    float toonSpec = step(0.5, spec) * (1.0 - roughness);


    float atten = 1.0;
    if (light.type != LightType.Directional) {
        atten = calculateDistanceAttenuation(length(light.position - fragPos), light.radius);
        if (light.type == LightType.Spot) {
            atten *= calculateAngularAttenuation(light.direction, l, light.spotParams.xy);
        }
    }

    float3 lightForce = light.color * light.intensity;
    float3 diffuse = diffuseColor * toonNoL * occlusion;
    float3 specular = f0 * toonSpec * occlusion;

    return (diffuse + specular) * lightForce * atten;
}
