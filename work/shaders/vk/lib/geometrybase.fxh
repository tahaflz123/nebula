//------------------------------------------------------------------------------
//  geometrybase.fxh
//  (C) 2012 Gustav Sterbrant
//------------------------------------------------------------------------------

#ifndef GEOMETRYBASE_FXH
#define GEOMETRYBASE_FXH

#include "lib/std.fxh"
#include "lib/skinning.fxh"
#include "lib/util.fxh"
#include "lib/shared.fxh"
#include "lib/objects_shared.fxh"
#include "lib/defaultsamplers.fxh"
#include "lib/pbr.fxh"
#include "lib/stencil.fxh"

//#define PN_TRIANGLES
render_state StandardState
{
};

render_state StencilState
{
	DepthWrite = false;
    DepthEnabled = true;
    DepthFunc = Equal;
	StencilEnabled = true;
	StencilWriteMask = STENCIL_BIT_CHARACTER;
	StencilFrontPassOp = Replace;
};

render_state StandardNoCullState
{
	CullMode = None;
};

render_state AlphaState
{
	BlendEnabled[0] = true;
	SrcBlend[0] = SrcAlpha;
	DstBlend[0] = OneMinusSrcAlpha;
	DepthWrite = false;
	DepthEnabled = true;
};

render_state DefaultState
{
    DepthWrite = false;
    DepthEnabled = true;
    DepthFunc = Equal;
};

render_state DoubleSidedState
{
    CullMode = None;
    DepthWrite = false;
    DepthEnabled = true;
    DepthFunc = Equal;
};

render_state AlphaDoubleSidedState
{
    BlendEnabled[0] = true;
    SrcBlend[0] = SrcAlpha;
    DstBlend[0] = OneMinusSrcAlpha;
    DepthWrite = false;
    DepthEnabled = true;
    DepthFunc = Less;
    CullMode = None;
};

render_state DepthState
{
    CullMode = Back;
};

render_state DepthStateDoubleSided
{
    CullMode = None;
};

float FresnelPower = 0.0f;
float FresnelStrength = 0.0f;

#include "lib/materialparams.fxh"

//---------------------------------------------------------------------------------------------------------------------------
//											DIFFUSE
//---------------------------------------------------------------------------------------------------------------------------
prototype vec4 CalculateColor(vec4 albedoColor);

subroutine (CalculateColor) vec4 SimpleColor(
	in vec4 albedoColor)
{
	return vec4(albedoColor.rgb, 1.0f);
}

subroutine (CalculateColor) vec4 AlphaColor(
	in vec4 albedoColor)
{
	return albedoColor;
}

subroutine (CalculateColor) vec4 AlphaMaskSimpleColor(
	in vec4 color)
{
#if PIXEL_SHADER
    if (color.a <= alphaCutoff)
        discard;
#endif

    return vec4(color.rgb, 1.0f);
}

subroutine (CalculateColor) vec4 AlphaMaskAlphaColor(
	in vec4 color)
{
#if PIXEL_SHADER
    if (color.a <= alphaCutoff)
        discard;
#endif

    return color;
}

CalculateColor calcColor;

//---------------------------------------------------------------------------------------------------------------------------
//											NORMAL
//---------------------------------------------------------------------------------------------------------------------------
prototype vec3 CalculateBump(in vec3 tangent, in vec3 binormal, in vec3 normal, in vec4 bump);

subroutine (CalculateBump) vec3 NormalMapFunctorDXT5NM(
	in vec3 tangent,
	in vec3 binormal,
	in vec3 normal,
	in vec4 bumpData)
{
	mat3 tangentViewMatrix = mat3(normalize(tangent.xyz), normalize(binormal.xyz), normalize(normal.xyz));
	vec3 tNormal = vec3(0,0,0);
	tNormal.xy = (bumpData.ag * 2.0f) - 1.0f;
	tNormal.z = saturate(sqrt(1.0f - dot(tNormal.xy, tNormal.xy)));
	return tangentViewMatrix * tNormal;
}

subroutine (CalculateBump) vec3 NormalMapFunctor(
	in vec3 tangent,
	in vec3 binormal,
	in vec3 normal,
	in vec4 bumpData)
{
	mat3 tangentViewMatrix = mat3(normalize(tangent.xyz), normalize(binormal.xyz), normalize(normal.xyz));
	vec3 tNormal = vec3(0,0,0);
	tNormal.xy = (bumpData.xy * 2.0f) - 1.0f;
	tNormal.z = saturate(sqrt(1.0f - dot(tNormal.xy, tNormal.xy)));
	return tangentViewMatrix * tNormal;
}

subroutine (CalculateBump) vec3 FlatNormalFunctor(
	in vec3 tangent,
	in vec3 binormal,
	in vec3 normal,
	in vec4 bumpData)
{
	return normal;	
}

CalculateBump calcBump;

//---------------------------------------------------------------------------------------------------------------------------
//											SPECULAR
//---------------------------------------------------------------------------------------------------------------------------
prototype vec4 CalculateMaterial(in vec4 material);

subroutine (CalculateMaterial) vec4 DefaultMaterialFunctor(in vec4 material)
{
	return material;
}

// OSM = Occlusion, Smoothness, Metalness
subroutine (CalculateMaterial) vec4 OSMMaterialFunctor(in vec4 material)
{
	return ConvertOSM(material);
}

CalculateMaterial calcMaterial;

//---------------------------------------------------------------------------------------------------------------------------
//											ENVIRONMENT
//---------------------------------------------------------------------------------------------------------------------------
prototype vec3 CalculateEnvironment(in vec4 albedo, in vec3 F0, in vec3 worldNormal, in vec3 worldViewVec, in vec4 material);

subroutine (CalculateEnvironment) vec3 IBL(
	in vec4 albedo,
	in vec3 F0,
	in vec3 worldNormal,
	in vec3 worldViewVec,
	in vec4 material)
{
	vec3 reflectVec = reflect(-worldViewVec, worldNormal);
    float NdotV = saturate(dot(worldNormal, worldViewVec));
    vec3 F = FresnelSchlickGloss(F0, NdotV, material[MAT_ROUGHNESS]);
    vec3 reflection = sampleCubeLod(EnvironmentMap, CubeSampler, reflectVec, material[MAT_ROUGHNESS] * NumEnvMips).rgb;
    vec3 irradiance = sampleCubeLod(IrradianceMap, CubeSampler, worldNormal, 0).rgb;
    vec3 kD = vec3(1.0f) - F;
    kD *= 1.0f - material[MAT_METALLIC];

    vec3 ambientTerm = (irradiance * kD * albedo.rgb);
    return (ambientTerm + reflection * F) * material[MAT_CAVITY];
}

subroutine (CalculateEnvironment) vec3 ReflectionOnly(
	in vec4 albedo,
	in vec3 F0,
	in vec3 worldNormal,
	in vec3 worldViewVec,
	in vec4 material)
{
	vec3 reflectVec = reflect(-worldViewVec, worldNormal);
    float NdotV = saturate(dot(worldNormal, worldViewVec));
    vec3 F = FresnelSchlickGloss(F0, NdotV, material[MAT_ROUGHNESS]);
    vec3 reflection = sampleCubeLod(EnvironmentMap, CubeSampler, reflectVec, material[MAT_ROUGHNESS] * NumEnvMips).rgb;
    return (reflection * F);
}

subroutine (CalculateEnvironment) vec3 IrradianceOnly(
	in vec4 albedo,
	in vec3 F0,
	in vec3 worldNormal,
	in vec3 worldViewVec,
	in vec4 material)
{
	float NdotV = saturate(dot(worldNormal, worldViewVec));
    vec3 F = FresnelSchlickGloss(F0, NdotV, material[MAT_ROUGHNESS]);
    vec3 irradiance = sampleCubeLod(IrradianceMap, CubeSampler, worldNormal, 0).rgb;
    vec3 kD = vec3(1.0f) - F;
    kD *= 1.0f - material[MAT_METALLIC];
	vec3 ambientTerm = (irradiance * kD * albedo.rgb);
    return ambientTerm * material[MAT_CAVITY];
}

subroutine (CalculateEnvironment) vec3 NoEnvironment(
	in vec4 albedo,
	in vec3 F0,
	in vec3 worldNormal,
	in vec3 worldViewVec,
	in vec4 material)
{
	return vec3(0);
}

CalculateEnvironment calcEnv;

//------------------------------------------------------------------------------
/**
				STATIC GEOMETRY
*/
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/**
*/
shader
void
vsDepthStatic(
    [slot = 0] in vec3 position)
{
    vec4 modelSpace = Model * vec4(position, 1);
    gl_Position = ViewProjection * modelSpace;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsDepthStaticAlphaMask(
    [slot = 0] in vec3 position,
    [slot = 2] in vec2 uv,
    out vec2 UV)
{
    vec4 modelSpace = Model * vec4(position, 1);
    gl_Position = ViewProjection * modelSpace;
    UV = uv;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
psDepthOnly()
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
shader
void
psDepthOnlyAlphaMask(in vec2 UV)
{
    vec4 baseColor = sample2D(AlbedoMap, MaterialSampler, UV);
    if (baseColor.a <= AlphaSensitivity)
        discard;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsStatic(
	[slot=0] in vec3 position,
	[slot=1] in vec3 normal,
	[slot=2] in vec2 uv,
	[slot=3] in vec3 tangent,
	[slot=4] in vec3 binormal,
	out vec3 Tangent,
	out vec3 Normal,
	out vec3 Binormal,
	out vec2 UV,
	out vec3 WorldSpacePos,
	out vec4 ViewSpacePos)
{
	vec4 modelSpace = Model * vec4(position, 1);
    gl_Position = ViewProjection * modelSpace;
    
	Tangent 	  = (Model * vec4(tangent, 0)).xyz;
	Normal 		  = (Model * vec4(normal, 0)).xyz;
	Binormal 	  = (Model * vec4(binormal, 0)).xyz;
	UV            = uv;
	WorldSpacePos = modelSpace.xyz;
	ViewSpacePos  = View * modelSpace;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsStaticInstanced(
	[slot=0] in vec3 position,
	[slot=1] in vec3 normal,
	[slot=2] in vec2 uv,
	[slot=3] in vec3 tangent,
	[slot=4] in vec3 binormal,
	out vec3 Tangent,
	out vec3 Normal,
	out vec3 Binormal,
	out vec2 UV,
	out vec3 WorldSpacePos,
	out vec4 ViewSpacePos)
{
	vec4 modelSpace = ModelArray[gl_InstanceID] * vec4(position, 1);
    gl_Position = ViewProjection * modelSpace;

	Tangent 	  = (Model * vec4(tangent, 0)).xyz;
	Normal 		  = (Model * vec4(normal, 0)).xyz;
	Binormal 	  = (Model * vec4(binormal, 0)).xyz;
	UV            = uv;
	WorldSpacePos = modelSpace.xyz;
	ViewSpacePos  = View * modelSpace;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsStaticTessellated(
	[slot=0] in vec3 position,
	[slot=1] in vec3 normal,
	[slot=2] in vec2 uv,
	[slot=3] in vec3 tangent,
	[slot=4] in vec3 binormal,
	out vec3 Tangent,
	out vec3 Normal,
	out vec4 Position,
	out vec3 Binormal,
	out vec2 UV,
	out float Distance)
{
    Position = Model * vec4(position, 1);
    UV = uv;

	Tangent = (Model * vec4(tangent, 0)).xyz;
	Normal = (Model * vec4(normal, 0)).xyz;
	Binormal = (Model * vec4(binormal, 0)).xyz;

	float vertexDistance = distance( Position.xyz, EyePos.xyz );
	Distance = 1.0 - clamp( ( (vertexDistance - MinDistance) / (MaxDistance - MinDistance) ), 0.0, 1.0 - 1.0/TessellationFactor);
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsStaticColored(
	[slot=0] in vec3 position,
	[slot=1] in vec3 normal,
	[slot=2] in vec2 uv,
	[slot=3] in vec3 tangent,
	[slot=4] in vec3 binormal,
	[slot=5] in vec4 color,
	out vec3 Tangent,
	out vec3 Normal,
	out vec3 Binormal,
	out vec2 UV,
	out vec4 Color,
	out vec3 WorldViewVec)
{
	vec4 modelSpace = Model * vec4(position, 1);
    gl_Position = ViewProjection * modelSpace;
	UV = uv;
	Color = color;

	Tangent = (Model * vec4(tangent, 0)).xyz;
	Normal = (Model * vec4(normal, 0)).xyz;
	Binormal = (Model * vec4(binormal, 0)).xyz;
	WorldViewVec = EyePos.xyz - modelSpace.xyz;
}

//------------------------------------------------------------------------------
/**
				SKINNED GEOMETRY
*/
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/**
*/
shader
void
vsDepthSkinned(
    [slot = 0] in vec3 position,
	[slot = 7] in vec4 weights,
	[slot = 8] in uvec4 indices)
{
    vec4 skinnedPos = SkinnedPosition(position, weights, indices);
    vec4 modelSpace = Model * skinnedPos;
    gl_Position = ViewProjection * modelSpace;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsDepthSkinnedAlphaMask(
    [slot = 0] in vec3 position,
    [slot = 2] in vec2 uv,
	[slot = 7] in vec4 weights,
	[slot = 8] in uvec4 indices,
    out vec2 UV)
{
	vec4 skinnedPos = SkinnedPosition(position, weights, indices);
    vec4 modelSpace = Model * skinnedPos;
    gl_Position = ViewProjection * modelSpace;
    UV = uv;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsSkinned(
	[slot=0] in vec3 position,
	[slot=1] in vec3 normal,
	[slot=2] in vec2 uv,
	[slot=3] in vec3 tangent,
	[slot=4] in vec3 binormal,
	[slot=7] in vec4 weights,
	[slot=8] in uvec4 indices,
	out vec3 Tangent,
	out vec3 Normal,
	out vec3 Binormal,
	out vec2 UV,
	out vec3 WorldSpacePos,
	out vec4 ViewSpacePos)
{
	vec4 skinnedPos      = SkinnedPosition(position, weights, indices);
	vec4 skinnedNormal   = SkinnedNormal(normal, weights, indices);
	vec4 skinnedTangent  = SkinnedNormal(tangent, weights, indices);
	vec4 skinnedBinormal = SkinnedNormal(binormal, weights, indices);

	vec4 modelSpace = Model * skinnedPos;
    gl_Position = ViewProjection * modelSpace;
	
	Tangent 	  = (Model * skinnedTangent).xyz;
	Normal 		  = (Model * skinnedNormal).xyz;
	Binormal 	  = (Model * skinnedBinormal).xyz;
	UV            = uv;
	WorldSpacePos = modelSpace.xyz;
	ViewSpacePos  = View * modelSpace;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsSkinnedTessellated(
	[slot=0] in vec3 position,
	[slot=1] in vec3 normal,
	[slot=2] in vec2 uv,
	[slot=3] in vec3 tangent,
	[slot=4] in vec3 binormal,
	[slot=7] in vec4 weights,
	[slot=8] in uvec4 indices,
	out vec3 Tangent,
	out vec3 Normal,
	out vec4 Position,
	out vec3 Binormal,
	out vec2 UV,
	out float Distance)
{
	vec4 skinnedPos      = SkinnedPosition(position, weights, indices);
	vec4 skinnedNormal   = SkinnedNormal(normal, weights, indices);
	vec4 skinnedTangent  = SkinnedNormal(tangent, weights, indices);
	vec4 skinnedBinormal = SkinnedNormal(binormal, weights, indices);

	Position = Model * skinnedPos;
	UV = uv;

	Tangent = (Model * skinnedTangent).xyz;
	Normal = (Model * skinnedNormal).xyz;
	Binormal = (Model * skinnedBinormal).xyz;

	float vertexDistance = distance( Position.xyz, EyePos.xyz );
	Distance = 1.0 - clamp( ( (vertexDistance - MinDistance) / (MaxDistance - MinDistance) ), 0.0, 1.0 - 1.0/TessellationFactor);
}

//------------------------------------------------------------------------------
/**
*/
shader
void
vsBillboard(
	[slot=0] in vec3 position,
	[slot=1] in vec3 normal,
	[slot=2] in vec2 uv,
	[slot=3] in vec3 tangent,
	[slot=4] in vec3 binormal,
	out vec2 UV)
{
	gl_Position = ViewProjection * Model * vec4(position, 0);
	UV = uv;
}

//------------------------------------------------------------------------------
/**
	Ubershader for standard geometry
*/
shader
void
psUber(
	in vec3 Tangent,
	in vec3 Normal,
	in vec3 Binormal,
	in vec2 UV,
	in vec3 WorldViewVec,
	[color0] out vec4 Albedo,
	[color1] out vec3 Normals,
	[color2] out vec4 Material)
{
	vec2 seed = gl_FragCoord.xy * RenderTargetDimensions[0].zw;
	float dither = hash12(seed);
	if (dither < DitherFactor)
		discard;

	vec4 albedo = 		calcColor(sample2D(AlbedoMap, MaterialSampler, UV)) * MatAlbedoIntensity;
	vec4 material = 	calcMaterial(sample2D(ParameterMap, MaterialSampler, UV));
	vec4 normals = 		sample2D(NormalMap, NormalSampler, UV);

	vec3 bumpNormal = normalize(calcBump(Tangent, Binormal, Normal, normals));

	Albedo = albedo;
	Normals = bumpNormal;
	Material = material;
}

//------------------------------------------------------------------------------
/**
	Ubershader for standard geometry.
	Tests for alpha clipping
*/
shader
void
psUberAlphaTest(
	in vec3 Tangent,
	in vec3 Normal,
	in vec3 Binormal,
	in vec2 UV,
	in vec3 WorldViewVec,
	[color0] out vec4 Albedo,
	[color1] out vec3 Normals,
	[color2] out vec4 Material)
{
	vec2 seed = gl_FragCoord.xy * RenderTargetDimensions[0].zw;
	vec3 rnd = vec3(hash12(seed) + hash12(seed + 0.59374) - 0.5);
	float dither = (rnd.z + rnd.x + rnd.y) * DitherFactor;
	if (dither > 1.0f)
		discard;

	vec4 albedo = 		calcColor(sample2D(AlbedoMap, MaterialSampler, UV)) * MatAlbedoIntensity;
	if (albedo.a < AlphaSensitivity) { discard; return; }
	vec4 material = 	calcMaterial(sample2D(ParameterMap, MaterialSampler, UV));
	vec4 normals = 		sample2D(NormalMap, NormalSampler, UV);
	
	vec3 bumpNormal = normalize(calcBump(Tangent, Binormal, Normal, normals));

	Albedo = albedo;
	Normals = bumpNormal;
	Material = material;
}

//------------------------------------------------------------------------------
/**
	Ubershader for standard geometry
*/
shader
void
psUberVertexColor(
	in vec3 Tangent,
	in vec3 Normal,
	in vec3 Binormal,
	in vec2 UV,
	in vec4 Color,
	in vec3 WorldViewVec,
	[color0] out vec4 Albedo,
	[color1] out vec3 Normals,
	[color2] out vec4 Material)
{
	vec2 seed = gl_FragCoord.xy * RenderTargetDimensions[0].zw;
	vec3 rnd = vec3(hash12(seed) + hash12(seed + 0.59374) - 0.5);
	float dither = (rnd.z + rnd.x + rnd.y) * DitherFactor;
	if (dither > 1.0f)
		discard;

	vec4 albedo = 		calcColor(sample2D(AlbedoMap, MaterialSampler, UV)) * MatAlbedoIntensity * Color;
	vec4 material = 	calcMaterial(sample2D(ParameterMap, MaterialSampler, UV));
	vec4 normals = 		sample2D(NormalMap, NormalSampler, UV);
	
	vec3 bumpNormal = normalize(calcBump(Tangent, Binormal, Normal, normals));

	Albedo = albedo;
	Normals = bumpNormal;
	Material = material;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
psUberAlpha(in vec3 ViewSpacePos,
	in vec3 Tangent,
	in vec3 Normal,
	in vec3 Binormal,
	in vec2 UV,
	[color0] out vec4 Albedo,
	[color1] out vec3 Normals,
	[color2] out vec4 Material)
{
	vec2 seed = gl_FragCoord.xy * RenderTargetDimensions[0].zw;
	vec3 rnd = vec3(hash12(seed) + hash12(seed + 0.59374) - 0.5);
	float dither = (rnd.z + rnd.x + rnd.y) * DitherFactor;
	if (dither > 1.0f)
		discard;

	vec4 albedo = 		calcColor(sample2D(AlbedoMap, MaterialSampler, UV)) * MatAlbedoIntensity;
	if (albedo.a < AlphaSensitivity) { discard; return; }
	vec4 material = 	calcMaterial(sample2D(ParameterMap, MaterialSampler, UV));
	vec4 normals = 		sample2D(NormalMap, NormalSampler, UV);

	vec3 bumpNormal = normalize(calcBump(Tangent, Binormal, Normal, normals));

	Albedo = vec4(albedo.rgb, albedo.a * AlphaBlendFactor);
	Normals = bumpNormal;
	Material = material;
}

//------------------------------------------------------------------------------
/**
*/
shader
void
psBillboard(in vec2 UV,
			[color0] out vec4 Albedo)
{
	// get diffcolor
	vec4 diffColor = calcColor(sample2D(AlbedoMap, MaterialSampler, UV));

	Albedo = diffColor;
}

#endif
