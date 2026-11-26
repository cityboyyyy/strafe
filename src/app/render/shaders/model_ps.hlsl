#include "common.hlsli"

Texture2D base_texture : register(t0);
Texture2D metal_rough_texture : register(t1);
Texture2D normal_texture : register(t2);
Texture2D occlusion_texture : register(t3);
Texture2D emissive_texture : register(t4);
Texture2D<float> shadow_map : register(t5);
SamplerState linear_sampler : register(s0);
SamplerComparisonState shadow_sampler : register(s1);

static const float pi = 3.14159265;
static const uint no_texture = 0xffffffff;

float2 select_uv(ps_input input, uint set)
{
    if (set == 1)
        return input.uv1;
    if (set == 2)
        return input.uv2;
    return input.uv0;
}

float3 perturb_normal(float3 n, float3 position, float2 uv, float3 sampled)
{
    const float3 dp1 = ddx(position);
    const float3 dp2 = ddy(position);
    const float2 duv1 = ddx(uv);
    const float2 duv2 = ddy(uv);
    const float3 dp2_perp = cross(dp2, n);
    const float3 dp1_perp = cross(n, dp1);
    const float3 t = dp2_perp * duv1.x + dp1_perp * duv2.x;
    const float3 b = dp2_perp * duv1.y + dp1_perp * duv2.y;
    const float scale = rsqrt(max(max(dot(t, t), dot(b, b)), 1e-20));
    const float3x3 tbn = float3x3(-t * scale, b * scale, n);
    return normalize(mul(sampled, tbn));
}

float3 studio(float3 direction, float roughness)
{
    const float spread = 0.10 + roughness * 0.65;
    const float sharp = 1.0 - roughness * 0.75;
    const float horizon = smoothstep(-0.35, 0.65, direction.y);
    float3 environment = lerp(ground_color.rgb * 0.5, sky_color.rgb * 1.5, horizon);
    const float top_box = exp(-pow((direction.y - 0.55) / spread, 2.0));
    const float side_box = exp(-pow((direction.x - 0.75) / spread, 2.0)) * smoothstep(-0.2, 0.3, direction.y);
    const float rim_box = exp(-pow((direction.x + 0.85) / spread, 2.0)) * 0.5;
    environment += (top_box * 1.6 + side_box * 0.9 + rim_box) * sharp;
    return environment;
}

float shadow(float3 position, float3 normal)
{
    const float3 offset_position = position + normal * shadow_params.y;
    const float4 light_space = mul(float4(offset_position, 1.0), light_view_projection);
    const float3 ndc = light_space.xyz / light_space.w;
    const float2 uv = ndc.xy * float2(0.5, -0.5) + 0.5;
    if (any(uv < 0.0) || any(uv > 1.0) || ndc.z > 1.0)
        return 1.0;

    static const float2 taps[12] = {
        float2(-0.326, -0.406), float2(-0.840, -0.074), float2(-0.696, 0.457), float2(-0.203, 0.621),
        float2(0.962, -0.195), float2(0.473, -0.480), float2(0.519, 0.767), float2(0.185, -0.893),
        float2(0.507, 0.064), float2(0.896, 0.412), float2(-0.322, -0.933), float2(-0.792, -0.598)
    };

    const float spread = shadow_params.x * 2.2;
    const float reference = ndc.z - shadow_params.z;
    float lit = 0.0;
    [unroll]
    for (int i = 0; i < 12; ++i)
        lit += shadow_map.SampleCmpLevelZero(shadow_sampler, uv + taps[i] * spread, reference);
    return lit / 12.0;
}

float3 tonemap_aces(float3 x)
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

float3 tonemap_neutral(float3 color)
{
    const float start = 0.76;
    const float desaturation = 0.15;
    const float low = min(color.r, min(color.g, color.b));
    const float offset = low < 0.08 ? low - 6.25 * low * low : 0.04;
    color -= offset;
    const float peak = max(color.r, max(color.g, color.b));
    if (peak < start)
        return saturate(color);
    const float d = 1.0 - start;
    const float new_peak = 1.0 - d * d / (peak + d - start);
    color *= new_peak / peak;
    const float g = 1.0 - 1.0 / (desaturation * (peak - new_peak) + 1.0);
    return saturate(lerp(color, new_peak.xxx, g));
}

float3 tonemap(float3 x)
{
    return shadow_params.w > 0.5 ? tonemap_neutral(x) : tonemap_aces(x);
}

float4 main(ps_input input, bool front : SV_IsFrontFace) : SV_Target
{
    float4 base = base_color_factor;
    if (base_uv != no_texture)
        base *= base_texture.Sample(linear_sampler, select_uv(input, base_uv));

    if (alpha_mode == 1 && base.a < alpha_cutoff)
        discard;

    float alpha = alpha_mode == 2 ? base.a : 1.0;
    if (split_alpha != 0)
    {
        const bool solid = base.a >= 0.99;
        if (solid == (blend_pass != 0))
            discard;
        alpha = solid ? 1.0 : base.a * glass_opacity;
    }

    float metallic = metallic_factor;
    float roughness = roughness_factor;
    if (metal_rough_uv != no_texture)
    {
        const float4 mr = metal_rough_texture.Sample(linear_sampler, select_uv(input, metal_rough_uv));
        roughness *= mr.g;
        metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.05, 1.0);
    metallic = saturate(metallic);

    float3 n = normalize(input.normal);
    if (!front)
        n = -n;

    if (normal_uv != no_texture)
    {
        const float2 uv = select_uv(input, normal_uv);
        float3 sampled = normal_texture.Sample(linear_sampler, uv).xyz * 2.0 - 1.0;
        sampled.xy *= normal_scale;
        n = perturb_normal(n, input.world_position, uv, normalize(sampled));
    }

    const float3 normal_dx = ddx(n);
    const float3 normal_dy = ddy(n);
    const float variance = 0.25 * (dot(normal_dx, normal_dx) + dot(normal_dy, normal_dy));
    roughness = sqrt(saturate(roughness * roughness + min(2.0 * variance, 0.18)));

    const float3 v = normalize(camera_position - input.world_position);
    const float nv = saturate(dot(n, v)) + 1e-4;
    const float3 f0 = lerp(0.04, base.rgb, metallic);
    const float3 diffuse = base.rgb * (1.0 - metallic);
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;

    const float key_shadow = shadow(input.world_position, normalize(input.normal) * (front ? 1.0 : -1.0));

    float3 color = 0.0;
    [unroll]
    for (uint i = 0; i < 3; ++i)
    {
        const float3 l = light_direction[i].xyz;
        const float visibility = i == 0 ? key_shadow : 1.0;
        const float3 h = normalize(l + v);
        const float nl = saturate(dot(n, l));
        const float nh = saturate(dot(n, h));
        const float vh = saturate(dot(v, h));
        const float denom = nh * nh * (a2 - 1.0) + 1.0;
        const float d = a2 / (pi * denom * denom);
        const float g = (nv / (nv * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
        const float3 f = f0 + (1.0 - f0) * pow(1.0 - vh, 5.0);
        const float3 specular = d * g * f / max(4.0 * nv * nl, 1e-4);
        color += ((1.0 - f) * diffuse / pi + specular) * light_color[i].rgb * nl * visibility;
    }

    const float3 ambient_light = lerp(ground_color.rgb, sky_color.rgb, n.y * 0.5 + 0.5);
    const float3 fresnel = f0 + (max(1.0 - roughness, f0) - f0) * pow(1.0 - nv, 5.0);
    float occlusion = 1.0;
    if (occlusion_uv != no_texture)
        occlusion = lerp(1.0, occlusion_texture.Sample(linear_sampler, select_uv(input, occlusion_uv)).r, occlusion_strength);
    const float3 reflected = reflect(-v, n);
    const float sky_occlusion = lerp(0.68, 1.0, key_shadow);
    color += ambient_light * diffuse * (1.0 - fresnel) * occlusion * sky_occlusion;
    color += studio(reflected, roughness) * fresnel * occlusion * sky_occlusion;

    float3 emissive = emissive_factor;
    if (emissive_uv != no_texture)
        emissive *= emissive_texture.Sample(linear_sampler, select_uv(input, emissive_uv)).rgb;
    color += emissive;

    if (highlight.a > 0.0)
    {
        const float rim = pow(1.0 - nv, 2.0);
        const float3 glow = highlight.rgb * (0.9 + 1.4 * rim);
        color = lerp(color, glow, saturate(highlight.a * 0.55)) + highlight.rgb * highlight.a * 0.12;
    }

    return float4(tonemap(color * exposure), alpha);
}
