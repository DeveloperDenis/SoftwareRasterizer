#include "platform_layer.h"

#include "main.h"
#include "denis_drawing.h"

static inline void clearArray(s32 array[], u32 numElements, s32 value)
{
	for (u32 i = 0; i < numElements; ++i)
	{
		array[i] = value;
	}
}

//NOTE(denis): we keep the z value for z-buffer stuff
static v3 pointToScreenPos(Bitmap* screen, v3f objectPos, Matrix4f* transform)
{
    v3 result;
    v4f worldPos = (*transform) * v4f(objectPos);

	//NOTE(denis): all points on the image plane are in the range [-1, 1]
	// so we scale those to fit in the screen area
	result.x = (s32)((worldPos.x + 1)/2 * screen->width);
	result.y = (s32)((1 - (worldPos.y + 1)/2) * screen->height);	
	result.z = (s32)(worldPos.z * ZSCALE_FACTOR);

	ASSERT((worldPos.z <=0 && worldPos.z*ZSCALE_FACTOR <= 0) ||
		   (worldPos.z >= 0 && worldPos.z*ZSCALE_FACTOR >= 0));
	
	return result;
}

static inline void faceToScreenPos(Bitmap* screen, v3f* vertices, Face* face, Matrix4f* transform, v3 output[3])
{
	v3f vertex1 = vertices[face->vertices[0]];
    v3f vertex2 = vertices[face->vertices[1]];
    v3f vertex3 = vertices[face->vertices[2]];

	output[0] = pointToScreenPos(screen, vertex1, transform);
	output[1] = pointToScreenPos(screen, vertex2, transform);
	output[2] = pointToScreenPos(screen, vertex3, transform);
}

static inline void drawTriangle(Bitmap* screen, v3 vertex1, v3 vertex2, v3 vertex3, u32 colour = 0xFFFFFFFF)
{
	drawLine(screen, vertex1.xy, vertex2.xy, colour);
	drawLine(screen, vertex2.xy, vertex3.xy, colour);
	drawLine(screen, vertex3.xy, vertex1.xy, colour);
}
static inline void drawTriangle(Bitmap* screen, v3 points[3], u32 colour = 0xFFFFFFFF)
{
	drawTriangle(screen, points[0], points[1], points[2], colour);
}
static inline void drawTriangle(Bitmap* screen, v3f vertex1, v3f vertex2, v3f vertex3, Matrix4f* transform,
								u32 colour = 0xFFFFFFFF)
{
	v3 screenPoints[3];
	screenPoints[0] = pointToScreenPos(screen, vertex1, transform);
	screenPoints[1] = pointToScreenPos(screen, vertex2, transform);
	screenPoints[2] = pointToScreenPos(screen, vertex3, transform);
	
	drawTriangle(screen, screenPoints[0], screenPoints[1], screenPoints[2], colour);

}

//NOTE(denis): returns a normalized direction of the light relative to a face
static inline v3f getLightDirection(Face* face, v3f* vertices, v3f lightPos)
{
	v3f midPoint =
		(vertices[face->vertices[0]] + vertices[face->vertices[1]] + vertices[face->vertices[2]])/3;
    v3f lightDirection = normalize(lightPos - midPoint);

	return lightDirection;
}

//NOTE(denis): assumes lightDirection is normalized, but not normal (for some reason)
u32 calculateLight(v3f normal, v3f lightDirection)
{
	v3f normalizedNormal = normalize(normal);
    f32 scalarProduct = dot(normalizedNormal, lightDirection);

    u32 ambientColour = 0x00202020;
	u32 colour;
	if (scalarProduct == 1.0f)
	    colour = 0xFFFFFFFF;
	else if (scalarProduct <= 0.0f)
	    colour = 0xFF000000 + ambientColour;
	else
	{
		u8 brightness = (u8)(0xFF * scalarProduct);
	    colour = (0xFF << 24) | (brightness << 16) | (brightness << 8) | brightness;

		if (colour <= 0xFFFFFFFF - ambientColour)
			colour += ambientColour;
		else
			colour = 0xFFFFFFFF;
	}

	return colour;
}

static void drawWireframe(Bitmap* buffer, Mesh* mesh)
{
	for (u32 faceIndex = 0; faceIndex < mesh->numFaces; ++faceIndex)
	{
	    Face face = mesh->faces[faceIndex];

		if (isValidFace(&face, mesh->numVertices))
		{
			v3 screenPoints[3];
			faceToScreenPos(buffer, mesh->vertices, &face, &mesh->worldTransform, screenPoints);
			
		    u32 colour = 0xFFA00055;
		    drawTriangle(buffer, screenPoints[0], screenPoints[1], screenPoints[2], colour);
		}
	}
}

//NOTE(denis): this uses the look-at matrix approach
static Matrix4f calculateViewMatrix(v3f cameraPos, v3f targetPos, v3f upVector)
{
	Matrix4f viewMatrix = M4f();

	v3f zAxis = normalize(cameraPos - targetPos);	
	//TODO(denis): if I am going to be rotating the camera around, I need to handle the case
	// where the zAxis is (0, 1, 0) or (0, -1, 0) since that would break this calculation
//	ASSERT(zAxis != v3f(0.0f, 1.0f, 0.0f) && zAxis != v3f(0.0f, -1.0f, 0.0f));
    v3f xAxis = normalize(cross(upVector, zAxis));
    v3f yAxis = cross(zAxis, xAxis);

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
static Matrix4f calculateProjectionMatrix(f32 near, f32 far, f32 fov)
{
	Matrix4f projectionMatrix = M4f();

	//set new w to -z to perform our perspective projection automatically through
	// the matrix multiplication
    projectionMatrix[3][2] = -1;
    projectionMatrix[3][3] = 0;

	//clipping plane stuff
	projectionMatrix[2][2] = -far / (far - near);
	projectionMatrix[2][3] = -((far*near)/(far - near));

    f32 fovScaleFactor = 1/tan(fov/2);
	projectionMatrix[0][0] = fovScaleFactor;
	projectionMatrix[1][1] = fovScaleFactor;

   return projectionMatrix;
}

static v3f triangleBarycentric(v3 points[3], v2 testPoint)
{
    v3f result;

	v2 p1 = points[0].xy;
    v2 p2 = points[1].xy;
    v2 p3 = points[2].xy;

	//TODO(denis): denominator can be taken out of this function as an optimization
    s32 denominator = (p2.y - p3.y)*(p1.x - p3.x) + (p3.x - p2.x)*(p1.y - p3.y);
	result.x = (f32)((p2.y - p3.y)*(testPoint.x - p3.x) + (p3.x - p2.x)*(testPoint.y - p3.y)) / (f32)denominator;
	result.y = (f32)((p3.y - p1.y)*(testPoint.x - p3.x) + (p1.x - p3.x)*(testPoint.y - p3.y)) / (f32)denominator;
	result.z = 1.0f - result.x - result.y;
	
	return result;
}

//NOTE(denis): uses barycentric coordinates to rasterize a triangle
static Fragments barycentricRasterize(Bitmap* buffer, v3 points[3])
{
    Fragments result = {};
	
	s32 top = MIN(MIN(points[0].y, points[1].y), points[2].y);
    s32 bottom = MAX(MAX(points[0].y, points[1].y), points[2].y);
    s32 left = MIN(MIN(points[0].x, points[1].x), points[2].x);
    s32 right = MAX(MAX(points[0].x, points[1].x), points[2].x);
	
	if (bottom < 0 || top >= (s32)buffer->height)
		return result;
	if (right < 0 || left >= (s32)buffer->width)
		return result;

    v3 p1(points[0]);
    v3 p2(points[1]);
    v3 p3(points[2]);

	//TODO(denis): this frequent heap allocation and free is waaaay slower than what I probably want
    u32 pixelCount = (bottom - top)*(right - left);
	result.points = (v3*)HEAP_ALLOC(sizeof(v3)*pixelCount);
	result.baryCoords = (v3f*)HEAP_ALLOC(sizeof(v3f)*pixelCount);

	for (s32 y = top; y < bottom; ++y)
	{
		for (s32 x = left; x < right; ++x)
		{
		    v2 testPoint(x, y);
			testPoint.x = CLAMP_RANGE(testPoint.x, 0, (s32)buffer->width - 1);
			testPoint.y = CLAMP_RANGE(testPoint.y, 0, (s32)buffer->height - 1);

			ASSERT(testPoint.x >= 0.0f && testPoint.y >= 0.0f);
			
			v3f testPointBarycentric = triangleBarycentric(points, testPoint);

			//TODO(denis): not perfect, but handles edge cases better than the cross product method
			if (testPointBarycentric.x >= 0.0f && testPointBarycentric.x <= 1.0f &&
				testPointBarycentric.y >= 0.0f && testPointBarycentric.y <= 1.0f &&
				testPointBarycentric.z >= 0.0f && testPointBarycentric.z <= 1.0f)
			{
			    s32 zValue = (s32)(testPointBarycentric.x*p1.z + testPointBarycentric.y*p2.z + testPointBarycentric.z*p3.z);
				result.points[result.numFragments] = v3(testPoint, zValue);
				result.baryCoords[result.numFragments] = testPointBarycentric;
				++result.numFragments;
			}
		}
	}

	return result;
}

static void drawMesh(Bitmap* buffer, s32* zBuffer, Scene* scene, Mesh* mesh)
{
	for (u32 faceIndex = 0; faceIndex < mesh->numFaces; ++faceIndex)
	{
		Face* face = &mesh->faces[faceIndex];
		if (!isValidFace(face, mesh->numVertices))
			continue;

	    v3 screenPoints[3] = {};
		faceToScreenPos(buffer, mesh->vertices, face, &mesh->worldTransform, screenPoints);

	    Fragments fragments = barycentricRasterize(buffer, screenPoints);

	    v3f p1 = mesh->vertices[face->vertices[0]];
	    v3f p2 = mesh->vertices[face->vertices[1]];
	    v3f p3 = mesh->vertices[face->vertices[2]];

	    v3f faceNormal = cross(p2 - p1, p3 - p2);
	    v3f facePosition = (p1 + p2 + p3) / 3;
	    v3f lightDirection = getLightDirection(face, mesh->vertices, scene->lightPos);
	    u32 colour = calculateLight(faceNormal, lightDirection);
		
		for (u32 i = 0; i < fragments.numFragments; ++i)
		{
			//TODO(denis): fragment function doesn't take into account when the fragment is off the screens
		    v3 fragmentPos = fragments.points[i];
		    v3f fragmentBary = fragments.baryCoords[i];

			s32 zValue = fragmentPos.z;
			ASSERT(zValue < 999999);

			u32 bufferPosition = fragmentPos.y*buffer->width + fragmentPos.x;
			ASSERT(bufferPosition < 720*720);
			
			if (zValue < zBuffer[bufferPosition])
			{
				drawPoint(buffer, fragmentPos.x, fragmentPos.y, colour);
			    zBuffer[bufferPosition] = zValue;
			}
		}

		HEAP_FREE(fragments.baryCoords);
		HEAP_FREE(fragments.points);
	}
}

static void testTriangleRasterization(Bitmap* buffer, void (*rasterizeFunction)(Bitmap*, v3 points[3], u32))
{
	v3 p1(640, 360, 100);
	v3 p2(600, 380, 100);
	v3 p3(630, 400, 100);
	v3 p4(625, 300, 100);
	v3 p5(700, 313, 100);
	v3 p6(720, 350, 100);
	v3 p7(712, 480, 100);

    v3 triangle1[3] = {p1, p2, p3};
	v3 triangle2[3] = {p1, p4, p5};
    v3 triangle3[3] = {p1, p2, p4};
    v3 triangle4[3] = {p1, p6, p5};
    v3 triangle5[3] = {p1, p6, p7};
    v3 triangle6[3] = {p1, p7, p3};

	rasterizeFunction(buffer, triangle1, 0xFFFF0000);
	rasterizeFunction(buffer, triangle2, 0xFF00FF00);
	rasterizeFunction(buffer, triangle3, 0xFF0000FF);
	rasterizeFunction(buffer, triangle4, 0xFF0000FF);
	rasterizeFunction(buffer, triangle5, 0xFFFF0000);
	rasterizeFunction(buffer, triangle6, 0xFF0000FF);
}

exportDLL APP_INIT_CALL(appInit)
{
	memory->lastMousePos = v2(-1, -1);
		
	memory->upVector = v3f(0.0f, 1.0f, 0.0f);

	Camera* camera = &memory->camera;
	camera->pos = v3f(0.0f, 5.0f, 8.0f);
	camera->targetPos = v3f(0.0f, 0.0f, 0.0f);
	camera->nearPlaneZ = 1.0f;
	camera->farPlaneZ = 10.0f;
	camera->fov = (f32)(M_PI/2);

	memory->cameraBuffer.pixels = memory->cameraBufferData;
	memory->cameraBuffer.width = 720;
	memory->cameraBuffer.height = 720;
	memory->cameraBufferPos = v2((screen->width - memory->cameraBuffer.width)/2, 0);
		
	memory->viewTransform =
		calculateViewMatrix(camera->pos, camera->targetPos, memory->upVector);
		
	memory->projectionTransform =
		calculateProjectionMatrix(camera->nearPlaneZ, camera->farPlaneZ, camera->fov);

	memory->scene.lightPos = v3f(3.0f, 8.0f, 10.0f);

	initMesh(&memory->cube, 8, 8, 12, memory->cubeMemory, (char*)"../data/cube.obj");
	memory->cube.objectTransform.translate(0.0f, 3.5f, 5.5f);
	memory->cube.objectTransform.setScale(0.6f, 0.6f, 0.6f);
	memory->cube.worldTransform =
		memory->projectionTransform * memory->viewTransform * memory->cube.objectTransform;

	initMesh(&memory->monkey, 507, 507, 968, memory->monkeyMemory, (char*)"../data/monkey.obj");
	memory->monkey.objectTransform.translate(0.0f, 3.5f, 5.5f);
	memory->monkey.worldTransform =
		memory->projectionTransform * memory->viewTransform * memory->monkey.objectTransform;

	memory->rotationAngle = 0;
}

exportDLL APP_UPDATE_CALL(appUpdate)
{
    Mesh* mesh = &memory->monkey;

	clearArray(memory->zBuffer, sizeof(memory->zBuffer)/sizeof(s32), 999999);

	memory->rotationAngle = (memory->rotationAngle + 1) % 360;
	mesh->objectTransform.setRotation(0.0f, DEGREE_TO_RAD(memory->rotationAngle), 0.0f);
	mesh->worldTransform = memory->projectionTransform * memory->viewTransform * mesh->objectTransform;
	
	fillBuffer(screen, 0xFF4D2177);

	fillBuffer(&memory->cameraBuffer, 0xFF604580);
	drawMesh(&memory->cameraBuffer, memory->zBuffer, &memory->scene, mesh);

	//TODO(denis): I want everything to draw directly into the screen, so this should also go away
	drawBitmap(screen, &memory->cameraBuffer, memory->cameraBufferPos);
	
	memory->lastMousePos = input->mouse.pos;
}
