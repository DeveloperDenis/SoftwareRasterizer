#if !defined(RASTERIZE_H_)
#define RASTERIZE_H_

//TODO(denis): dunno if I like this having it's own file like this

extern uint32 calculateLight(Vector3f normal, Vector3f lightDirection);

static inline int32 barycentricHelper(Vector2 v1, Vector2 v2, Vector2 inputVector)
{
	int32 result;
	result = (v1.y - v2.y)*inputVector.x + (v2.x - v1.x)*inputVector.y + v1.x*v2.y - v2.x*v1.y;

	return result;
}

static inline Vector3f getBarycentricCoords(Vector2 v1, Vector2 v2, Vector2 v3, Vector2 v,
											ZBufferData* zBufferData)
{
	Vector3f coefficients;
	
    coefficients.x = (real32)barycentricHelper(v2, v3, v) /
		zBufferData->denominators[0];

    coefficients.y = (real32)barycentricHelper(v1, v3, v) /
		zBufferData->denominators[1];

    coefficients.z = (real32)barycentricHelper(v1, v2, v) /
		zBufferData->denominators[2];
	
	return coefficients;
}

//NOTE(denis): uses the flat shading technique (not much of a technique really, though)
static void fillLineFlat(Bitmap* screen, int32 lineY, int32 startX, int32 endX,
						 ZBufferData* zBufferData, uint32 colour)
{
	startX = MAX(startX, 0);
	endX = MIN(endX, (int32)screen->width);
	
	for (int32 x = startX; x < endX; ++x)
	{
		Vector3f barycentric =
			getBarycentricCoords(zBufferData->points[0].xy, zBufferData->points[1].xy,
								 zBufferData->points[2].xy, V2(x, lineY), zBufferData);
		
		int32 zValue = (int32)(barycentric.x*zBufferData->points[0].z +
							   barycentric.y*zBufferData->points[1].z +
							   barycentric.z*zBufferData->points[2].z);
		
		//NOTE(denis): don't draw anything that is on the near or far clipping plane
		if (zValue != 0 && zValue != ZSCALE_FACTOR &&
			zValue < zBufferData->zBuffer[lineY*screen->width + x])
		{
		    zBufferData->zBuffer[lineY*screen->width + x] = zValue;
		    drawPoint(screen, x, lineY, colour);
		}
	}
}

//NOTE(denis): these assume that the points are already sorted from lowest to highest y-value
// also assume that screenPoints[0].x < screenPoints[1].x
static void rasterizeFlatTopTriangle(Bitmap* screen, Vector3 screenPoints[3],
									 ZBufferData* zBufferData, uint32 colour)
{
	//TODO(denis): dunno if I want to keep this or not
	fillLineFlat(screen, screenPoints[0].y, screenPoints[0].x, screenPoints[1].x, zBufferData, colour);
	
	real32 invSlopeP31 = inverseSlope(screenPoints[2].xy, screenPoints[0].xy);
	real32 invSlopeP32 = inverseSlope(screenPoints[2].xy, screenPoints[1].xy);
	
	for (int32 y = screenPoints[2].y; y > screenPoints[0].y; --y)
	{
		if (y < 0 || y >= (int32)screen->height)
			continue;

		int32 x31 = (int32)ceil((y - screenPoints[2].y)*invSlopeP31 + screenPoints[2].x);
		int32 x32 = (int32)ceil((y - screenPoints[2].y)*invSlopeP32 + screenPoints[2].x);

		int32 startX = MIN(x31, x32);
		int32 endX = MAX(x31, x32);

		fillLineFlat(screen, y, startX, endX, zBufferData, colour);
	}
}
//NOTE(denis): assumes screenPoints[1].x < screenPoints[2].x
static void rasterizeFlatBottomTriangle(Bitmap* screen, Vector3 screenPoints[3],
										ZBufferData* zBufferData, uint32 colour)
{
	//TODO(denis): dunno if I want to keep this or not.
	fillLineFlat(screen, screenPoints[1].y, screenPoints[1].x, screenPoints[2].x, zBufferData, colour);
	
	real32 invSlopeP12 = inverseSlope(screenPoints[0].xy, screenPoints[1].xy);
	real32 invSlopeP13 = inverseSlope(screenPoints[0].xy, screenPoints[2].xy);

	for (int32 y = screenPoints[0].y; y < screenPoints[1].y; ++y)
	{
		if (y < 0 || y >= (int32)screen->height)
			continue;
		
		int32 x12 = (int32)ceil((y - screenPoints[0].y)*invSlopeP12 + screenPoints[0].x);
		int32 x13 = (int32)ceil((y - screenPoints[0].y)*invSlopeP13 + screenPoints[0].x);

		int32 startX = MIN(x12, x13);
		int32 endX = MAX(x12, x13);
		
		fillLineFlat(screen, y, startX, endX, zBufferData, colour);
	}
}

//TODO(denis): not a good place for the swap functions, but since they are only
// used here they will live here
static inline void swap(Vector3f* a, Vector3f* b)
{
	Vector3f temp = *a;
	*a = *b;
	*b = temp;
}
static inline void swap(Vector3* a, Vector3* b)
{
	Vector3 temp = *a;
	*a = *b;
	*b = temp;
}
static inline void swap(uint32* a, uint32* b)
{
	uint32 temp = *a;
	*a = *b;
	*b = temp;
}

static void rasterize(Bitmap* screen, Vector3 screenPoints[3], int32* zBuffer, uint32 colour)
{
	if (screenPoints[0].y > screenPoints[1].y)
		swap(&screenPoints[0], &screenPoints[1]);
	if (screenPoints[1].y > screenPoints[2].y)
		swap(&screenPoints[1], &screenPoints[2]);
	if (screenPoints[0].y > screenPoints[1].y)
		swap(&screenPoints[0], &screenPoints[1]);
	
	ZBufferData zBufferData;
	zBufferData.zBuffer = zBuffer;
	zBufferData.points[0] = screenPoints[0];
	zBufferData.points[1] = screenPoints[1];
	zBufferData.points[2] = screenPoints[2];
	zBufferData.denominators[0] = (real32)barycentricHelper(screenPoints[1].xy, screenPoints[2].xy, screenPoints[0].xy);
	zBufferData.denominators[1] = (real32)barycentricHelper(screenPoints[0].xy, screenPoints[2].xy, screenPoints[1].xy);
	zBufferData.denominators[2] = (real32)barycentricHelper(screenPoints[0].xy, screenPoints[1].xy, screenPoints[2].xy);
	
	if (screenPoints[1].y == screenPoints[2].y)
	{
		rasterizeFlatBottomTriangle(screen, screenPoints, &zBufferData, colour);
	}
	else if (screenPoints[0].y == screenPoints[1].y)
	{
		rasterizeFlatTopTriangle(screen, screenPoints, &zBufferData, colour);
	}
	else
	{
		Vector3 midPoint1 = screenPoints[1];
		Vector3 midPoint2 = screenPoints[1];

		real32 invSlopeLong = inverseSlope(screenPoints[0].xy, screenPoints[2].xy);
	    midPoint2.x = (int32)round((screenPoints[1].y - screenPoints[0].y)*invSlopeLong + screenPoints[0].x);

		Vector3 leftPoint;
		Vector3 rightPoint;
		if (screenPoints[0].x < screenPoints[1].x)
		{
			leftPoint = midPoint2;
			rightPoint = midPoint1;
			rightPoint.x += 1;
		}
		else
		{
			leftPoint = midPoint1;
			leftPoint.x -= 1;
			rightPoint = midPoint2;
		}

		Vector3 topTrianglePoints[] = {screenPoints[0], leftPoint, rightPoint};
		Vector3 bottomTrianglePoints[] = {leftPoint, rightPoint, screenPoints[2]};
		
		rasterizeFlatBottomTriangle(screen, topTrianglePoints, &zBufferData, colour);

		if (screenPoints[1].y >= 0 && screenPoints[1].y < (int32)screen->height)
		{
			fillLineFlat(screen, screenPoints[1].y, leftPoint.x, rightPoint.x, &zBufferData, colour);
		}
		
		rasterizeFlatTopTriangle(screen, bottomTrianglePoints, &zBufferData, colour);
	}
}

#endif
