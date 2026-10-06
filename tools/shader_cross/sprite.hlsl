// SPIRV-Cross output from sprite.frag. Member names keep the SPIR-V id prefix.
cbuffer Sprite : register(b7)
{
    float4 _18_tint : packoffset(c0);
    float _18_extra : packoffset(c1);
    float2 _18_scale : packoffset(c1.z);
    row_major float4x4 _18_world : packoffset(c2);
    float _18_weights[2] : packoffset(c6);
};

static float4 frag_color;

struct SPIRV_Cross_Output
{
    float4 frag_color : SV_Target0;
};

void frag_main()
{
    float used = ((((_18_scale.x + _18_scale.y) + _18_world[0].x) + _18_world[1].y) + _18_weights[0]) + _18_weights[1];
    frag_color = (_18_tint * _18_extra) + float4(used, 0.0f, 0.0f, 0.0f);
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.frag_color = frag_color;
    return stage_output;
}
