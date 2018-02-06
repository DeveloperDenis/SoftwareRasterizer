#include "platform_layer.h"
#include "main.h" // I know platform_layer.h includes main.h, this is just to make it clear to all
#include "denis_drawing.h"

//TODO(denis): probably don't want to pass the entire memory pointer here, just the specific "transient memory area
static void* pushArray(Memory* memory, uint32 elementSize, uint32 elementCount)
{
	void* result = 0;
	
	uint32 requiredMemory = elementSize*elementCount;
	ASSERT(memory->transientMemoryIndex + requiredMemory < TRANSIENT_MEMORY_SIZE);

	result = &memory->transientMemory[memory->transientMemoryIndex];
	memory->transientMemoryIndex += requiredMemory;

	return result;
}

static inline void clearArray(int32 array[], uint32 numElements, int32 value)
{
	for (uint32 i = 0; i < numElements; ++i)
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

static inline void faceToScreenPos(Bitmap* screen, Vector3f* vertices, Face* face, Matrix4f* transform,
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

//NOTE(denis): returns a normalized direction of the light relative to a face
static inline Vector3f getLightDirection(Face* face, Vector3f* vertices, Vector3f lightPos)
{
	Vector3f midPoint =
		(vertices[face->vertices[0]] + vertices[face->vertices[1]] + vertices[face->vertices[2]])/3;
	Vector3f lightDirection = normalize(lightPos - midPoint);

	return lightDirection;
}

//NOTE(denis): assumes lightDirection is normalized, but not normal (for some reason)
uint32 calculateLight(Vector3f normal, Vector3f lightDirection)
{
	Vector3f normalizedNormal = normalize(normal);
	real32 scalarProduct = dot(normalizedNormal, lightDirection);

	uint32 ambientColour = 0x00202020;
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

union Triangle3
{
	struct
	{
		Vector3 p1;
		Vector3 p2;
		Vector3 p3;
	};
	Vector3 p[3];

	Vector3& operator[](uint32 index)
	{
		ASSERT(index >= 0 && index < 3);
		return p[index];
	};
};

static inline Triangle3 Tri3(Vector3 p1, Vector3 p2, Vector3 p3)
{
	Triangle3 tri;
	tri.p1 = p1;
	tri.p2 = p2;
	tri.p3 = p3;
	return tri;
}

static Vector3f triangleBarycentric(Triangle3 triangle, Vector2 point)
{
	Vector3f result;

	Vector2 p1 = triangle[0].xy;
	Vector2 p2 = triangle[1].xy;
	Vector2 p3 = triangle[2].xy;

	//TODO(denis): denominator can be taken out of this function as an optimization
	int32 denominator = (p2.y - p3.y)*(p1.x - p3.x) + (p3.x - p2.x)*(p1.y - p3.y);
	result.x = (real32)((p2.y - p3.y)*(point.x - p3.x) + (p3.x - p2.x)*(point.y - p3.y)) / (real32)denominator;
	result.y = (real32)((p3.y - p1.y)*(point.x - p3.x) + (p1.x - p3.x)*(point.y - p3.y)) / (real32)denominator;
	result.z = 1.0f - result.x - result.y;
	
	return result;
}

struct TriangleFragments
{
	uint32 numFragments;

	//TODO(denis): if the number of properties here becomes a lot, I could potentially create a Fragment struct that could
	// help simplify the memory allocation
	Vector3* fragments;
	Vector3f* baryCoords;
};
//TODO(denis): see comment for pushArray
static TriangleFragments barycentricRasterize(Memory* memory, Bitmap* buffer, Triangle3 triangle)
{
	TriangleFragments result = {};
	
	int32 top = MIN(MIN(triangle.p1.y, triangle.p2.y), triangle.p3.y);
	int32 bottom = MAX(MAX(triangle.p1.y, triangle.p2.y), triangle.p3.y);
	int32 left = MIN(MIN(triangle.p1.x, triangle.p2.x), triangle.p3.x);
	int32 right = MAX(MAX(triangle.p1.x, triangle.p2.x), triangle.p3.x);

	if (bottom < 0 || top >= (int32)buffer->height)
		return result;
	if (right < 0 || left >= (int32)buffer->width)
		return result;

	Vector3 p1 = V3(triangle.p1.xy, 0);
	Vector3 p2 = V3(triangle.p2.xy, 0);
	Vector3 p3 = V3(triangle.p3.xy, 0);

	//TODO(denis): not great solution, potentially allocates WAY more than required
	uint32 pixelCount = (bottom - top)*(right - left);
	result.fragments = (Vector3*)pushArray(memory, sizeof(Vector3), pixelCount);
	result.baryCoords = (Vector3f*)pushArray(memory, sizeof(Vector3f), pixelCount);

	for (int32 y = top; y < bottom; ++y)
	{
		for (int32 x = left; x < right; ++x)
		{
			Vector2 testPoint = V2(x, y);
			Vector3f testPointBarycentric = triangleBarycentric(triangle, testPoint);

			//TODO(denis): not perfect, but handles edge cases better than the cross product method
			if (testPointBarycentric.x >= 0.0f && testPointBarycentric.x <= 1.0f &&
				testPointBarycentric.y >= 0.0f && testPointBarycentric.y <= 1.0f &&
				testPointBarycentric.z >= 0.0f && testPointBarycentric.z <= 1.0f)
			{
				int32 zValue = testPointBarycentric.x*p1.z + testPointBarycentric.y*p2.z + testPointBarycentric.z*p3.z;
				result.fragments[result.numFragments] = V3(testPoint, zValue);
				result.baryCoords[result.numFragments] = testPointBarycentric;
				++result.numFragments;
			}
		}
	}

	return result;
}

static bool pointsOnSameSideOfLine(Vector3 linePoint1, Vector3 linePoint2, Vector3 testPoint1, Vector3 testPoint2)
{
	bool onSameSide = false;
	
	Vector3 lineVector = linePoint2 - linePoint1;
	Vector3 testVector1 = linePoint2 - testPoint1;
	Vector3 testVector2 = linePoint2 - testPoint2;

	Vector3 testResult1 = cross(lineVector, testVector1);
	Vector3 testResult2 = cross(lineVector, testVector2);

	if ((testResult1.z < 0 && testResult2.z < 0) || (testResult1.z > 0 && testResult2.z > 0))
	{
		onSameSide = true;
	}

	return onSameSide;
}

//NOTE(denis): this is the cross product approach to testing if a point is within a triangle
static void rasterizeTest(Bitmap* buffer, Triangle3 triangle, uint32 colour)
{
	int32 top = MIN(MIN(triangle.p1.y, triangle.p2.y), triangle.p3.y);
	int32 bottom = MAX(MAX(triangle.p1.y, triangle.p2.y), triangle.p3.y);
	int32 left = MIN(MIN(triangle.p1.x, triangle.p2.x), triangle.p3.x);
	int32 right = MAX(MAX(triangle.p1.x, triangle.p2.x), triangle.p3.x);

	if (bottom < 0 || top >= (int32)buffer->height)
		return;
	if (right < 0 || left >= (int32)buffer->width)
		return;

	Vector3 p1 = V3(triangle.p1.xy, 0);
	Vector3 p2 = V3(triangle.p2.xy, 0);
	Vector3 p3 = V3(triangle.p3.xy, 0);
	
	for (int32 y = top; y < bottom; ++y)
	{
		for (int32 x = left; x < right; ++x)
		{
			Vector3 testPoint = V3(x, y, 0);

			//TODO(denis): this approach doesn't seem to draw points that fall directly on an edge properly
			// need to create actual test cases to see how to do this properly
			if (pointsOnSameSideOfLine(p1, p2, testPoint, p3) &&
				pointsOnSameSideOfLine(p2, p3, testPoint, p1) &&
				pointsOnSameSideOfLine(p3, p1, testPoint, p2))
			{
				drawPoint(buffer, x, y, colour);
			}

#if 0
			//TODO(denis): super simple and bad way of solving the above problem
			drawLine(buffer, p1.xy, p2.xy, colour);
			drawLine(buffer, p2.xy, p3.xy, colour);
			drawLine(buffer, p3.xy, p1.xy, colour);
#endif
		}
	}
}

static void testTriangleRasterization(Bitmap* buffer, void (*rasterizeFunction)(Bitmap*, Triangle3, uint32))
{
	Vector3 p1 = V3(640, 360, 100);
	Vector3 p2 = V3(600, 380, 100);
	Vector3 p3 = V3(630, 400, 100);
	Vector3 p4 = V3(625, 300, 100);
	Vector3 p5 = V3(700, 313, 100);
	Vector3 p6 = V3(720, 350, 100);
	Vector3 p7 = V3(712, 480, 100);

	Triangle3 triangle1 = Tri3(p1, p2, p3);
	Triangle3 triangle2 = Tri3(p1, p4, p5);
	Triangle3 triangle3 = Tri3(p1, p2, p4);
	Triangle3 triangle4 = Tri3(p1, p6, p5);
	Triangle3 triangle5 = Tri3(p1, p6, p7);
	Triangle3 triangle6 = Tri3(p1, p7, p3);

	rasterizeFunction(buffer, triangle1, 0xFFFF0000);
	rasterizeFunction(buffer, triangle2, 0xFF00FF00);
	rasterizeFunction(buffer, triangle3, 0xFF0000FF);
	rasterizeFunction(buffer, triangle4, 0xFF0000FF);
	rasterizeFunction(buffer, triangle5, 0xFFFF0000);
	rasterizeFunction(buffer, triangle6, 0xFF0000FF);
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

	initMesh(&memory->cube1, 8, 8, 12, memory->cubeMemory, (char*)"../data/cube.obj");
	memory->cube1.objectTransform.translate(0.0f, 3.0f, 5.0f);
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

	memory->transientMemoryIndex = 0;
	
	clearArray(memory->zBuffer, sizeof(memory->zBuffer)/sizeof(int32), 999999);

	Rect2 viewportRect = Rect2(memory->cameraBufferPos.x, memory->cameraBufferPos.y,
							   memory->cameraBuffer.width, memory->cameraBuffer.height);

	cube1->objectTransform.rotate(0.0f, 0.01f, 0.0f);
    cube1->worldTransform = memory->projectionTransform * memory->viewTransform * cube1->objectTransform;
	
	// drawing
	fillBuffer(screen, 0xFF4D2177);
	fillBuffer(&memory->cameraBuffer, 0xFF604580);

	//TODO(denis): fill this out
	uint32 faceColours[12];
	
	//TODO(denis): separate into function later
	for (uint32 faceIndex = 0; faceIndex < cube1->numFaces; ++faceIndex)
	{
		Face* face = &cube1->faces[faceIndex];
		if (!isValidFace(face, cube1->numVertices))
			continue;

		Triangle3 screenTriangle;
		faceToScreenPos(&memory->cameraBuffer, cube1->vertices, face, &cube1->worldTransform, screenTriangle.p);

		//TODO(denis): we only ever need to hold one TriangleFragment in memory at a time, so we don't have to do all the transient
		// memory stuff, we only need to hold one at a time.
		TriangleFragments triangleFragments = barycentricRasterize(memory, &memory->cameraBuffer, screenTriangle);

		Vector3f v1 = cube1->vertices[face->vertices[0]];
		Vector3f v2 = cube1->vertices[face->vertices[1]];
		Vector3f v3 = cube1->vertices[face->vertices[2]];

		//TODO(denis): still some funky-ness around edges while rotating with weird shark teeth looking things
		// don't know if this is a lighting or zbuffer issue. Need to do some testing. Draw each face with a specified colour, without
		// any lighting to test.
		Vector3f faceNormal = cross(v2 - v1, v3 - v2);
		Vector3f facePosition = (v1 + v2 + v3) / 3;
		Vector3f lightDirection = getLightDirection(face, cube1->vertices, memory->lightPos);
		uint32 colour = calculateLight(faceNormal, lightDirection);
		
		for (uint32 i = 0; i < triangleFragments.numFragments; ++i)
		{
			Vector3 fragmentPos = triangleFragments.fragments[i];
			Vector3f fragmentBary = triangleFragments.baryCoords[i];
			int32 zValue = fragmentBary.x*screenTriangle[0].z + fragmentBary.y*screenTriangle[1].z + fragmentBary.z*screenTriangle[2].z;

			if (zValue < memory->zBuffer[fragmentPos.y*screen->width + fragmentPos.x])
			{
				drawPoint(&memory->cameraBuffer, triangleFragments.fragments[i].x, triangleFragments.fragments[i].y, colour);
				memory->zBuffer[fragmentPos.y*screen->width + fragmentPos.x] = zValue;
			}
		}
	}
	
	drawBitmap(screen, &memory->cameraBuffer, memory->cameraBufferPos);
	
	memory->lastMousePos = input->mouse.pos;
}
