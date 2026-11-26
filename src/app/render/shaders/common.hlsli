cbuffer frame_constants : register(b0)
{
    float4x4 view_projection;
    float4x4 world;
    float4x4 world_normal;
    float3 camera_position;
    float exposure;
    float4 light_direction[3];
    float4 light_color[3];
    float4 sky_color;
    float4 ground_color;
    float4x4 light_view_projection;
    float4 shadow_params;
};

cbuffer material_constants : register(b1)
{
    float4 base_color_factor;
    float3 emissive_factor;
    float metallic_factor;
    float roughness_factor;
    float alpha_cutoff;
    float normal_scale;
    float occlusion_strength;
    uint base_uv;
    uint metal_rough_uv;
    uint normal_uv;
    uint occlusion_uv;
    uint emissive_uv;
    uint alpha_mode;
    uint split_alpha;
    float glass_opacity;
};

cbuffer pass_constants : register(b2)
{
    uint blend_pass;
    uint3 pass_padding;
};

cbuffer part_constants : register(b3)
{
    float4 highlight;
    float3 part_offset;
    uint part_id;
};

struct vs_input
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
};

struct ps_input
{
    float4 position : SV_Position;
    float3 world_position : POSITION;
    float3 normal : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
};
