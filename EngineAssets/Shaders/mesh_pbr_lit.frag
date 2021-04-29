//glsl version 4.5
#version 450

#extension GL_EXT_nonuniform_qualifier : require

#define SHADOW_MAP_CASCADE_COUNT 4
#define ambientShadow 0.3
//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 WorldPos;
layout (location = 3) in vec3 Normal;
layout (location = 4) in vec3 inViewPos;
//output write
layout (location = 0) out vec4 outFragColor;

struct Cascade{
	vec4 cascadeSplits;
	mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];

    uint cascadeSplitsDebug;
};

layout(set = 0, binding = 0) uniform  CameraBuffer{   
    mat4 view;
    mat4 proj;
	mat4 viewproj;
	vec4 camPos; // vec3

    Cascade cascadeData;
} cameraData;

struct DirectionLight{
    vec4 direction; //vec3
    vec4 color; //vec3
    vec4 intensity; //float
};
struct PointLight
{
	vec4 position; // vec3
	vec4 color; // vec3
    vec4 radius; //float
    vec4 intensity; //float
};



layout(std430, set = 0, binding = 1)  readonly buffer SceneData{ 
    vec4 plightCount; //int
    DirectionLight dLight;
	PointLight pointLights[];
} sceneData;

layout(set = 2, binding = 0) uniform MaterialData{
	vec4 albedo; // vec4
	vec4 metallic; // float
	vec4 roughness; // float
	vec4 ao; // float

    vec4 emissionColor; //vec3
    vec4 emissionPower; // float
} materialData;

layout (set = 2, binding = 1) uniform sampler2DArray shadowMap;
layout(set = 2, binding = 2) uniform sampler2D textureMaps[];

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);


const float PI = 3.14159265359;
vec3 getNormalFromMap(vec3 NormalMap)
{
    vec3 tangentNormal = NormalMap * 2.0 - 1.0;

    vec3 Q1  = dFdx(WorldPos);
    vec3 Q2  = dFdy(WorldPos);
    vec2 st1 = dFdx(texCoord);
    vec2 st2 = dFdy(texCoord);

    vec3 N   = normalize(Normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = r*r / 8.0;

    float num = NdotV;
    float denom = 1 / (NdotV* (1.0 - k) + k);

    return num * denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(float nDotV, float nDotL, float rough)
{
    float ggx2  = GeometrySchlickGGX(nDotV, rough);
    float ggx1  = GeometrySchlickGGX(nDotL, rough);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 calcPointLight(int index, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo, float rough, float metal, vec3 F0,  float viewDistance);

// ----------------------------------------------------------------------------
vec3 calcDirLight(DirectionLight light, vec3 normal, vec3 viewDir, vec3 albedo, float rough, float metal, vec3 F0);
// ----------------------------------------------------------------------------

uint globalCascadeIndex;

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
	float shadow = 1.0;
	float bias = 0.005;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		float dist = texture(shadowMap, vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
			shadow = ambientShadow;
		}
	}
	return shadow;

}

float filterPCF(vec4 sc, uint cascadeIndex)
{
	ivec2 texDim = textureSize(shadowMap, 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

void main()
{
	vec4 albedo =  pow(texture(textureMaps[nonuniformEXT(0)], texCoord).rgba, vec4(2.2));
    vec4 emission = texture(textureMaps[nonuniformEXT(2)], texCoord).rgba * materialData.emissionColor * float(materialData.emissionPower);
    float ao = texture(textureMaps[nonuniformEXT(3)], texCoord).r;
    float metallic;
    float roughness;
    if(texture(textureMaps[nonuniformEXT(4)], texCoord).rgba != vec4(0.0))
    {
        metallic = texture(textureMaps[nonuniformEXT(4)], texCoord).b;
        roughness = texture(textureMaps[nonuniformEXT(4)], texCoord).g;
    }
    else
    {
        metallic = texture(textureMaps[nonuniformEXT(5)], texCoord).r;
        roughness = texture(textureMaps[nonuniformEXT(6)], texCoord).r;
    }

    // this is for objects that have a texture loaded
    if (albedo.a > 0.1)
    {
        albedo *= materialData.albedo;
    }
    // this is for objects that have an empty texture
    else if (albedo.rgba == vec4(0.0f))
    {
        albedo += materialData.albedo;
    }

    if(ao > 0.1f)
    {
        ao *= float(materialData.ao);
    }
    else
    {
        ao += float(materialData.ao);
    }
    vec3 N = texture(textureMaps[nonuniformEXT(1)], texCoord).xyz;
    if(N.x > 0.1f || N.y > 0.1f || N.z > 0.1f)
    {
        N = getNormalFromMap(texture(textureMaps[nonuniformEXT(1)], texCoord).xyz);
    }
    else
    {
        N = normalize(Normal);
    }
    if(metallic > 0.1)
    {
        metallic *= float(materialData.metallic);
    }
    else
    {
        metallic += float(materialData.metallic);
    }
    if(roughness > 0.1)
    {
        roughness *= float(materialData.roughness);
    }
    else
    {
        roughness += float(materialData.roughness);  
    }
    if (albedo.a < 0.1)
    {
        discard;
    }
    
    vec3 V = normalize(vec3(cameraData.camPos) - WorldPos);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, vec3(albedo), metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    vec3 radianceOut = calcDirLight(sceneData.dLight, N, V, albedo.rgb, roughness, metallic, F0);
    float viewDistance = length(vec3(cameraData.camPos) - WorldPos);
    for(int i = 0; i < int(sceneData.plightCount); ++i) 
    {
        // calculate per-light radiance
        radianceOut += calcPointLight(i, N, WorldPos, V, albedo.rgb, roughness, metallic, F0, viewDistance);
    }
    
    // ambient lighting (note that the next IBL tutorial will replace 
    // this ambient lighting with environment lighting).
    vec3 ambient = vec3(0.025)* albedo.rgb;
    ambient *= ao;
    radianceOut += ambient;
    radianceOut += emission.rgb;
    vec3 color = radianceOut;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    //color = pow(color, vec3(1.0/2.2)); 

    outFragColor = vec4(color, albedo.a);
    if(cameraData.cascadeData.cascadeSplitsDebug == 1)
    {
        switch(globalCascadeIndex) {
            case 0 : 
                outFragColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
                break;
            case 1 : 
                outFragColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
                break;
            case 2 : 
                outFragColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
                break;
            case 3 : 
                outFragColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
                break;
        }
    }

}

vec3 calcPointLight(int index, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo, float rough, float metal, vec3 F0,  float viewDistance)
{
    //Point light basics
    vec3 position = sceneData.pointLights[index].position.xyz;
    vec3 color    = sceneData.pointLights[index].color.rgb * float(sceneData.pointLights[index].intensity);
    float radius  = float(sceneData.pointLights[index].radius);

    //Stuff common to the BRDF subfunctions 
    vec3 lightDir = normalize(position - fragPos);
    vec3 halfway  = normalize(lightDir + viewDir);
    float nDotV = max(dot(normal, viewDir), 0.0);
    float nDotL = max(dot(normal, lightDir), 0.0);

    //Attenuation calculation that is applied to all
    float distance    = length(position - fragPos);
    float attenuation = pow(clamp(1 - pow((distance / radius), 4.0), 0.0, 1.0), 2.0)/(1.0  + (distance * distance) );
    vec3 radianceIn   = color * attenuation;

    //Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, halfway, rough);
    float G   = GeometrySmith(nDotV, nDotL, rough);
    vec3  F   = fresnelSchlick(max(dot(halfway,viewDir), 0.0), F0);

    //Finding specular and diffuse component
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metal;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * nDotV * nDotL;
    vec3 specular = numerator / max(denominator, 0.0000001);
    // vec3 specular = numerator / denominator;

    vec3 radiance = (kD * (albedo / PI) + specular ) * radianceIn * nDotL;

    //we do not currently support shadows
    // //shadow stuff
    // vec3 fragToLight = fragPos - position;
    // float shadow = calcPointLightShadows(depthMaps[index], fragToLight, viewDistance);
    
    // radiance *= (1.0 - shadow);

    return radiance;
}

vec3 calcDirLight(DirectionLight light, vec3 normal, vec3 viewDir, vec3 albedo, float rough, float metal, vec3 F0)
{
    uint cascadeIndex = 0;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
		if(inViewPos.z < cameraData.cascadeData.cascadeSplits[i]) {	
			cascadeIndex = i + 1;
		}
	}
    globalCascadeIndex = cascadeIndex;
    vec4 shadowCoord = (biasMat * cameraData.cascadeData.cascadeViewProjMat[cascadeIndex]) * vec4(WorldPos, 1.0);	
    //Variables common to BRDFs
    vec3 lightDir = normalize(vec3(-light.direction));
    vec3 halfway  = normalize(lightDir + viewDir);
    float nDotV = max(dot(normal, viewDir), 0.0);
    float nDotL = max(dot(normal, lightDir), 0.0);
    vec3 radianceIn = light.color.rgb * float(light.intensity);

    //Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, halfway, rough);
    float G   = GeometrySmith(nDotV, nDotL, rough);
    vec3  F   = fresnelSchlick(max(dot(halfway,viewDir), 0.0), F0);

    //Finding specular and diffuse component
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metal;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * nDotV * nDotL;
    vec3 specular = numerator / max (denominator, 0.0001);

    vec3 radiance = (kD * (albedo / PI) + specular ) * radianceIn * nDotL;
    float shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
    radiance *= shadow;

    return radiance;
}

