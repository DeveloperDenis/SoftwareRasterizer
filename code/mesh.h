#if !defined(MESH_H_)
#define MESH_H_

#include "denis_strings.h"
//NOTE(denis): needed to load the model .obj file
#include <cstdio>

struct Face
{
	Vector3 vertices;
	Vector3 vertexNormals;
};

struct Mesh
{
	uint32 numVertices;
	Vector3f* vertices;

	uint32 numVertexNormals;
	Vector3f* vertexNormals;
	
	uint32 numFaces;
    Face* faces;
	
	Matrix4f objectTransform;
	Matrix4f worldTransform;
};

static void readMeshData(char* filename, Vector3f* modelVertices, Vector3f* modelVertexNormals,
						 Face* modelFaces);

static inline bool isValidFace(Face* face, uint32 numVertices)
{
	return (uint32)face->vertices[0] < numVertices &&
		(uint32)face->vertices[1] < numVertices &&
		(uint32)face->vertices[2] < numVertices;
}

static void initMesh(Mesh* mesh, uint32 numVertices, uint32 numVertexNormals, uint32 numFaces,
					 uint8* meshMemory, char* modelFilename)
{
	mesh->numVertices = numVertices;
	mesh->vertices = (Vector3f*)meshMemory;

	mesh->numVertexNormals = numVertexNormals;
	mesh->vertexNormals = (Vector3f*)(meshMemory + sizeof(Vector3f)*numVertices);

	mesh->numFaces = numFaces;
	mesh->faces = (Face*)(meshMemory + sizeof(Vector3f)*(numVertices + numVertexNormals));

	readMeshData(modelFilename, mesh->vertices, mesh->vertexNormals, mesh->faces);

	mesh->objectTransform = getIdentityMatrix4f();
}

//NOTE(denis): if you insert 0 for the faces of vertices parameters the function will allow
// you to count all the faces or vertices
static void readMeshData(char* filename, Vector3f* modelVertices, Vector3f* modelVertexNormals,
						  Face* modelFaces)
{
	FILE* modelFile = fopen(filename, "r");
	if (modelFile)
	{
		uint32 vertexIndex = 0;
		uint32 vertexNormalIndex = 0;
		uint32 faceIndex = 0;
			
		char line[201] = {};
		while (fgets(line, 200, modelFile) != NULL)
		{
			if (line[0] == 'v' && line[1] == ' ')
			{
				char** tokenArray = tokenizeStringInPlace(line, 5, ' ');

				real32 value1 = parseReal32String(tokenArray[1]);
				real32 value2 = parseReal32String(tokenArray[2]);
				real32 value3 = parseReal32String(tokenArray[3]);

				Vector3f vertex = V3f(value1, value2, value3);

				if (modelVertices != 0)
					modelVertices[vertexIndex++] = vertex;
				else
					++vertexIndex;
				
				HEAP_FREE(tokenArray);
			}
			else if (line[0] == 'v' && line[1] == 'n')
			{
				char** tokenArray = tokenizeStringInPlace(line, 5, ' ');
				
				real32 value1 = parseReal32String(tokenArray[1]);
				real32 value2 = parseReal32String(tokenArray[2]);
				real32 value3 = parseReal32String(tokenArray[3]);
				
				Vector3f normal = V3f(value1, value2, value3);

				if (modelVertexNormals != 0)
					modelVertexNormals[vertexNormalIndex++] = normal;
				else
					++vertexNormalIndex;

				HEAP_FREE(tokenArray);
			}
			else if (line[0] == 'f' && line[1] == ' ')
			{
				char** tokenArray = tokenizeStringInPlace(line, 5, ' ');
				
				char** vertex1Tokens = tokenizeStringInPlace(tokenArray[1], 3, '/');
				char** vertex2Tokens = tokenizeStringInPlace(tokenArray[2], 3, '/');
				char** vertex3Tokens = tokenizeStringInPlace(tokenArray[3], 3, '/');
				
				//NOTE(denis): vertex indices start at 1 in .obj files
				int32 vertexValue1 = parseInt32String(vertex1Tokens[0]) - 1;
				int32 vertexValue2 = parseInt32String(vertex2Tokens[0]) - 1;
				int32 vertexValue3 = parseInt32String(vertex3Tokens[0]) - 1;

				int32 normalValue1 = parseInt32String(vertex1Tokens[1]) - 1;
				int32 normalValue2 = parseInt32String(vertex2Tokens[1]) - 1;
				int32 normalValue3 = parseInt32String(vertex3Tokens[1]) - 1;
				
			    Face face;
				
				face.vertices.x = vertexValue1;
				face.vertices.y = vertexValue2;
				face.vertices.z = vertexValue3;

				face.vertexNormals.x = normalValue1;
				face.vertexNormals.y = normalValue2;
				face.vertexNormals.z = normalValue3;

				if (modelFaces != 0)
					modelFaces[faceIndex++] = face;
				else
					++faceIndex;

				HEAP_FREE(vertex1Tokens);
				HEAP_FREE(vertex2Tokens);
				HEAP_FREE(vertex3Tokens);
				HEAP_FREE(tokenArray);
			}
		}

		fclose(modelFile);
	}
}

#endif
