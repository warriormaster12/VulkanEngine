#include "Include/properties_panel.h"


#include "imgui.h"
#include "logger.h"
#include "material_manager.h"
#include "mesh_component.h"
#include "material_component.h"
#include "transform_component.h"
#include "project_manager.h"
#include "unordered_finder.h"
#include <memory>
#include <unordered_map>


static std::vector<std::string> components;
struct MaterialProperties
{
    float albedo[4]  = {1.0f,1.0f,1.0f,1.0f};
    std::string albedoTexture;
};
static std::unordered_map<Material*, MaterialProperties> materials;
void PropertiesPanel::ShowPropertiesPanelWindow(Entity& entity,const std::string& entityName)
{
    static bool isActive = false;
    ImGui::Begin(entityName.c_str(), &isActive, ImGuiWindowFlags_NoCollapse);
    if(ImGui::BeginMenu("Add component"))
    {
        if(ImGui::MenuItem("Transform3D"))
        {
            entity.AddComponent(std::make_unique<Transform3D>(), "Transform3D");
        }
        if(ImGui::MenuItem("Mesh"))
        {
            entity.AddComponent(std::make_unique<MeshComponent>(), "Mesh");
            
        }
        if(ImGui::MenuItem("Material"))
        {
            entity.AddComponent(std::make_unique<MaterialComponent>(), "Material");
        }
        
        ImGui::EndMenu();
    }
    components = entity.GetComponents();
    
    if(components.size() > 0)
    {
        for(auto& currentComponent : components)
        {
            if(currentComponent == "Transform3D")
            {
                ImGui::BeginChild(currentComponent.c_str(), ImVec2(ImGui::GetWindowContentRegionWidth(), 256), true);
                ImGui::Text("Transform");
                //for some reason I need to supply vec4
                static float vec3T[3];
                static float vec3R[3];
                static float vec3S[3] = {1.0f, 1.0f, 1.0f};
                static glm::mat4 transform;
                static glm::mat4 lastTransform;
                ImGui::DragFloat3("translation", vec3T, 0.2f, -FLT_MAX, FLT_MAX);
                ImGui::DragFloat3("rotation", vec3R, 0.2f, -360, 360);
                ImGui::DragFloat3("scale", vec3S, 0.2f, -FLT_MAX, FLT_MAX);
                auto* transformComponent = static_cast<Transform3D*>(entity.GetComponent(currentComponent));
                transformComponent->UpdateTranslation(glm::vec3(vec3T[0], vec3T[1], vec3T[2]));
                transformComponent->UpdateRotation(glm::vec3(glm::radians(vec3R[0]), glm::radians(vec3R[1]), glm::radians(vec3R[2])));
                transformComponent->UpdateScale(glm::vec3(vec3S[0], vec3S[1], vec3S[2]));
                transformComponent->UpdateTransform();
                transform = transformComponent->GetTransform();
                if(transform != lastTransform)
                {
                    if(static_cast<MeshComponent*>(entity.GetComponent("Mesh")) != nullptr)
                    {
                        if(static_cast<MaterialComponent*>(entity.GetComponent("Material")) != nullptr)
                        {
                            static_cast<MeshComponent&>(*entity.GetComponent("Mesh")).UpdateCurrentMesh(entityName, static_cast<MaterialComponent&>(*entity.GetComponent("Material")).GetMaterialName(), transformComponent);
                        }
                        else {
                            static_cast<MeshComponent&>(*entity.GetComponent("Mesh")).UpdateCurrentMesh(entityName, "");
                        }
                    }
                    
                    lastTransform = transform;
                }
                ImGui::EndChild();
            }
            if(currentComponent == "Mesh")
            {
                ImGui::BeginChild(currentComponent.c_str(), ImVec2(ImGui::GetWindowContentRegionWidth(), 256), true);
                static std::string filePath;
                filePath.resize(64);
                if(ImGui::BeginMenu("Select material"))
                {
                    for(auto& currentMat : static_cast<MaterialComponent&>(*entity.GetComponent("Material")).GetMaterials())
                    {
                        if(ImGui::MenuItem(currentMat.c_str()))
                        {
                            auto& meshComponent = static_cast<MeshComponent&>(*entity.GetComponent(currentComponent));
                            
                            if(static_cast<MaterialComponent*>(entity.GetComponent("Material")) != nullptr)
                            {
                                
                                if(static_cast<Transform3D*>(entity.GetComponent("Transform3D")) != nullptr)
                                {
                                    meshComponent.UpdateCurrentMesh(entityName, currentMat, static_cast<Transform3D*>(entity.GetComponent("Transform3D")));
                                }
                                else {
                                    meshComponent.UpdateCurrentMesh(entityName, currentMat);
                                }
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Meshes"))
                {
                    auto& meshComponent = static_cast<MeshComponent&>(*entity.GetComponent(currentComponent));
                    for(auto& currentMesh : ProjectManager::ListMeshes())
                    {
                        if(ImGui::MenuItem(currentMesh.c_str()))
                        {
                            if(static_cast<Transform3D*>(entity.GetComponent("Transform3D")) != nullptr)
                            {
                                if(static_cast<MaterialComponent*>(entity.GetComponent("Material")) != nullptr)
                                {
                                    meshComponent.AddMesh(ProjectManager::GetMesh(currentMesh), entityName,static_cast<MaterialComponent&>(*entity.GetComponent("Material")).GetMaterialName(), static_cast<Transform3D*>(entity.GetComponent("Transform3D")));
                                }
                                else
                                {
                                    meshComponent.AddMesh(ProjectManager::GetMesh(currentMesh), entityName,"", static_cast<Transform3D*>(entity.GetComponent("Transform3D")));
                                }
                            }
                            else {
                                meshComponent.AddMesh(ProjectManager::GetMesh(currentMesh), entityName);
                            }
                        } 
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndChild();
            }
            if(currentComponent == "Material")
            {
                ImGui::BeginChild(currentComponent.c_str(), ImVec2(ImGui::GetWindowContentRegionWidth(), 256), true);
                static std::string filePath;
                static bool editMaterial = false;
                static std::string editMaterialName;
                filePath.resize(64);
                if(ImGui::BeginMenu("materials"))
                {
                    static bool inputText = false;
                    for(auto& currentMaterial : static_cast<MaterialComponent&>(*entity.GetComponent(currentComponent)).GetMaterials())
                    {
                        if(currentMaterial != "default material")
                        {
                            if(ImGui::MenuItem(currentMaterial.c_str()))
                            {
                                editMaterial = true;
                                editMaterialName = currentMaterial;
                                static_cast<MaterialComponent&>(*entity.GetComponent(currentComponent)).SetMaterialName(editMaterialName);
                            }
                        }
                    }
                    if(ImGui::Button("create new"))
                    {
                        inputText = true;
                    }
                    if(inputText)
                    {
                        static std::string materialName;
                        materialName.resize(32);
                        if(ImGui::InputText("material name", materialName.data(), materialName.size(), ImGuiInputTextFlags_EnterReturnsTrue))
                        {
                            inputText = false;
                            static_cast<MaterialComponent&>(*entity.GetComponent(currentComponent)).CreateMaterial(materialName);
                            if(FindUnorderedMap(&static_cast<MaterialComponent&>(*entity.GetComponent(currentComponent)).GetMaterial(materialName), materials) == nullptr)
                            {
                                materials[&static_cast<MaterialComponent&>(*entity.GetComponent(currentComponent)).GetMaterial(materialName)];
                            }
                            editMaterialName = materialName;
                            static_cast<MaterialComponent&>(*entity.GetComponent(currentComponent)).SetMaterialName(editMaterialName);
                            editMaterial = true;
                            materialName = "";
                        }
                    }
                    ImGui::EndMenu();
                }
                if(editMaterial)
                {
                    auto& currentMat = static_cast<MaterialComponent&>(*entity.GetComponent(currentComponent)).GetMaterial(editMaterialName);
                    ImGui::BeginChild(editMaterialName.c_str(), ImVec2(ImGui::GetWindowContentRegionWidth(), 256), true);
                    auto& mat = *FindUnorderedMap(&static_cast<MaterialComponent&>(*entity.GetComponent(currentComponent)).GetMaterial(editMaterialName), materials); 
                    if(ImGui::ColorEdit4("albedo", mat.albedo))
                    {
                        currentMat.SetColor(glm::vec4(mat.albedo[0], mat.albedo[1],mat.albedo[2],mat.albedo[3]));
                    }
                    mat.albedoTexture.resize(32);
                    if(ImGui::InputText("albedo texture", mat.albedoTexture.data(), mat.albedoTexture.size(), ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        MaterialManager::GetMaterial(editMaterialName).textureCheck.textures[0] = 1;
                        currentMat.SetTextures({mat.albedoTexture, "", "", "", ""});
                        MaterialManager::AddTextures(editMaterialName);
                    }
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }
        }
    }

    ImGui::End();
}