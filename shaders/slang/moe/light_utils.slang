import light;

// ! fixme: use a more smooth falloff
float calculateDistanceAttenuation(float dist, float range) {
    // in glTF spec: attenuation = max( min( 1.0 - ( current_distance / range )^4, 1 ), 0 ) / current_distance^2
    return clamp(1.0 - pow((dist / range), 4.0), 0.0, 1.0) / (dist * dist);
}


float calculateAngularAttenuation(
        float3 lightDir, float3 l,
        float2 spotParams) {
    // ! fixme: no idea whether this is correct, double check
    // ! but it seems to work anyway
    // l: direction from fragment to light
    // lightVec : direction from light to fragment (should be normalized)
    float3 lightVec = -l;
    float cd = dot(lightDir, lightVec);
    // spotParams : (lightAngleScale, lightAngleOffset)
    float angularAttenuation = clamp(cd * spotParams.x + spotParams.y, 0.0, 1.0);
    angularAttenuation *= angularAttenuation;

    return angularAttenuation;
}

float calculateAttenuation(float3 pos, float3 l, Light light) {
    float dist = length(light.position - pos);
    float atten = calculateDistanceAttenuation(dist, light.radius);
    if (light.type == LightType.Spot) {
        atten *= calculateAngularAttenuation(light.direction, l, light.spotParams.xy);
    }
    return atten;
}
