struct VertexFormat
{
    float3 position;
    float3 normal;
    float3 tan;
    float2 uv;
};

cbuffer TerrainInfo : register(b0)
{
	float3 HeightmapRect;
	float WorldTileSize;

	float TerrainHeight;
	float UvQuadSize;
	float2 TilePosition;

	uint2 HeightmapRes;
	uint VerticesOnEdge;
	uint QuadsOnEdge;

	uint EdgesData; 
	uint DataOffset;
}

#define TOP_LOD 0x0001
#define BOTTOM_LOD 0x0002
#define RIGHT_LOD 0x0004
#define LEFT_LOD 0x0008

#define TOP_TILE 0x0100
#define BOTTOM_TILE 0x0200
#define RIGHT_TILE 0x0400
#define LEFT_TILE 0x0800

#ifdef TERRAIN_VERTICES
Texture2D Heightmap : register(t0);
RWStructuredBuffer<VertexFormat> Vertices : register(u0);

void GetTriangleNormTan( const int id0, const int id1, const float u, const float3 position, inout float3 normal, inout float3 tan )
{
	const float3 V0 = Vertices[id0].position - position;
	const float3 V1 = Vertices[id1].position - position;
	normal += normalize( cross( V0, V1 ) );
	tan += normalize( V0 / (Vertices[id0].uv.x - u) );
}

float3 GetPosition(float2 normalUV, uint2 dispatchThreadID)
{
	float height = 0.f;
	if ((EdgesData & TOP_LOD && dispatchThreadID.x & 1 && dispatchThreadID.y == VerticesOnEdge - 1) || (EdgesData & BOTTOM_LOD && dispatchThreadID.x & 1 && dispatchThreadID.y == 0))
	{
		float height0 = Heightmap.Load( int3(mad(normalUV + float2(UvQuadSize, 0.f), HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0) ).x;
		float height1 = Heightmap.Load( int3(mad(normalUV - float2(UvQuadSize, 0.f), HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0) ).x;
		height = (height0 + height1) * 0.5f;
	}
	else if ((EdgesData & RIGHT_LOD && dispatchThreadID.y & 1 && dispatchThreadID.x == VerticesOnEdge - 1) || (EdgesData & LEFT_LOD && dispatchThreadID.y & 1 && dispatchThreadID.x == 0))
	{
		float height0 = Heightmap.Load(int3(mad(normalUV + float2(0.f, UvQuadSize), HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0)).x;
		float height1 = Heightmap.Load(int3(mad(normalUV - float2(0.f, UvQuadSize), HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0)).x;
		height = (height0 + height1) * 0.5f;
	}
	else
	{
		float2 uv = mad(normalUV, HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes;
		height = Heightmap.Load(int3(uv, 0)).x;
	}

	return float3(normalUV.x * WorldTileSize, height * TerrainHeight, normalUV.y * WorldTileSize)
		+ float3(TilePosition.x, 0.f, TilePosition.y);
}

[numthreads( 22, 22, 1 )]
void TerrainVertices( uint3 dispatchThreadID : SV_DispatchThreadID )
{
	const bool inScope = dispatchThreadID.x < VerticesOnEdge && dispatchThreadID.y < VerticesOnEdge;
	const int vertexID = mad(VerticesOnEdge, dispatchThreadID.y, dispatchThreadID.x) + DataOffset;
#ifdef TERRAIN_VERTICES_POS
	if ( inScope )
	{
		const float2 normUV = dispatchThreadID.xy * UvQuadSize;
		Vertices[vertexID].position = GetPosition( normUV, dispatchThreadID.xy);
		Vertices[vertexID].uv = mad(normUV, HeightmapRect.xx, HeightmapRect.yz);
	}
#endif
#ifdef TERRAIN_VERTICES_NOR_TAN
	if ( inScope )
	{
		const int vBottom = vertexID - VerticesOnEdge;
		const int vTop = vertexID + VerticesOnEdge;
		const int vRight = vertexID + 1;
		const int vRightBottom = vBottom + 1;
		const int vLeft = vertexID - 1;
		const int vLeftTop = vTop - 1;

		float3 normal = 0.f;
		float3 tan = 0.f;

		const float3 position = Vertices[vertexID].position;
		const float u = Vertices[vertexID].uv.x;

		if ( 0 < dispatchThreadID.y )
		{
			if ( 0 < dispatchThreadID.x )
			{
				//. _x
				// \ |
				//   .
				GetTriangleNormTan( vLeft, vBottom, u, position, normal, tan );
			}

			if ( dispatchThreadID.x < VerticesOnEdge - 1 )
			{
				//x_.
				//|\|
				//.-.
				GetTriangleNormTan( vRight, vRightBottom, u, position, normal, tan );
				GetTriangleNormTan( vRightBottom, vBottom, u, position, normal, tan );
			}

			
		}

		if ( dispatchThreadID.y < VerticesOnEdge - 1 )
		{
			if ( 0 < dispatchThreadID.x )
			{
				//._.
				//|\|
				//.-x
				GetTriangleNormTan( vLeft, vLeftTop, u, position, normal, tan );
				GetTriangleNormTan( vLeftTop, vTop, u, position, normal, tan );
			}

			if ( dispatchThreadID.x < VerticesOnEdge - 1 )
			{
				//.
				//|\
				//x-.
				GetTriangleNormTan( vRight, vTop, u, position, normal, tan );
			}
		}

		Vertices[vertexID].normal = normalize( normal );
		Vertices[vertexID].tan = normalize( tan );
	}
#endif
}
#endif

#ifdef TERRAIN_INDICES
RWStructuredBuffer<uint> g_indices : register(u0);

void rot( const uint n, const uint rx, const uint ry, inout uint x, inout uint y ) 
{
	if ( ry == 0 ) 
	{
		if ( rx == 1 ) 
		{
			x = n - 1 - x;
			y = n - 1 - y;
		}

		//Swap x and y
		uint t = x;
		x = y;
		y = t;
	}
}

uint xy2d( const uint n, uint x, uint y ) 
{
	uint rx, ry, s, d = 0;
	[unroll(7)]
	for ( s = (n >> 1); s>0; s >>= 1 )
	{
		rx = (x & s) > 0;
		ry = (y & s) > 0;
		d += s * s * ((3 * rx) ^ ry);
		rot( s, rx, ry, x, y );
	}
	return d;
}

[numthreads( 32, 32, 1 )]
void TerrainIndices( uint3 dispatchThreadID : SV_DispatchThreadID )
{
	if ( dispatchThreadID.x < QuadsOnEdge && dispatchThreadID.y < QuadsOnEdge)
	{
		const uint qIndex = xy2d(QuadsOnEdge, dispatchThreadID.x, dispatchThreadID.y );

		const uint vLeftBottom = mad(VerticesOnEdge, qIndex / QuadsOnEdge, qIndex % QuadsOnEdge);
		const uint vLeftTop = vLeftBottom + VerticesOnEdge;
		const uint vRightBottom = vLeftBottom + 1;
		const uint vRightTop = vLeftTop + 1;

		const uint startIndex = qIndex * 6;
		g_indices[startIndex + 0] = vLeftBottom;
		g_indices[startIndex + 1] = vLeftTop;
		g_indices[startIndex + 2] = vRightBottom;

		g_indices[startIndex + 3] = vRightBottom;
		g_indices[startIndex + 4] = vLeftTop;
		g_indices[startIndex + 5] = vRightTop;
	}
}
#endif