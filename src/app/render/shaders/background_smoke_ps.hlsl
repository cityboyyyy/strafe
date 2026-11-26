#include "background_common.hlsli"

float noise(float2 p)
{
    const float2 cell = floor(p);
    const float2 f = frac(p);
    const float2 u = f * f * (3.0 - 2.0 * f);
    const float a = hash(cell);
    const float b = hash(cell + float2(1.0, 0.0));
    const float c = hash(cell + float2(0.0, 1.0));
    const float d = hash(cell + float2(1.0, 1.0));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm(float2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    const float2x2 turn = float2x2(0.8, -0.6, 0.6, 0.8);
    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        value += amplitude * noise(p);
        p = mul(turn, p) * 2.03 + 17.1;
        amplitude *= 0.5;
    }
    return value;
}

float4 main(float4 position : SV_Position) : SV_Target
{
    const float2 frag = position.xy - origin;
    const float2 p = (frag - 0.5 * resolution) / resolution.y;
    const float t = time * 0.035;

    const float2 q = float2(fbm(p * 1.4 + float2(0.0, t)), fbm(p * 1.4 + float2(5.2, 1.3) - t));
    const float2 r = float2(fbm(p * 1.4 + 2.6 * q + float2(1.7, 9.2) + t * 1.3), fbm(p * 1.4 + 2.6 * q + float2(8.3, 2.8) - t * 0.9));
    const float field = fbm(p * 1.4 + 2.8 * r);

    const float smoke = smoothstep(0.38, 0.93, field);
    const float veins = smoothstep(0.55, 0.0, abs(field - 0.62) * 9.0) * 0.35;

    float3 color = float3(0.006, 0.0065, 0.008);
    color += (smoke * 0.11 + veins * 0.05) * float3(0.92, 0.95, 1.0);
    color += smoke * smoke * tint.rgb * tint.a * 0.06;

    const float2 spot = p - float2(0.0, -0.02);
    color += exp(-dot(spot, spot) * 4.5) * 0.018;
    color *= saturate(1.0 - dot(p * float2(0.85, 1.1), p * float2(0.85, 1.1)) * 0.75);

    const float grain = hash(frag + frac(time * 7.13) * 311.7) - 0.5;
    color += grain * 0.010;
    return finish(color, frag);
}
