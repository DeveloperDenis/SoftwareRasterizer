#if !defined(MAIN_H_)
#define MAIN_H_

#include "mesh.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define ZSCALE_FACTOR 5000

#define MIN_ZOOM 2.0f
#define MAX_ZOOM 20.0f

enum
{
    DRAW_WIREFRAME,
	DRAW_FLAT,
	DRAW_PHONG,

	NUM_DRAW_MODES
};

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

struct Scene
{
	Camera camera;
	
	v3f ambientColour;
	
	v3f lightPos;
	v3f lightColour;
};

struct Memory
{
    v3f upVector;

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
	Mesh monkey;

	Matrix4f viewTransform;
	Matrix4f projectionTransform;

	u32 drawMode;
};

#endif
