// One particle quad per instance, the outputs match the CDiffuse_T1 (LIT) and Color_T1 permutations of Particle.wfx.

#ifndef LIT
#define LIT 0
#endif

#ifndef LIGHT_ENABLED
#define LIGHT_ENABLED 0
#endif

#ifndef LOCAL_LIGHTS
#define LOCAL_LIGHTS 0
#endif

#ifndef SHADOW
#define SHADOW 0
#endif

float4 c_proj[4] : register(c2);
float4 c_texMtx[2] : register(c6);
float4 c_sunDiffuse : register(c10);
float4 c_sunAmbient : register(c11);
float4 c_sunDir : register(c12);
float4 c_localColor[4] : register(c17);
float4 c_localPos[4] : register(c21);
float4 c_attenConst : register(c25);
float4 c_attenLinear : register(c26);
float4 c_attenQuad : register(c27);
float4 c_emissive : register(c29);
float4 c_fog : register(c30);
float4 c_worldView[3] : register(c31);
float4 c_particleNormal : register(c35);
float4 c_shadow[12] : register(c224);

struct VsIn {
    float2 corner : POSITION;
    float4 color : COLOR0;
    float3 center : TEXCOORD1;
    float3 axisU : TEXCOORD2;
    float3 axisV : TEXCOORD3;
    float2 texBase : TEXCOORD4;
    float2 texCell : TEXCOORD5;
};

struct VsOut {
    float4 position : POSITION;
    float4 color : COLOR0;
    float2 texCoord : TEXCOORD0;
#if SHADOW >= 1
    float3 shadowPosition : TEXCOORD1;
    float3 shadowNormal : TEXCOORD2;
#endif
#if SHADOW == 1
    float3 shadow0 : TEXCOORD3;
#elif SHADOW == 2
    float4 shadow0 : TEXCOORD3;
    float4 shadow1 : TEXCOORD4;
    float4 shadow2 : TEXCOORD5;
#endif
    float fog : FOG;
};

float AttenComponent(float4 v, int i) {
    return i == 0 ? v.x : i == 1 ? v.y : i == 2 ? v.z : v.w;
}

VsOut main(VsIn input) {
    VsOut output;

    float4 local = float4(input.center + input.corner.x * input.axisU + input.corner.y * input.axisV, 1.0f);
    float4 p = float4(dot(c_worldView[0], local), dot(c_worldView[1], local), dot(c_worldView[2], local), 1.0f);

    output.position = float4(dot(c_proj[0], p), dot(c_proj[1], p), dot(c_proj[2], p), dot(c_proj[3], p));
    output.fog = min(pow(max(p.z * c_fog.x + c_fog.y, 0.0f), c_fog.z), 1.0f);

    float2 uv = input.texBase + float2(input.corner.x + 1.0f, 1.0f - input.corner.y) * 0.5f * input.texCell;
    float3 uv1 = float3(uv, 1.0f);
    output.texCoord = float2(dot(c_texMtx[0].xyw, uv1), dot(c_texMtx[1].xyw, uv1));

#if LIT
    float3 normal = normalize(float3(dot(c_worldView[0].xyz, c_particleNormal.xyz), dot(c_worldView[1].xyz, c_particleNormal.xyz), dot(c_worldView[2].xyz, c_particleNormal.xyz)));
#else
    float3 normal = float3(0.0f, 0.0f, 1.0f);
#endif

#if LIT && LIGHT_ENABLED
    float3 light = c_sunAmbient.xyz + saturate(dot(-c_sunDir.xyz, normal)) * c_sunDiffuse.xyz;

    [unroll]
    for (int i = 0; i < LOCAL_LIGHTS; i++) {
        float3 toLight = c_localPos[i].xyz - p.xyz;
        float distSq = dot(toLight, toLight);
        float inv = rsqrt(distSq);
        float atten = 1.0f / (AttenComponent(c_attenConst, i) + distSq * inv * AttenComponent(c_attenLinear, i) + distSq * AttenComponent(c_attenQuad, i));
        light += c_localColor[i].xyz * (max(dot(toLight, normal) * inv, 0.0f) * atten);
    }

    output.color = saturate(input.color * float4(saturate(light), 1.0f) + c_emissive);
#elif !LIT && LIGHT_ENABLED
    output.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
#else
    output.color = input.color;
#endif

#if SHADOW >= 1
    output.shadowPosition = p.xyz;
    output.shadowNormal = normal;
#endif
#if SHADOW == 1
    output.shadow0 = float3(dot(p, c_shadow[0]), dot(p, c_shadow[1]), dot(p, c_shadow[2]));
#elif SHADOW == 2
    output.shadow0 = float4(dot(p, c_shadow[0]), dot(p, c_shadow[1]), dot(p, c_shadow[2]), dot(p, c_shadow[9]));
    output.shadow1 = float4(dot(p, c_shadow[3]), dot(p, c_shadow[4]), dot(p, c_shadow[5]), dot(p, c_shadow[10]));
    output.shadow2 = float4(dot(p, c_shadow[6]), dot(p, c_shadow[7]), dot(p, c_shadow[8]), dot(p, c_shadow[11]));
#endif

    return output;
}
