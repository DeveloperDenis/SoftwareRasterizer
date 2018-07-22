#if !defined(MESH_H_)
#define MESH_H_

#include "denis_strings.h"
//NOTE(denis): needed to load the model .obj file
#include <cstdio>

struct Face
{
    v3 vertices;
	v3 vertexNormals;
};

struct Mesh
{
    u32 numVertices;
    v3f* vertices;

    u32 numVertexNormals;
    v3f* vertexNormals;
	
    u32 numFaces;
    Face* faces;
	
	Matrix4f objectTransform;
	Matrix4f worldTransform;
};

static void readMeshData(char* filename, v3f* modelVertices, v3f* modelVertexNormals, Face* modelFaces);

static inline bool isValidFace(Face* face, u32 numVertices)
{
	return (u32)face->vertices[0] < numVertices &&
		(u32)face->vertices[1] < numVertices &&
		(u32)face->vertices[2] < numVertices;
}

static void initMesh(Mesh* mesh, u32 numVertices, u32 numVertexNormals, u32 numFaces,
					 u8* meshMemory, char* modelFilename)
{
	mesh->numVertices = numVertices;
	mesh->vertices = (v3f*)meshMemory;

	mesh->numVertexNormals = numVertexNormals;
	mesh->vertexNormals = (v3f*)(meshMemory + sizeof(v3f)*numVertices);

	mesh->numFaces = numFaces;
	mesh->faces = (Face*)(meshMemory + sizeof(v3f)*(numVertices + numVertexNormals));

	readMeshData(modelFilename, mesh->vertices, mesh->vertexNormals, mesh->faces);

	mesh->objectTransform = getIdentityMatrix4f();
}

//NOTE(denis): if you insert 0 for the faces of vertices parameters the function will allow
// you to count all the faces or vertices
static void readMeshData(char* filename, v3f* modelVertices, v3f* modelVertexNormals, Face* modelFaces)
{
	FILE* modelFile = fopen(filename, "r");
	if (modelFile)
	{
	    u32 vertexIndex = 0;
	    u32 vertexNormalIndex = 0;
	    u32 faceIndex = 0;
			
		char line[201] = {};
		while (fgets(line, 200, modelFile) != NULL)
		{
			if (line[0] == 'v' && line[1] == ' ')
			{
				char** tokenArray = tokenizeStringInPlace(line, 5, ' ');

			    f32 value1 = parseF32String(tokenArray[1]);
			    f32 value2 = parseF32String(tokenArray[2]);
			    f32 value3 = parseF32String(tokenArray[3]);

			    v3f vertex(value1, value2, value3);

				if (modelVertices != 0)
					modelVertices[vertexIndex++] = vertex;
				else
					++vertexIndex;
				
				HEAP_FREE(tokenArray);
			}
			else if (line[0] == 'v' && line[1] == 'n')
			{
				char** tokenArray = tokenizeStringInPlace(line, 5, ' ');
				
			    f32 value1 = parseF32String(tokenArray[1]);
			    f32 value2 = parseF32String(tokenArray[2]);
			    f32 value3 = parseF32String(tokenArray[3]);
				
			    v3f normal(value1, value2, value3);

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
				s32 vertexValue1 = parseS32String(vertex1Tokens[0]) - 1;
			    s32 vertexValue2 = parseS32String(vertex2Tokens[0]) - 1;
			    s32 vertexValue3 = parseS32String(vertex3Tokens[0]) - 1;

				s32 normalValue1 = parseS32String(vertex1Tokens[1]) - 1;
				s32 normalValue2 = parseS32String(vertex2Tokens[1]) - 1;
			    s32 normalValue3 = parseS32String(vertex3Tokens[1]) - 1;
				
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