cbuffer goo_constants : register(b0)
{
    float4 c0;
    float4 c1;
    float4 c2;
    float4 c3;
    float4 c4;
    float4 c5;
    float4 c6;
    float4 c7;
};

struct ps_input
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv : TEXCOORD0;
};

float sd_round_box(float2 p, float2 b, float r)
{
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float4 over(float3 top_rgb, float top_a, float3 under_rgb, float under_a)
{
    float a = top_a + under_a * (1.0 - top_a);
    float3 rgb = a > 0.0001 ? (top_rgb * top_a + under_rgb * under_a * (1.0 - top_a)) / a : float3(0.0, 0.0, 0.0);
    return float4(rgb, a);
}

float field(float2 p, out float bubble_mix)
{
    float2 center = (c0.xy + c0.zw) * 0.5;
    float2 half_size = (c0.zw - c0.xy) * 0.5;
    float dp = sd_round_box(p - center, half_size, c1.w);
    float db = c1.z > 0.0 ? length(p - c1.xy) - c1.z : 1e5;
    float k = max(c7.z, 0.0001);
    float h = saturate(0.5 + 0.5 * (db - dp) / k);
    bubble_mix = 1.0 - h;
    return lerp(db, dp, h) - k * h * (1.0 - h);
}

float4 main(ps_input input) : SV_Target
{
    float2 p = input.pos.xy;

    float bubble_mix;
    float d = field(p, bubble_mix);
    float coverage = saturate(0.5 - d);

    float unused;
    float ds = max(field(p - float2(0.0, c6.z), unused), 0.0) / max(c6.y, 0.0001);
    float shade = c6.x * exp(-ds * ds);

    float4 base = lerp(c2, c3, bubble_mix);

    float cy = (c0.y + c0.w) * 0.5;
    float half_h = max((c0.w - c0.y) * 0.5, 1.0);
    float vy = (p.y - cy) / half_h;
    float span = max(c5.y - c5.x, 1.0);
    float along = saturate((p.x - c5.x) / span);
    float fall = 1.0 - smoothstep(c5.y - c5.z * 0.45, c5.y + c5.z * 0.55, p.x);
    float band = exp(-vy * vy * 1.4);
    float core = 0.55 + 0.45 * (1.0 - along);
    float g = saturate(c5.w * fall * band * core * (1.0 - bubble_mix) * c4.a);
    float3 color = lerp(base.rgb, c4.rgb, g);
    color += (1.0 - saturate(-d / 1.25)) * saturate(-vy) * c7.y;

    float4 result = over(color, coverage * base.a, float3(0.0, 0.0, 0.0), shade);
    return float4(result.rgb, result.a * c7.x);
}
