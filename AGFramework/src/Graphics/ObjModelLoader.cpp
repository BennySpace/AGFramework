#include "ObjModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <DirectXMath.h>
#include <stdexcept>

using namespace DirectX;

namespace
{
	std::string JoinPath(const std::string& basePath, const std::string& relativePath)
	{
		if (relativePath.empty())
		{
			return relativePath;
		}

		if (relativePath.size() > 1 && relativePath[1] == ':')
		{
			return relativePath;
		}

		if (relativePath[0] == '\\' || relativePath[0] == '/')
		{
			return relativePath;
		}

		if (basePath.empty())
		{
			return relativePath;
		}

		if (basePath.back() == '\\' || basePath.back() == '/')
		{
			return basePath + relativePath;
		}

		return basePath + "\\" + relativePath;
	}
}

std::vector<ObjModelLoader::MeshData> ObjModelLoader::Load(const std::string& filename) const
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		filename,
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_ConvertToLeftHanded |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace);

	if (scene == nullptr || scene->mRootNode == nullptr)
	{
		throw std::runtime_error(importer.GetErrorString());
	}

	const size_t lastSlash = filename.find_last_of("\\/");
	const std::string basePath = (lastSlash == std::string::npos) ? std::string() : filename.substr(0, lastSlash);

	std::vector<MeshData> meshes;
	meshes.reserve(scene->mNumMeshes);

	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		const aiMesh* sourceMesh = scene->mMeshes[meshIndex];
		MeshData meshData;
		meshData.Vertices.reserve(sourceMesh->mNumVertices);
		meshData.Indices32.reserve(sourceMesh->mNumFaces * 3);

		if (sourceMesh->mMaterialIndex < scene->mNumMaterials)
		{
			const aiMaterial* material = scene->mMaterials[sourceMesh->mMaterialIndex];

			aiString materialName;
			if (material->Get(AI_MATKEY_NAME, materialName) == aiReturn_SUCCESS)
			{
				meshData.MaterialName = materialName.C_Str();
			}

			aiString diffusePath;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &diffusePath) == aiReturn_SUCCESS)
			{
				meshData.DiffuseTexturePath = JoinPath(basePath, diffusePath.C_Str());
			}

			aiString opacityPath;
			meshData.HasAlphaCutout =
				material->GetTexture(aiTextureType_OPACITY, 0, &opacityPath) == aiReturn_SUCCESS;
		}

		for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex)
		{
			GeometryGenerator::Vertex vertex;

			const aiVector3D& position = sourceMesh->mVertices[vertexIndex];
			vertex.Position = XMFLOAT3(position.x, position.y, position.z);

			if (sourceMesh->HasNormals())
			{
				const aiVector3D& normal = sourceMesh->mNormals[vertexIndex];
				vertex.Normal = XMFLOAT3(normal.x, normal.y, normal.z);
			}
			else
			{
				vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
			}

			if (sourceMesh->HasTangentsAndBitangents())
			{
				const aiVector3D& tangent = sourceMesh->mTangents[vertexIndex];
				vertex.TangentU = XMFLOAT3(tangent.x, tangent.y, tangent.z);
			}
			else
			{
				vertex.TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);
			}

			if (sourceMesh->HasTextureCoords(0))
			{
				const aiVector3D& texCoord = sourceMesh->mTextureCoords[0][vertexIndex];
				vertex.TexC = XMFLOAT2(texCoord.x, texCoord.y);
			}
			else
			{
				vertex.TexC = XMFLOAT2(0.0f, 0.0f);
			}

			meshData.Vertices.push_back(vertex);
		}

		for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex)
		{
			const aiFace& face = sourceMesh->mFaces[faceIndex];
			if (face.mNumIndices != 3)
			{
				continue;
			}

			meshData.Indices32.push_back(face.mIndices[0]);
			meshData.Indices32.push_back(face.mIndices[1]);
			meshData.Indices32.push_back(face.mIndices[2]);
		}

		meshes.push_back(std::move(meshData));
	}

	return meshes;
}
