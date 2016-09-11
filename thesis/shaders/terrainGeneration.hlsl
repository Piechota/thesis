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

	uint NeighborsTilesLeft;
	uint NeighborsTilesRight;
	uint NeighborsTilesTop;
	uint NeighborsTilesBottom;

	uint NeighborsTilesTopLeft;
	uint NeighborsTilesTopRight;
	uint NeighborsTilesBottomLeft;
	uint NeighborsTilesBottomRight;

	uint EdgesData; 
	uint DataOffset;
}

#define TOP_LOD 0x0001
#define BOTTOM_LOD 0x0002
#define RIGHT_LOD 0x0004
#define LEFT_LOD 0x0008
#define TOP_LEFT_LOD 0x0010
#define TOP_RIGHT_LOD 0x0020
#define BOTTOM_LEFT_LOD 0x0040
#define BOTTOM_RIGHT_LOD 0x0080

#define TOP_LOW_LOD 0x0100
#define BOTTOM_LOW_LOD 0x0200
#define RIGHT_LOW_LOD 0x0400
#define LEFT_LOW_LOD 0x0800
#define TOP_LEFT_LOW_LOD 0x1000
#define TOP_RIGHT_LOW_LOD 0x2000
#define BOTTOM_LEFT_LOW_LOD 0x4000
#define BOTTOM_RIGHT_LOW_LOD 0x8000

#define TOP_TILE 0x00010000
#define BOTTOM_TILE 0x00020000
#define RIGHT_TILE 0x00040000
#define LEFT_TILE 0x00080000
#define TOP_LEFT_TILE TOP_TILE | LEFT_TILE
#define TOP_RIGHT_TILE TOP_TILE | RIGHT_TILE
#define BOTTOM_LEFT_TILE BOTTOM_TILE | LEFT_TILE
#define BOTTOM_RIGHT_TILE BOTTOM_TILE | RIGHT_TILE

#ifdef TERRAIN_VERTICES
Texture2D Heightmap : register(t0);
RWStructuredBuffer<VertexFormat> Vertices : register(u0);

#ifdef TERRAIN_VERTICES_NOR_TAN
void GetTriangleNormTan( int id0, int id1, int tanID, float u, float3 position, inout float3 normal, inout float3 tan )
{
	float3 V0 = Vertices[id0].position - position;
	float3 V1 = Vertices[id1].position - position;
	float3 tanV = Vertices[tanID].position - position;
	normal += normalize( cross( V0, V1 ) );
	tan += normalize(tanV / (Vertices[tanID].uv.x - u) );
}

void CalculateLightInTile(uint2 dispatchThreadID, uint vertexID, inout float3 normal, inout float3 tan)
{
	int vBottom = vertexID - VerticesOnEdge;
	int vTop = vertexID + VerticesOnEdge;
	int vRight = vertexID + 1;
	int vRightBottom = vBottom + 1;
	int vLeft = vertexID - 1;
	int vLeftTop = vTop - 1;

	float3 position = Vertices[vertexID].position;
	float u = Vertices[vertexID].uv.x;

	if (0 < dispatchThreadID.y)
	{
		if (0 < dispatchThreadID.x)
		{
			//. _x
			// \ |
			//   .
			GetTriangleNormTan(vBottom, vLeft, vLeft, u, position, normal, tan);
		}

		if (dispatchThreadID.x < VerticesOnEdge - 1)
		{
			//x_.
			//|\|
			//.-.
			GetTriangleNormTan(vRight, vRightBottom, vRight, u, position, normal, tan);
			GetTriangleNormTan(vRightBottom, vBottom, vRightBottom, u, position, normal, tan);
		}
	}

	if (dispatchThreadID.y < VerticesOnEdge - 1)
	{
		if (0 < dispatchThreadID.x)
		{
			//._.
			//|\|
			//.-x
			GetTriangleNormTan(vLeft, vLeftTop, vLeft, u, position, normal, tan);
			GetTriangleNormTan(vLeftTop, vTop, vLeftTop, u, position, normal, tan);
		}

		if (dispatchThreadID.x < VerticesOnEdge - 1)
		{
			//.
			//|\
			//x-.
			GetTriangleNormTan(vTop, vRight, vRight, u, position, normal, tan);
		}
	}
}
void CalculateLightOnLodEdge( uint2 coord, uint vertexID, uint idOffset, uint dataOffset, bool lod, bool lowLod, bool oddID )
{
	if (lod)
	{
		uint2 lodCoord = coord >> 1;
		uint lodVerticesOnEdge = (VerticesOnEdge >> 1) + 1;
		uint sameVertexID = lodVerticesOnEdge * lodCoord.y + lodCoord.x + dataOffset;
		if (oddID)
		{
			float3 halfNormal = 
				normalize(
					lerp(
						Vertices[sameVertexID].normal
						, Vertices[sameVertexID+idOffset].normal
						, 0.5f));
			float3 halfTan = 
				normalize(
					lerp(
						Vertices[sameVertexID].tan
						, Vertices[sameVertexID + idOffset].tan
						,0.5f));
	
			Vertices[vertexID].normal = normalize(Vertices[vertexID].normal + halfNormal);
			Vertices[vertexID].tan = normalize(Vertices[vertexID].tan + halfTan);
		}
		else
		{
			float3 normal = normalize(Vertices[vertexID].normal + Vertices[sameVertexID].normal);
			float3 tan = normalize(Vertices[vertexID].tan + Vertices[sameVertexID].tan);
	
			Vertices[vertexID].normal = normal;
			Vertices[vertexID].tan = tan;
			Vertices[sameVertexID].normal = normal;
			Vertices[sameVertexID].tan = tan;
		}
	}
	else if (lowLod)
	{
		uint2 lowLodCoord = coord << 1;
		uint lowLodVerticesOnEdge = (VerticesOnEdge << 1) - 1;
		uint sameVertexID = lowLodVerticesOnEdge * lowLodCoord.y + lowLodCoord.x + dataOffset;
	
		float3 normal = normalize(Vertices[vertexID].normal + Vertices[sameVertexID].normal);
		float3 tan = normalize(Vertices[vertexID].tan + Vertices[sameVertexID].tan);
	
		Vertices[vertexID].normal = normal;
		Vertices[vertexID].tan = tan;
		Vertices[sameVertexID].normal = normal;
		Vertices[sameVertexID].tan = tan;
	}
	else
	{
		uint sameVertexID = VerticesOnEdge * coord.y + coord.x  + dataOffset;
		float3 normal = normalize(Vertices[vertexID].normal + Vertices[sameVertexID].normal);
		float3 tan = normalize(Vertices[vertexID].tan + Vertices[sameVertexID].tan);
		Vertices[vertexID].normal = normal;
		Vertices[vertexID].tan = tan;
		Vertices[sameVertexID].normal = normal;
		Vertices[sameVertexID].tan = tan;
	}
}
#endif
#ifdef TERRAIN_VERTICES_POS
float GetHeight( float2 normalUV, float2 dir, bool lod, bool oddID, bool fEdge, bool sEdge, bool tEdge )
{
	if (lod)
	{
		if (oddID && (tEdge || fEdge))
		{
			float height0 = Heightmap.Load(int3(mad(normalUV + dir, HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0)).x;
			float height1 = Heightmap.Load(int3(mad(normalUV - dir, HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0)).x;

			return (height0 + height1) * 0.5f;
		}

		if (sEdge)
		{
			if (oddID)
			{
				float height0 = Heightmap.Load(int3(mad(normalUV + float2(-UvQuadSize, +UvQuadSize), HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0)).x;
				float height1 = Heightmap.Load(int3(mad(normalUV + float2(+UvQuadSize, -UvQuadSize), HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0)).x;

				return (height0 + height1) * 0.5f;
			}
			else
			{
				float height0 = Heightmap.Load(int3(mad(normalUV + dir.yx, HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0)).x;
				float height1 = Heightmap.Load(int3(mad(normalUV - dir.yx, HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes, 0)).x;

				return (height0 + height1) * 0.5f;
			}
		}
	}
	float2 uv = mad(normalUV, HeightmapRect.xx, HeightmapRect.yz) * HeightmapRes;
	return Heightmap.Load(int3(uv, 0)).x;
}
float3 GetPosition(float2 normalUV, uint2 dispatchThreadID)
{
	float height = 0.f;
	bool lod = false;
	float2 dir = 0.f;
	bool oddID = false;
	bool fEdge = false;
	bool sEdge = false;
	bool tEdge = false;

	if ((EdgesData & TOP_LOD && VerticesOnEdge - 4 < dispatchThreadID.y)
		|| (EdgesData & TOP_LEFT_LOD && VerticesOnEdge - 4 < dispatchThreadID.y && dispatchThreadID.x < 3)
		|| (EdgesData & TOP_RIGHT_LOD && VerticesOnEdge - 4 < dispatchThreadID.y && VerticesOnEdge - 4 < dispatchThreadID.x))
	{
		lod = true;
		dir = float2(UvQuadSize, 0.f);
		oddID = dispatchThreadID.x & 1;
		fEdge = dispatchThreadID.y == VerticesOnEdge - 1;
		sEdge = dispatchThreadID.y == VerticesOnEdge - 2;
		tEdge = dispatchThreadID.y == VerticesOnEdge - 3;
	}

	if ((EdgesData & BOTTOM_LOD && dispatchThreadID.y < 3)
		|| (EdgesData & BOTTOM_LEFT_LOD && dispatchThreadID.y < 3 && dispatchThreadID.x < 3)
		|| (EdgesData & BOTTOM_RIGHT_LOD && dispatchThreadID.y < 3 && VerticesOnEdge - 4 < dispatchThreadID.x))
	{
		lod = true;
		dir = float2(UvQuadSize, 0.f);
		oddID = dispatchThreadID.x & 1;
		fEdge = dispatchThreadID.y == 0;
		sEdge = dispatchThreadID.y == 1;
		tEdge = dispatchThreadID.y == 2;
	}

	if (EdgesData & RIGHT_LOD && VerticesOnEdge - 4 < dispatchThreadID.x)
	{
		lod = true;
		dir = float2(0.f, UvQuadSize);
		oddID = dispatchThreadID.y & 1;
		fEdge = dispatchThreadID.x == VerticesOnEdge - 1;
		sEdge = dispatchThreadID.x == VerticesOnEdge - 2;
		tEdge = dispatchThreadID.x == VerticesOnEdge - 3;
	}

	if (EdgesData & LEFT_LOD && dispatchThreadID.x < 3)
	{
		lod = true;
		dir = float2(0.f, UvQuadSize);
		oddID = dispatchThreadID.y & 1;
		fEdge = dispatchThreadID.x == 0;
		sEdge = dispatchThreadID.x == 1;
		tEdge = dispatchThreadID.x == 2;
	}

	height = GetHeight(normalUV, dir, lod, oddID, fEdge, sEdge, tEdge);

	return float3(normalUV.x * WorldTileSize, height * TerrainHeight, normalUV.y * WorldTileSize)
		+ float3(TilePosition.x, 0.f, TilePosition.y);
}
#endif

[numthreads( 22, 22, 1 )]
void TerrainVertices( uint3 dispatchThreadID : SV_DispatchThreadID )
{
	bool inScope = dispatchThreadID.x < VerticesOnEdge && dispatchThreadID.y < VerticesOnEdge;
	int vertexID = mad(VerticesOnEdge, dispatchThreadID.y, dispatchThreadID.x) + DataOffset;
	if (inScope)
	{
#ifdef TERRAIN_VERTICES_POS
		float2 normUV = dispatchThreadID.xy * UvQuadSize;
		Vertices[vertexID].position = GetPosition(normUV, dispatchThreadID.xy);
		Vertices[vertexID].uv = mad(normUV, HeightmapRect.xx, HeightmapRect.yz);
#endif

#ifdef TERRAIN_VERTICES_NOR_TAN
#ifdef TERRAIN_VERTICES_NOR_TAN_PASS0
		float3 normal = float3(0.f, 0.f, 0.f);
		float3 tan = float3(0.f, 0.f, 0.f);
		CalculateLightInTile(dispatchThreadID.xy, vertexID, normal, tan);

		Vertices[vertexID].normal = normalize(normal);
		Vertices[vertexID].tan = normalize(tan);
#endif
#ifdef TERRAIN_VERTICES_NOR_TAN_PASS1
		if (!(EdgesData & TOP_LEFT_TILE && dispatchThreadID.x == 0 && dispatchThreadID.y == VerticesOnEdge - 1)
			&& !(EdgesData & BOTTOM_RIGHT_TILE && dispatchThreadID.x == VerticesOnEdge - 1 && dispatchThreadID.y == 0)
			&& !(EdgesData & BOTTOM_LEFT_TILE && dispatchThreadID.x == 0 && dispatchThreadID.y == 0))
		{
			if (EdgesData & TOP_RIGHT_TILE && dispatchThreadID.x == VerticesOnEdge - 1 && dispatchThreadID.y ==		VerticesOnEdge - 1)
			{
				uint sameVertexID0 = VerticesOnEdge + NeighborsTilesTop - 1;
				uint sameVertexID1 = NeighborsTilesTopRight;
				uint sameVertexID2 = mad(VerticesOnEdge, VerticesOnEdge - 1, NeighborsTilesRight);
				
				float3 normal 
					= normalize(
						Vertices[vertexID].normal + Vertices[sameVertexID0].normal +
						Vertices[sameVertexID1].normal + Vertices[sameVertexID2].normal
					);
				float3 tan
					= normalize(
						Vertices[vertexID].tan + Vertices[sameVertexID0].tan +
						Vertices[sameVertexID1].tan + Vertices[sameVertexID2].tan
					);

				Vertices[vertexID].normal = normal;
				Vertices[vertexID].tan = tan;
				Vertices[sameVertexID0].normal = normal;
				Vertices[sameVertexID0].tan = tan;
				Vertices[sameVertexID1].normal = normal;
				Vertices[sameVertexID1].tan = tan;
				Vertices[sameVertexID2].normal = normal;
				Vertices[sameVertexID2].tan = tan;
			}
			else
			{
				uint2 coord = 0;
				uint idOffset = 0;
				uint dataOffset = 0;
				bool calculate = false;
				bool lod = false;
				bool lowLod = false;
				bool oddID = false;

				if (EdgesData & TOP_TILE && dispatchThreadID.y == VerticesOnEdge - 1)
				{
					coord.x = dispatchThreadID.x;
					idOffset = 1;
					dataOffset = NeighborsTilesTop;
					calculate = true;
					lod = EdgesData & TOP_LOD;
					lowLod = EdgesData & TOP_LOW_LOD;
					oddID = dispatchThreadID.x & 1;
				}
				else if (EdgesData & LEFT_TILE && dispatchThreadID.x == 0)
				{
					coord.x = VerticesOnEdge - 1;
					coord.y = dispatchThreadID.y;
					idOffset = (VerticesOnEdge >> 1) + 1;
					dataOffset = NeighborsTilesLeft;
					calculate = true;
					lod = EdgesData & LEFT_LOD;
					lowLod = EdgesData & LEFT_LOW_LOD;
					oddID = dispatchThreadID.y & 1;
				}

				if (calculate)
				{
					CalculateLightOnLodEdge(coord, vertexID, idOffset, dataOffset, lod, lowLod, oddID);
				}
			}
		}
#endif
#endif
	}
}
#endif

#ifdef TERRAIN_INDICES
RWStructuredBuffer<uint> g_indices : register(u0);

void rot( uint n, uint rx, uint ry, inout uint x, inout uint y ) 
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

uint xy2d( uint n, uint x, uint y ) 
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
		uint qIndex = xy2d(QuadsOnEdge, dispatchThreadID.x, dispatchThreadID.y );

		uint vLeftBottom = mad(VerticesOnEdge, qIndex / QuadsOnEdge, qIndex % QuadsOnEdge);
		uint vLeftTop = vLeftBottom + VerticesOnEdge;
		uint vRightBottom = vLeftBottom + 1;
		uint vRightTop = vLeftTop + 1;

		uint startIndex = qIndex * 6;
		g_indices[startIndex + 0] = vLeftBottom;
		g_indices[startIndex + 1] = vLeftTop;
		g_indices[startIndex + 2] = vRightBottom;

		g_indices[startIndex + 3] = vRightBottom;
		g_indices[startIndex + 4] = vLeftTop;
		g_indices[startIndex + 5] = vRightTop;
	}
}
#endif