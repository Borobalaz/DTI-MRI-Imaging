#include "ModelLoader.h"

#include <filesystem>
#include <iostream>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>

#include "ImportedGeometry.h"
#include "Material.h"
#include "Mesh.h"
#include "Texture2D.h"

namespace
{
  glm::vec3 ClampColor(const glm::vec3& color)
  {
    return glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));
  }

  std::shared_ptr<Texture> TryLoadTexture(const aiScene* scene,
                                          const aiMesh* mesh,
                                          const std::filesystem::path& modelDirectory,
                                          aiTextureType type,
                                          const char* textureTypeName)
  {
    if (scene == nullptr || mesh == nullptr || mesh->mMaterialIndex >= scene->mNumMaterials)
    {
      return nullptr;
    }

    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    if (material == nullptr || material->GetTextureCount(type) == 0)
    {
      return nullptr;
    }

    aiString texturePath;
    if (material->GetTexture(type, 0, &texturePath) != aiReturn_SUCCESS)
    {
      return nullptr;
    }

    const std::string rawTexturePath = texturePath.C_Str();
    if (rawTexturePath.empty())
    {
      std::cout << "Skipping empty " << textureTypeName << " texture path." << std::endl;
      return nullptr;
    }

    if (!rawTexturePath.empty() && rawTexturePath[0] == '*')
    {
      std::cout << "Embedded " << textureTypeName
                << " textures are not supported yet: " << rawTexturePath
                << std::endl;
      return nullptr;
    }

    const std::filesystem::path relativeTexturePath(rawTexturePath);
    if (relativeTexturePath.is_absolute())
    {
      std::cout << "Skipping absolute " << textureTypeName
                << " texture path: " << rawTexturePath << std::endl;
      return nullptr;
    }

    const std::filesystem::path resolvedPath = modelDirectory / relativeTexturePath;
    return std::make_shared<Texture2D>(resolvedPath.string());
  }

  void ApplyPhongMaterialProperties(const aiScene* scene,
                                    const aiMesh* mesh,
                                    Material& destination)
  {
    if (scene == nullptr || mesh == nullptr || mesh->mMaterialIndex >= scene->mNumMaterials)
    {
      return;
    }

    aiMaterial* sourceMaterial = scene->mMaterials[mesh->mMaterialIndex];
    if (sourceMaterial == nullptr)
    {
      return;
    }

    aiColor3D ambient(0.2f, 0.2f, 0.2f);
    aiColor3D diffuse(0.8f, 0.8f, 0.8f);
    aiColor3D specular(1.0f, 1.0f, 1.0f);
    float materialShininess = 32.0f;

    sourceMaterial->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
    sourceMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
    sourceMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specular);
    sourceMaterial->Get(AI_MATKEY_SHININESS, materialShininess);

    destination.SetAmbientColor(ClampColor(glm::vec3(ambient.r, ambient.g, ambient.b)));
    destination.SetDiffuseColor(ClampColor(glm::vec3(diffuse.r, diffuse.g, diffuse.b)));
    destination.SetSpecularColor(ClampColor(glm::vec3(specular.r, specular.g, specular.b)));
    destination.SetShininess(materialShininess);
  }

  std::shared_ptr<Mesh> BuildMesh(const aiScene* scene,
                                  const aiMesh* sourceMesh,
                                  const std::filesystem::path& modelDirectory,
                                  const std::shared_ptr<Shader>& shader)
  {
    if (sourceMesh == nullptr)
    {
      return nullptr;
    }

    std::vector<Vertex> vertices;
    vertices.reserve(sourceMesh->mNumVertices);

    for (unsigned int i = 0; i < sourceMesh->mNumVertices; ++i)
    {
      const aiVector3D& aiPosition = sourceMesh->mVertices[i];

      glm::vec3 normal(0.0f, 0.0f, 0.0f);
      if (sourceMesh->HasNormals())
      {
        const aiVector3D& aiNormal = sourceMesh->mNormals[i];
        normal = glm::vec3(aiNormal.x, aiNormal.y, aiNormal.z);
      }

      glm::vec2 texCoord(0.0f, 0.0f);
      if (sourceMesh->HasTextureCoords(0))
      {
        const aiVector3D& aiTexCoord = sourceMesh->mTextureCoords[0][i];
        texCoord = glm::vec2(aiTexCoord.x, aiTexCoord.y);
      }

      vertices.push_back(Vertex{
        glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z),
        normal,
        texCoord
      });
    }

    std::vector<unsigned int> indices;
    for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex)
    {
      const aiFace& face = sourceMesh->mFaces[faceIndex];
      for (unsigned int index = 0; index < face.mNumIndices; ++index)
      {
        indices.push_back(face.mIndices[index]);
      }
    }

    std::shared_ptr<ImportedGeometry> geometry =
      std::make_shared<ImportedGeometry>(std::move(vertices), std::move(indices));

    std::shared_ptr<Material> material = std::make_shared<Material>(shader);
    ApplyPhongMaterialProperties(scene, sourceMesh, *material);

    const std::shared_ptr<Texture> diffuseTexture =
      TryLoadTexture(scene, sourceMesh, modelDirectory, aiTextureType_DIFFUSE, "diffuse");
    if (diffuseTexture)
    {
      material->SetDiffuseTexture(diffuseTexture);
    }

    const std::shared_ptr<Texture> specularTexture =
      TryLoadTexture(scene, sourceMesh, modelDirectory, aiTextureType_SPECULAR, "specular");
    if (specularTexture)
    {
      material->SetSpecularTexture(specularTexture);
    }

    return std::make_shared<Mesh>(geometry, material);
  }

  void ProcessNode(const aiScene* scene,
                   const aiNode* node,
                   const std::filesystem::path& modelDirectory,
                   const std::shared_ptr<Shader>& shader,
                   const std::shared_ptr<GameObject>& gameObject)
  {
    if (scene == nullptr || node == nullptr || !gameObject)
    {
      return;
    }

    for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex)
    {
      const unsigned int sceneMeshIndex = node->mMeshes[meshIndex];
      if (sceneMeshIndex >= scene->mNumMeshes)
      {
        continue;
      }

      std::shared_ptr<Mesh> mesh =
        BuildMesh(scene, scene->mMeshes[sceneMeshIndex], modelDirectory, shader);
      if (mesh)
      {
        gameObject->AddMesh(mesh);
      }
    }

    for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
    {
      ProcessNode(scene,
                  node->mChildren[childIndex],
                  modelDirectory,
                  shader,
                  gameObject);
    }
  }
}

std::shared_ptr<GameObject> ModelLoader::LoadGameObject(const std::string& modelPath,
                                                         const std::shared_ptr<Shader>& shader,
                                                         bool flipUv)
{
  if (!shader)
  {
    std::cout << "ModelLoader requires a valid shader instance." << std::endl;
    return nullptr;
  }

  Assimp::Importer importer;
  unsigned int postProcessFlags =
    aiProcess_Triangulate |
    aiProcess_GenSmoothNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_JoinIdenticalVertices |
    aiProcess_ImproveCacheLocality;

  if (flipUv)
  {
    postProcessFlags |= aiProcess_FlipUVs;
  }

  const aiScene* scene = importer.ReadFile(modelPath, postProcessFlags);
  if (scene == nullptr || scene->mRootNode == nullptr)
  {
    std::cout << "Failed to import model: " << modelPath << std::endl;
    std::cout << "Assimp error: " << importer.GetErrorString() << std::endl;
    return nullptr;
  }

  std::shared_ptr<GameObject> gameObject = std::make_shared<GameObject>();
  const std::filesystem::path modelDirectory =
    std::filesystem::path(modelPath).parent_path();

  ProcessNode(scene, scene->mRootNode, modelDirectory, shader, gameObject);
  return gameObject;
}
