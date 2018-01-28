#include "platform_layer.h"
#include "main.h" // I know platform_layer.h includes main.h, this is just to make it clear to all
#include "denis_drawing.h"
#include "rasterize.h"

static inline void clearArray(int32 array[], uint32 numElements, int32 value)
{
	for (uint32 i = 0; i < numElements; ++i)
	{
		array[i] = value;
	}
}
static inline void clearArray(int32 array[], int32 value)
{
	int32 length = ARRAY_COUNT(array, int32);
	for (int32 i = 0; i < length; ++i)
	{
		array[i] = value;
	}
}

//NOTE(denis): we keep the z value for z-buffer stuff
static Vector3 pointToScreenPos(Bitmap* screen, Vector3f objectPos, Matrix4f* transform)
{
	Vector3 result;
	Vector4f worldPos = (*transform) * objectPos;

	//NOTE(denis): all points on the image plane are in the range [-1, 1]
	// so we scale those to fit in the screen area
	result.x = (int32)((worldPos.x + 1)/2 * screen->width);
	result.y = (int32)((1 - (worldPos.y + 1)/2) * screen->height);

	if (worldPos.z > 1.0f)
		worldPos.z = 1.0f;
	else if (worldPos.z < 0.0f)
		worldPos.z = 0.0f;
	
	result.z = (int32)(worldPos.z * ZSCALE_FACTOR);
	
	return result;
}

static void faceToScreenPos(Bitmap* screen, Vector3f* vertices, Face* face, Matrix4f* transform,
							Vector3 output[3])
{
	Vector3f vertex1 = vertices[face->vertices[0]];
	Vector3f vertex2 = vertices[face->vertices[1]];
	Vector3f vertex3 = vertices[face->vertices[2]];

	output[0] = pointToScreenPos(screen, vertex1, transform);
	output[1] = pointToScreenPos(screen, vertex2, transform);
	output[2] = pointToScreenPos(screen, vertex3, transform);
}

//NOTE(denis): draws the bitmap onto the buffer with x and y specified in rect
// clips bitmap width and height to rect width & rect height
//TODO(denis): doesn't draw partial alpha, only full or none
static void drawBitmap(Bitmap* buffer, Bitmap* bitmap, Rect2 rect)
{
	int32 startY = MAX(rect.getTop(), 0);
	int32 endY = MIN(rect.getBottom(), (int32)buffer->height);

	int32 startX = MAX(rect.getLeft(), 0);
	int32 endX = MIN(rect.getRight(), (int32)buffer->width);

	uint32 row = 0;
	for (int32 y = startY; y < endY && row < bitmap->height; ++y, ++row)
	{
		uint32 col = 0;
		for (int32 x = startX; x < endX && col < bitmap->width; ++x, ++col)
		{
			uint32* inPixel = bitmap->pixels + row*bitmap->width + col;
			uint32* outPixel = buffer->pixels + y*buffer->width + x;

			if (((*inPixel) & (0xFF << 24)) != 0)
			{
				*outPixel = *inPixel;
			}
		}
	}
}
static inline void drawBitmap(Bitmap* buffer, Bitmap* bitmap, Vector2 pos)
{
	Rect2 rect = Rect2(pos.x, pos.y, bitmap->width, bitmap->height);
	drawBitmap(buffer, bitmap, rect);
}

static inline void drawTriangle(Bitmap* screen, Vector3 vertex1, Vector3 vertex2, Vector3 vertex3,
						 uint32 colour = 0xFFFFFFFF)
{
	drawLine(screen, vertex1.xy, vertex2.xy, colour);
	drawLine(screen, vertex2.xy, vertex3.xy, colour);
	drawLine(screen, vertex3.xy, vertex1.xy, colour);
}
static inline void drawTriangle(Bitmap* screen, Vector3 points[3], uint32 colour = 0xFFFFFFFF)
{
	drawTriangle(screen, points[0], points[1], points[2], colour);
}
static inline void drawTriangle(Bitmap* screen, Vector3f v1, Vector3f v2, Vector3f v3, Matrix4f* transform,
								uint32 colour = 0xFFFFFFFF)
{
	Vector3 screenPoints[3];
	screenPoints[0] = pointToScreenPos(screen, v1, transform);
	screenPoints[1] = pointToScreenPos(screen, v2, transform);
	screenPoints[2] = pointToScreenPos(screen, v3, transform);
	
	drawTriangle(screen, screenPoints[0], screenPoints[1], screenPoints[2], colour);

}

static inline void rasterizeTriangle(Bitmap* screen, Vector3f v1, Vector3f v2, Vector3f v3, Matrix4f* transform,
									 int32* zBuffer, uint32 colour = 0xFF00FF00)
{
	Vector3 screenPoints[3];
	screenPoints[0] = pointToScreenPos(screen, v1, transform);
	screenPoints[1] = pointToScreenPos(screen, v2, transform);
	screenPoints[2] = pointToScreenPos(screen, v3, transform);
	
	rasterize(screen, screenPoints, zBuffer, colour);
}
static inline void rasterizeTriangle(Bitmap* screen, Vector3 v1, Vector3 v2, Vector3 v3, int32* zBuffer,
									 uint32 colour = 0xFF00FF00)
{
	Vector3 pointArray[3] = {v1, v2, v3};
	rasterize(screen, pointArray, zBuffer, colour);
}

static inline Vector3f getLightDirection(Face* face, Vector3f* vertices, Vector3f lightPos)
{
	Vector3f midPoint =
		(vertices[face->vertices[0]] + vertices[face->vertices[1]] + vertices[face->vertices[2]])/3;
	Vector3f lightDirection = normalize(lightPos - midPoint);

	return lightDirection;
}

uint32 calculateLight(Vector3f normal, Vector3f lightDirection)
{
	Vector3f normalizedNormal = normalize(normal);
	real32 scalarProduct = dot(normalizedNormal, lightDirection);

	uint32 ambientColour = 0x00151515;
	uint32 colour;
	if (scalarProduct == 1.0f)
	    colour = 0xFFFFFFFF;
	else if (scalarProduct <= 0.0f)
	    colour = 0xFF000000 + ambientColour;
	else
	{
		uint8 brightness = (uint8)(0xFF * scalarProduct);
	    colour = (0xFF << 24) | (brightness << 16) | (brightness << 8) | brightness;

		if (colour <= 0xFFFFFFFF - ambientColour)
			colour += ambientColour;
		else
			colour = 0xFFFFFFFF;
	}

	return colour;
}

static void drawWireframe(Bitmap* screen, Mesh* mesh)
{
	for (uint32 faceIndex = 0; faceIndex < mesh->numFaces; ++faceIndex)
	{
	    Face face = mesh->faces[faceIndex];

		if (isValidFace(&face, mesh->numVertices))
		{
			Vector3 screenPoints[3];
			faceToScreenPos(screen, mesh->vertices, &face, &mesh->worldTransform, screenPoints);
			
			uint32 colour = 0xFFA00055;
		    drawTriangle(screen, screenPoints[0], screenPoints[1], screenPoints[2], colour);
		}
	}
}

static void drawFlatShaded(Bitmap* screen, Mesh* mesh, int32* zBuffer, Vector3f lightPos)
{
	for (uint32 faceIndex = 0; faceIndex < mesh->numFaces; ++faceIndex)
	{
	    Face face = mesh->faces[faceIndex];

		if (isValidFace(&face, mesh->numVertices))
		{
			Vector3 screenPoints[3];
			faceToScreenPos(screen, mesh->vertices, &face, &mesh->worldTransform, screenPoints);

			Vector3f lightDirection = getLightDirection(&face, mesh->vertices, lightPos);
			
			Vector3f side1 = mesh->vertices[face.vertices[1]] - mesh->vertices[face.vertices[0]];
			Vector3f side2 = mesh->vertices[face.vertices[2]] - mesh->vertices[face.vertices[1]];
			Vector3f triangleNormal = cross(side1, side2);
			uint32 faceColour = calculateLight(triangleNormal, lightDirection);
			
			rasterize(screen, screenPoints, zBuffer, faceColour);
		}
	}
}

//NOTE(denis): this uses the look-at matrix approach
static Matrix4f calculateViewMatrix(Vector3f cameraPos, Vector3f targetPos, Vector3f upVector)
{
	Matrix4f viewMatrix = M4f();

	Vector3f zAxis = normalize(cameraPos - targetPos);	
	//TODO(denis): if I am going to be rotating the camera around, I need to handle the case
	// where the zAxis is (0, 1, 0) or (0, -1, 0) since that would break this calculation
//	ASSERT(zAxis != V3f(0.0f, 1.0f, 0.0f) && zAxis != V3f(0.0f, -1.0f, 0.0f));
	Vector3f xAxis = normalize(cross(upVector, zAxis));
	Vector3f yAxis = cross(zAxis, xAxis);

	Matrix4f orientation = M4f();
	orientation.setRow(0, xAxis);
	orientation.setRow(1, yAxis);
	orientation.setRow(2, zAxis);

	Matrix4f translation = M4f();
	translation.setTranslation(-cameraPos);

	viewMatrix = orientation*translation;
	return viewMatrix;
}

//NOTE(denis): calculates a perspective projection matrix only, no other projection types
static Matrix4f calculateProjectionMatrix(real32 near, real32 far, real32 fov)
{
	Matrix4f projectionMatrix = M4f();

	//set new w to -z to perform our perspective projection automatically through
	// the matrix multiplication
    projectionMatrix[3][2] = -1;
    projectionMatrix[3][3] = 0;

	//clipping plane stuff
	projectionMatrix[2][2] = -far / (far - near);
	projectionMatrix[2][3] = -((far*near)/(far - near));

	real32 fovScaleFactor = 1/tan(fov/2);
	projectionMatrix[0][0] = fovScaleFactor;
	projectionMatrix[1][1] = fovScaleFactor;

	return projectionMatrix;
}

static void initMemory(Bitmap* screen, Memory* memory, Input* input)
{
	memory->isInit = true;

	memory->lastMousePos = input->mouse.pos;
		
	memory->upVector = V3f(0.0f, 1.0f, 0.0f);

	Camera* camera = &memory->camera;
	camera->pos = V3f(0.0f, 5.0f, 8.0f);
	camera->targetPos = V3f(0.0f, 0.0f, 0.0f);
	camera->nearPlaneZ = 1.0f;
	camera->farPlaneZ = 10.0f;
	camera->fov = (real32)(M_PI/2);

	memory->cameraBuffer.pixels = memory->cameraBufferData;
	memory->cameraBuffer.width = 720;
	memory->cameraBuffer.height = 720;
	memory->cameraBufferPos = V2((screen->width - memory->cameraBuffer.width)/2, 0);
		
	memory->viewTransform =
		calculateViewMatrix(camera->pos, camera->targetPos, memory->upVector);
		
	memory->projectionTransform =
		calculateProjectionMatrix(camera->nearPlaneZ, camera->farPlaneZ, camera->fov);

	memory->lightPos = V3f(3.0f, 8.0f, 10.0f);

	initMesh(&memory->cube1, 8, 8, 12, memory->cubeMemory, "../data/cube.obj");
	memory->cube1.worldTransform =
		memory->projectionTransform * memory->viewTransform * memory->cube1.objectTransform;

	memory->cube2 = memory->cube1;
	memory->cube2.objectTransform.translate(2.5f, 0.0f, 0.0f);
	memory->cube2.worldTransform =
		memory->projectionTransform * memory->viewTransform * memory->cube2.objectTransform;

	memory->cube3 = memory->cube1;
	memory->cube3.objectTransform.translate(-2.5f, 0.0f, 0.0f);
	memory->cube3.worldTransform =
		memory->projectionTransform * memory->viewTransform * memory->cube3.objectTransform;
}

exportDLL MAIN_UPDATE_CALL(mainUpdateCall)
{
	static Mesh* cube1 = &memory->cube1;
	static Mesh* cube2 = &memory->cube2;
	static Mesh* cube3 = &memory->cube3;
	
	static Camera* camera = &memory->camera;
	
	if (!memory->isInit)
	{
		initMemory(screen, memory, input);
	}
	
	clearArray(memory->zBuffer, sizeof(memory->zBuffer)/sizeof(int32), 999999);

	Rect2 viewportRect = Rect2(memory->cameraBufferPos.x, memory->cameraBufferPos.y,
							   memory->cameraBuffer.width, memory->cameraBuffer.height);
	
	// drawing
	fillBuffer(screen, 0xFFFFFFFF);

	//NOTE(denis): triangle drawing tests
	Vector3 p1 = V3(640, 360, 100);
	Vector3 p2 = V3(600, 380, 100);
	Vector3 p3 = V3(630, 400, 100);
	Vector3 p4 = V3(625, 300, 100);
	Vector3 p5 = V3(700, 313, 100);
	Vector3 p6 = V3(720, 350, 100);
	Vector3 p7 = V3(712, 480, 100);

	Vector3 triangle1[3] = {p1, p2, p3};
	Vector3 triangle2[3] = {p1, p4, p5};
	Vector3 triangle3[3] = {p1, p2, p4};
	Vector3 triangle4[3] = {p1, p6, p5};
	Vector3 triangle5[3] = {p1, p6, p7};
	Vector3 triangle6[3] = {p1, p7, p3};

	bool fillTriangles = true;
	bool drawOutlines = false;
	
	if (fillTriangles)
	{
		rasterize(screen, triangle1, memory->zBuffer, 0xFFFF0000);
		rasterize(screen, triangle2, memory->zBuffer, 0xFF00FF00);
		rasterize(screen, triangle3, memory->zBuffer, 0xFF0000FF);
		rasterize(screen, triangle4, memory->zBuffer, 0xFF0000FF);
		rasterize(screen, triangle5, memory->zBuffer, 0xFFFF0000);
		rasterize(screen, triangle6, memory->zBuffer, 0xFF0000FF);
	}

	if (drawOutlines)
	{
		drawTriangle(screen, triangle1, 0xFF000000);
		drawTriangle(screen, triangle2, 0xFF000000);
		drawTriangle(screen, triangle3, 0xFF000000);
		drawTriangle(screen, triangle4, 0xFF000000);
		drawTriangle(screen, triangle5, 0xFF000000);
		drawTriangle(screen, triangle6, 0xFF000000);
	}
	
#if 0
	fillBuffer(&memory->cameraBuffer, 0xFF3D212A);

	drawFlatShaded(&memory->cameraBuffer, cube1, memory->zBuffer, memory->lightPos);
	drawFlatShaded(&memory->cameraBuffer, cube2, memory->zBuffer, memory->lightPos);
	drawFlatShaded(&memory->cameraBuffer, cube3, memory->zBuffer, memory->lightPos);

	drawBitmap(screen, &memory->cameraBuffer, memory->cameraBufferPos);
#endif

	memory->lastMousePos = input->mouse.pos;
}
