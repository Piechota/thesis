#include "meshLoader.h"

#include "../assimp/Importer.hpp"
#include "../assimp/scene.h"
#include "../assimp/postprocess.h"

CMeshLoader GMeshLoader;

void CMeshLoader::LoadMesh(char const* path, UINT& verticesNum, SVertexData** outVertices, UINT& indicesNum, UINT32** outIndices)
{
	static Assimp::Importer importer;
	aiScene const* scene =
		importer.ReadFile(
			path
			, aiProcess_Triangulate
			| aiProcess_ImproveCacheLocality
			| aiProcess_OptimizeMeshes
			| aiProcess_OptimizeGraph
			| aiProcess_CalcTangentSpace
		);

	if (scene && scene->HasMeshes())
	{
		if (scene->mNumMeshes > 0)
		{
			aiMesh* assimpMesh = scene->mMeshes[0];

			verticesNum = assimpMesh->mNumVertices;
			*outVertices = new SVertexData[verticesNum];

			for (UINT vertexID = 0; vertexID < verticesNum; ++vertexID)
			{
				SVertexData& vertex = (*outVertices)[vertexID];

				aiVector3D vertexData = assimpMesh->mVertices[vertexID];
				vertex.m_position[0] = vertexData.x;
				vertex.m_position[1] = vertexData.y;
				vertex.m_position[2] = vertexData.z;

				vertexData = assimpMesh->mTextureCoords[0][vertexID];
				vertex.m_uv[0] = vertexData.x;
				vertex.m_uv[1] = vertexData.y;

				vertexData = assimpMesh->mNormals[vertexID];
				vertex.m_normal[0] = vertexData.x;
				vertex.m_normal[1] = vertexData.y;
				vertex.m_normal[2] = vertexData.z;

				vertexData = assimpMesh->mTangents[vertexID];
				vertex.m_tangent[0] = vertexData.x;
				vertex.m_tangent[1] = vertexData.y;
				vertex.m_tangent[2] = vertexData.z;
			}

			indicesNum = 0;
			UINT const facesNum = assimpMesh->mNumFaces;
			for (UINT faceID = 0; faceID < facesNum; ++faceID)
			{
				aiFace face = assimpMesh->mFaces[faceID];
				indicesNum += face.mNumIndices;
			}

			*outIndices = new UINT32[indicesNum];
			UINT iID = 0;
			for (UINT faceID = 0; faceID < facesNum; ++faceID)
			{
				aiFace face = assimpMesh->mFaces[faceID];
				for (UINT indexID = 0; indexID < face.mNumIndices; ++indexID)
				{
					(*outIndices)[iID] = face.mIndices[indexID];
					++iID;
				}
			}
		}
	}
}
