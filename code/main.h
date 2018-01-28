#if !defined(MAIN_H_)
#define MAIN_H_

struct STATIC_SETTINGS
{
	static const char* WINDOW_TITLE;
	static const uint32 WINDOW_WIDTH;
	static const uint32 WINDOW_HEIGHT;
	static const bool WINDOW_RESIZABLE;

	static const char* DLL_FILE_NAME;
	static const uint32 FPS_TARGET;
};

const char* STATIC_SETTINGS::WINDOW_TITLE = "Graphics Testing";
const uint32 STATIC_SETTINGS::WINDOW_WIDTH = 1280;
const uint32 STATIC_SETTINGS::WINDOW_HEIGHT = 720;
const bool STATIC_SETTINGS::WINDOW_RESIZABLE = false;

const char* STATIC_SETTINGS::DLL_FILE_NAME = "GraphicsTesting.dll";
const uint32 STATIC_SETTINGS::FPS_TARGET = 60;

#include "mesh.h"

#define ZSCALE_FACTOR 200

#define MIN_ZOOM 2.0f
#define MAX_ZOOM 20.0f

struct Camera
{
	Vector3f pos;
	Vector3f targetPos;
	
	real32 nearPlaneZ;
	real32 farPlaneZ;

	real32 fov; // in radians
};

struct ZBufferData
{
	int32* zBuffer;
	Vector3 points[3];

    real32 denominators[3];
};

struct Memory
{
	bool isInit;
	
	Vector3f upVector;
	
	Camera camera;

	//NOTE(denis): used for the camera movement code
	Vector2 lastMousePos;

	//TODO(denis): step one: draw the 3D scene in the whole screen
	// which means I need to change how I calculate the projection matrix so that it supports
	// non-square resolutions
	Bitmap cameraBuffer;
	Vector2 cameraBufferPos;
	uint32 cameraBufferData[720*720];

    int32 zBuffer[STATIC_SETTINGS::WINDOW_WIDTH * STATIC_SETTINGS::WINDOW_HEIGHT];

	Vector3f lightPos;
	
	Mesh cube1;
	Mesh cube2;
	Mesh cube3;
	uint8 cubeMemory[sizeof(Vector3f)*8 + sizeof(Vector3f)*8 + sizeof(Face)*12];
	
	Matrix4f viewTransform;

	Matrix4f projectionTransform;
};

#endif
