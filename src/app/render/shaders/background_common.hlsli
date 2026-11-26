cbuffer background_constants : register(b0)
{
    float2 resolution;
    float time;
    float scale;
    float4 tint;
    float2 origin;
    float corner_radius;
    float background_padding;
    float2 mask_origin;
    float2 mask_size;
};

float hash(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float panel_mask(float2 frag)
{
    const float2 local = frag + origin - mask_origin;
    const float2 half_size = mask_size * 0.5;
    const float2 q = abs(local - half_size) - (half_size - corner_radius);
    const float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - corner_radius;
    return saturate(0.5 - distance);
}

float4 finish(float3 color, float2 frag)
{
    const float mask = panel_mask(frag);
    return float4(max(color, 0.0) * mask, mask);
}
