#pragma once 

#include "mesh.h"

#include <iostream>
#include <string>
#include <memory>



struct RenderObject
{
    std::shared_ptr<Mesh> p_mesh = std::make_shared<Mesh>();
    std::string material;

    glm::mat4 transformMatrix;
};

struct DrawCall
{
    std::shared_ptr<Mesh> p_mesh;
    std::string* p_material = nullptr;

    uint32_t descriptorSetCount;

    glm::mat4 transformMatrix;
	/* Start index of the first object to draw */
	uint32_t index;
	/* Number of objects to draw, starting from 'index' */
	uint32_t count;
};

struct ObjectManager 
{
    static void Init();

    static void PushObjectToQueue(RenderObject& object);

    static void RenderObjects();

    static void Destroy();
};