#include "common.hlsli"

ps_input main(vs_input input)
{
    ps_input output;
    const float4 world_position = mul(float4(input.position, 1.0), world) + float4(part_offset, 0.0);
    output.position = mul(world_position, view_projection);
    output.world_position = world_position.xyz;
    output.normal = mul(float4(input.normal, 0.0), world_normal).xyz;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.uv2 = input.uv2;
    return output;
}
