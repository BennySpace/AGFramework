#include "ObjModelLoader.h"
#include "Assets/AssetPathUtils.h"
#include "../Utils/StringUtils.h"

#pragma warning(push)
#pragma warning(disable : 4244)
#include <assimp/Importer.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#pragma warning(pop)

#include <DirectXMath.h>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

using namespace DirectX;

namespace
{
std::wstring GetBasePathWide(const std::wstring &filename)
{
	const size_t lastSlash = filename.find_last_of(L"\\/");
	return lastSlash == std::wstring::npos ? std::wstring() : filename.substr(0, lastSlash);
}

std::wstring ResolveAssimpPath(const std::wstring &baseDirectory, const char *path)
{
	if (path == nullptr || path[0] == '\0')
	{
		return std::wstring();
	}

	const std::wstring widePath = StringUtils::Utf8ToWide(path);
	if (AssetPathUtils::FileExists(widePath))
	{
		return widePath;
	}

	return AssetPathUtils::JoinPath(baseDirectory, widePath);
}

std::wstring OpenModeToWide(const char *mode)
{
	if (mode == nullptr || mode[0] == '\0')
	{
		return L"rb";
	}

	std::wstring wideMode;
	while (*mode != '\0')
	{
		wideMode.push_back(static_cast<wchar_t>(*mode));
		++mode;
	}

	return wideMode;
}

class WideFileIOStream final : public Assimp::IOStream
{
  public:
	explicit WideFileIOStream(FILE *file) : m_file(file)
	{
		std::fseek(m_file, 0, SEEK_END);
		m_fileSize = static_cast<size_t>(std::ftell(m_file));
		std::fseek(m_file, 0, SEEK_SET);
	}

	~WideFileIOStream() override
	{
		if (m_file != nullptr)
		{
			std::fclose(m_file);
		}
	}

	size_t Read(void *buffer, size_t size, size_t count) override
	{
		return std::fread(buffer, size, count, m_file);
	}

	size_t Write(const void *buffer, size_t size, size_t count) override
	{
		return std::fwrite(buffer, size, count, m_file);
	}

	aiReturn Seek(size_t offset, aiOrigin origin) override
	{
		int whence = SEEK_SET;
		switch (origin)
		{
			case aiOrigin_SET:
				whence = SEEK_SET;
				break;
			case aiOrigin_CUR:
				whence = SEEK_CUR;
				break;
			case aiOrigin_END:
				whence = SEEK_END;
				break;
			default:
				return aiReturn_FAILURE;
		}

		return std::fseek(m_file, static_cast<long>(offset), whence) == 0 ? aiReturn_SUCCESS : aiReturn_FAILURE;
	}

	size_t Tell() const override
	{
		return static_cast<size_t>(std::ftell(m_file));
	}

	size_t FileSize() const override
	{
		return m_fileSize;
	}

	void Flush() override
	{
		std::fflush(m_file);
	}

  private:
	FILE *m_file = nullptr;
	size_t m_fileSize = 0;
};

class Utf8AwareIOSystem final : public Assimp::IOSystem
{
  public:
	explicit Utf8AwareIOSystem(const std::wstring &baseDirectory) : m_baseDirectory(baseDirectory) {}

	bool Exists(const char *path) const override
	{
		const std::wstring resolvedPath = ResolveAssimpPath(m_baseDirectory, path);
		return AssetPathUtils::FileExists(resolvedPath);
	}

	char getOsSeparator() const override
	{
		return '\\';
	}

	Assimp::IOStream *Open(const char *path, const char *mode) override
	{
		const std::wstring resolvedPath = ResolveAssimpPath(m_baseDirectory, path);
		if (resolvedPath.empty())
		{
			return nullptr;
		}

		FILE *file = nullptr;
		const std::wstring wideMode = OpenModeToWide(mode);
		if (_wfopen_s(&file, resolvedPath.c_str(), wideMode.c_str()) != 0 || file == nullptr)
		{
			return nullptr;
		}

		return new WideFileIOStream(file);
	}

	void Close(Assimp::IOStream *file) override
	{
		delete file;
	}

  private:
	std::wstring m_baseDirectory;
};

std::string GetTexturePath(const aiMaterial *material, aiTextureType textureType, const std::string &basePath)
{
	aiString texturePath;
	if (material->GetTexture(textureType, 0, &texturePath) == aiReturn_SUCCESS)
	{
		return AssetPathUtils::JoinPath(basePath, texturePath.C_Str());
	}

	return std::string();
}
} // namespace

std::vector<ObjModelLoader::MeshData> ObjModelLoader::Load(const std::string &filename) const
{
	if (!AssetPathUtils::FileExists(filename))
	{
		throw std::runtime_error("Required model asset not found: " + filename);
	}

	Assimp::Importer importer;
	const std::wstring wideFilename = StringUtils::Utf8ToWide(filename);
	importer.SetIOHandler(new Utf8AwareIOSystem(GetBasePathWide(wideFilename)));
	const aiScene *scene =
	    importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ConvertToLeftHanded |
	                                    aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);

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
		const aiMesh *sourceMesh = scene->mMeshes[meshIndex];
		MeshData meshData;
		meshData.Vertices.reserve(sourceMesh->mNumVertices);
		meshData.Indices32.reserve(sourceMesh->mNumFaces * 3);

		if (sourceMesh->mMaterialIndex < scene->mNumMaterials)
		{
			const aiMaterial *material = scene->mMaterials[sourceMesh->mMaterialIndex];

			aiString materialName;
			if (material->Get(AI_MATKEY_NAME, materialName) == aiReturn_SUCCESS)
			{
				meshData.MaterialName = materialName.C_Str();
			}

			meshData.DiffuseTexturePath = GetTexturePath(material, aiTextureType_DIFFUSE, basePath);
			meshData.NormalTexturePath = GetTexturePath(material, aiTextureType_NORMALS, basePath);
			if (meshData.NormalTexturePath.empty())
			{
				meshData.NormalTexturePath = GetTexturePath(material, aiTextureType_HEIGHT, basePath);
			}
			meshData.OrmTexturePath = GetTexturePath(material, aiTextureType_UNKNOWN, basePath);
			meshData.OpacityTexturePath = GetTexturePath(material, aiTextureType_OPACITY, basePath);
			aiColor4D diffuseAlbedo;
			if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseAlbedo) == aiReturn_SUCCESS)
			{
				meshData.DiffuseAlbedo = XMFLOAT4(diffuseAlbedo.r, diffuseAlbedo.g, diffuseAlbedo.b, diffuseAlbedo.a);
			}

			float opacity = 1.0f;
			if (material->Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS)
			{
				meshData.DiffuseAlbedo.w *= std::clamp(opacity, 0.0f, 1.0f);
			}
			meshData.HasAlphaCutout = !meshData.OpacityTexturePath.empty() || meshData.DiffuseAlbedo.w < 1.0f;
		}

		for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex)
		{
			GeometryGenerator::Vertex vertex;

			const aiVector3D &position = sourceMesh->mVertices[vertexIndex];
			vertex.Position = XMFLOAT3(position.x, position.y, position.z);

			if (sourceMesh->HasNormals())
			{
				const aiVector3D &normal = sourceMesh->mNormals[vertexIndex];
				vertex.Normal = XMFLOAT3(normal.x, normal.y, normal.z);
			}
			else
			{
				vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
			}

			if (sourceMesh->HasTangentsAndBitangents())
			{
				const aiVector3D &tangent = sourceMesh->mTangents[vertexIndex];
				vertex.TangentU = XMFLOAT3(tangent.x, tangent.y, tangent.z);
			}
			else
			{
				vertex.TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);
			}

			if (sourceMesh->HasTextureCoords(0))
			{
				const aiVector3D &texCoord = sourceMesh->mTextureCoords[0][vertexIndex];
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
			const aiFace &face = sourceMesh->mFaces[faceIndex];
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
