#version 460

layout (location = 0) in vec3 inPos;

layout(set = 0, binding = 0) uniform LightMatrixData{   
	mat4 lightSpaceMatrix;
} lightMatrixData;

struct ObjectData{
	mat4 model;
}; 

//all object matrices
layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{   
	ObjectData objects[];
} objectBuffer;
 
void main()
{
	gl_Position = lightMatrixData.lightSpaceMatrix * objectBuffer.objects[gl_InstanceIndex].model * vec4(inPos, 1.0);
}
