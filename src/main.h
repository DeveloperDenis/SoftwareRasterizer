#if !defined(MAIN_H_)
#define MAIN_H_

#include "mesh.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define ZSCALE_FACTOR 200

#define MIN_ZOOM 2.0f
#define MAX_ZOOM 20.0f

struct Fragments
{
    u32 numFragments;

    v3* points;
    v3f* baryCoords;
};

struct Camera
{
    v3f pos;
    v3f targetPos;
	
    f32 nearPlaneZ;
    f32 farPlaneZ;

    f32 fov; // in radians
};

struct ZBufferData
{
    s32* zBuffer;
    v3 points[3];

    f32 denominators[3];
};

struct Scene
{
	v3f lightPos;
};

struct Memory
{
    v3f upVector;
	
	Camera camera;

	//NOTE(denis): used for the camera movement code
    v2 lastMousePos;

	//TODO(denis): step one: draw the 3D scene in the whole screen
	// which means I need to change how I calculate the projection matrix so that it supports
	// non-square resolutions
	Bitmap cameraBuffer;
    v2 cameraBufferPos;
    u32 cameraBufferData[720*720];
    s32 zBuffer[720*720];
	
	Scene scene;
	
	Mesh cube;
    u8 cubeMemory[sizeof(v3f)*8 + sizeof(v3f)*8 + sizeof(Face)*12];

	Mesh monkey;
    u8 monkeyMemory[sizeof(v3f)*507 + sizeof(v3f)*507 + sizeof(Face)*968];

	u32 rotationAngle;
	
	Matrix4f viewTransform;
	Matrix4f projectionTransform;
};

#endif
