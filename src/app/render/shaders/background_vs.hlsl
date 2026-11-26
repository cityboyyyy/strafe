float4 main(uint id : SV_VertexID) : SV_Position
{
    const float2 corner = float2(id == 2 ? 3.0 : -1.0, id == 1 ? 3.0 : -1.0);
    return float4(corner, 0.0, 1.0);
}
