#pragma once
#include <DirectXMath.h>
#include <math.h>

using namespace DirectX;

namespace MathConsts
{
	float const PI = 3.14159265359f;
	float const DegToRad = PI / 180.f;
}

struct Vec2
{
	union
	{
		struct
		{
			float x, y;
		};
		float data[2];
	};


	Vec2()
		: x( 0.f )
		, y( 0.f )
	{}
	Vec2( Vec2 const& v )
		: x( v.x )
		, y( v.y )
	{}
	Vec2( float const x, float const y )
		: x( x )
		, y( y )
	{}

	void Set( float const x, float const y )
	{
		this->x = x;
		this->y = y;
	}
};

struct Vec2i
{
	union
	{
		struct
		{
			int x, y;
		};
		int data[2];
	};

	Vec2i()
		: x( 0 )
		, y( 0 )
	{}
	Vec2i( Vec2i const& v )
		: x( v.x )
		, y( v.y )
	{}
	Vec2i( int const x, int const y )
		: x( x )
		, y( y )
	{}
	void operator -=( Vec2i const& v )
	{
		x -= v.x;
		y -= v.y;
	}
};

struct Vec2u
{
	union
	{
		struct
		{
			unsigned int x, y;
		};
		unsigned int data[2];
	};

	Vec2u()
		: x( 0 )
		, y( 0 )
	{}
	Vec2u( Vec2u const& v )
		: x( v.x )
		, y( v.y )
	{}
	Vec2u( unsigned int const x, unsigned int const y )
		: x( x )
		, y( y )
	{}
	void Set( unsigned int const x, unsigned int const y )
	{
		this->x = x;
		this->y = y;
	}
	void operator -=( Vec2u const& v )
	{
		x -= v.x;
		y -= v.y;
	}
};

struct Vec3
{
	union
	{
		struct
		{
			float x, y, z;
		};
		float data[3];
	};

	void Normalize()
	{
		float const l = 1.f / static_cast<float>( sqrt( x * x + y * y + z * z ) );
		x *= l;
		y *= l;
		z *= l;
	}

	float LengthSq()
	{
		return x * x + y * y + z * z;
	}

	void Set( float const _x, float const _y, float const _z )
	{
		x = _x;
		y = _y;
		z = _z;
	}

	Vec3()
		: x( 0.f )
		, y( 0.f )
		, z( 0.f )
	{}
	Vec3( Vec3 const& v )
		: x( v.x )
		, y( v.y )
		, z( v.z )
	{}
	Vec3( float const x, float const y, float const z )
		: x( x )
		, y( y )
		, z( z )
	{}
	void operator +=( Vec3 const& v )
	{
		x += v.x;
		y += v.y;
		z += v.z;
	}
	void operator -=( Vec3 const& v )
	{
		x -= v.x;
		y -= v.y;
		z -= v.z;
	}

	Vec3 operator *( float const s ) const
	{
		return Vec3( x * s, y * s, z * s );
	}

	Vec3 operator -() const
	{
		return Vec3( -x, -y, -z );
	}
	Vec3 operator +( Vec3 const& v ) const
	{
		return Vec3( x + v.x, z + v.z, z + v.z );
	}
	Vec3 operator -( Vec3 const& v ) const
	{
		return Vec3( x - v.x, z - v.z, z - v.z );
	}
};

struct Vec4
{
	union
	{
		struct
		{
			float x, y, z, w;
		};
		XMVECTOR vec;
	};

	Vec4()
	{}
	Vec4( float const x, float const y, float const z, float const w )
		: x( x )
		, y( y )
		, z( z )
		, w( w )
	{}
	Vec4( XMVECTOR const& v )
	{
		vec = v;
	}
	void Set( float const x, float const y, float const z, float const w )
	{
		this->x = x;
		this->y = y;
		this->z = z;
		this->w = w;
	}
	operator XMVECTOR() const
	{
		return vec;
	}
	void Normalize()
	{
		float const l = 1.f / static_cast<float>( sqrt( x * x + y * y + z * z + w * w ) );
		x *= l;
		y *= l;
		z *= l;
		w *= l;
	}
	Vec4 operator *( float const s )
	{
		return XMVectorScale( vec, s );
	}
	Vec4 operator +( Vec4 const& v )
	{
		return XMVectorAdd( vec, v );
	}
	static Vec4 Cross( Vec4 const& v0, Vec4 const& v1 )
	{
		return XMVector3Cross( v0, v1 );
	}
};

struct Quaternion
{
	union
	{
		struct
		{
			float x, y, z, w;
		};
		XMVECTOR quaternion;
	};

	Quaternion()
	{}
	Quaternion( float const x, float const y, float const z, float const w )
		: x( x )
		, y( y )
		, z( z )
		, w( w )
	{}
	Quaternion( XMVECTOR const& q )
	{
		quaternion = q;
	}
	operator XMVECTOR() const
	{
		return quaternion;
	}
	void operator =( Quaternion const& q )
	{
		quaternion = q.quaternion;
	}
	Quaternion operator *( Quaternion const& q ) const
	{
		return XMQuaternionMultiply( quaternion, q );
	}
	void operator *=( Quaternion const& q )
	{
		quaternion = XMQuaternionMultiply( quaternion, q );
	}
	static Quaternion CreateRotation( Vec4 const& axis, float const radAngle )
	{
		return XMQuaternionRotationAxis( axis, radAngle );
	}
	static Quaternion CreateRotationRollPitchYaw( float const roll, float const pitch, float const yaw )
	{
		return XMQuaternionRotationRollPitchYaw( pitch, yaw, roll );
	}
	static Quaternion CreateRotationRollPitchYaw( Vec4 rotation )
	{
		return XMQuaternionRotationRollPitchYawFromVector( rotation );
	}
	static Quaternion CreateQuaternionIdentity()
	{
		return XMQuaternionIdentity();
	}
};

struct Matrix4x4
{
	union
	{
		XMMATRIX matrix;
		XMFLOAT4X4 data;
	};

	Matrix4x4()
	{}
	Matrix4x4( XMMATRIX const& mat )
	{
		matrix = mat;
	}

	void operator=( Matrix4x4 const& mat )
	{
		matrix = mat.matrix;
	}
	Matrix4x4 operator*( Matrix4x4 const& mat ) const
	{
		return XMMatrixMultiply( matrix, mat.matrix );
	}

	void operator *=( Matrix4x4 const& mat )
	{
		matrix = XMMatrixMultiply( matrix, mat.matrix );
	}

	Vec3 TransformVector( Vec3 const& vec ) const
	{
		Vec4 vec4( vec.x, vec.y, vec.z, 0.f );
		vec4 = XMVector4Transform( vec4, matrix );
		return Vec3( vec4.x, vec4.y, vec4.z );
	}

	Vec3 TransformPoint( Vec3 const& vec ) const
	{
		Vec4 vec4( vec.x, vec.y, vec.z, 1.f );
		vec4 = XMVector4Transform( vec4, matrix );
		return Vec3( vec4.x, vec4.y, vec4.z );
	}

	operator XMMATRIX() const
	{
		return matrix;
	}

	static Matrix4x4 Transpose( Matrix4x4 const& m )
	{
		return XMMatrixTranspose( m );
	}
	static Matrix4x4 CreateFromQuaternion( Quaternion const& q )
	{
		return XMMatrixRotationQuaternion( q );
	}
	static Matrix4x4 CreateTranslateMatrix( Vec3 const& v )
	{
		return XMMatrixTranslation( v.x, v.y, v.z );
	}
	static Matrix4x4 CreateScaleMatrix( Vec3 const& s )
	{
		return XMMatrixScaling( s.x, s.y, s.z );
	}
	static Matrix4x4 CreateOrthoLH( float const viewWidth, float const viewHeight, float const nearZ, float const farZ )
	{
		return XMMatrixOrthographicLH( viewWidth, viewHeight, nearZ, farZ );
	}
	static Matrix4x4 CreateOrthoRH( float const viewWidth, float const viewHeight, float const nearZ, float const farZ )
	{
		return XMMatrixOrthographicRH( viewWidth, viewHeight, nearZ, farZ );
	}
	static Matrix4x4 CreatePerspectiveLH( float const viewWidth, float const viewHeight, float const nearZ, float const farZ )
	{
		return XMMatrixPerspectiveLH( viewWidth, viewHeight, nearZ, farZ );
	}
	static Matrix4x4 CreatePerspectiveRH( float const viewWidth, float const viewHeight, float const nearZ, float const farZ )
	{
		return XMMatrixPerspectiveRH( viewWidth, viewHeight, nearZ, farZ );
	}
	static Matrix4x4 CreatePerspectiveFovLH( float const fovY, float const aspect, float const nearZ, float const farZ )
	{
		return XMMatrixPerspectiveFovLH( fovY, aspect, nearZ, farZ );
	}
	static Matrix4x4 CreatePerspectiveFovRH( float const fovY, float const aspect, float const nearZ, float const farZ )
	{
		return XMMatrixPerspectiveFovRH( fovY, aspect, nearZ, farZ );
	}
	static Matrix4x4 Inverse( Matrix4x4 const& mat )
	{
		return XMMatrixInverse( nullptr, mat );
	}

	static Matrix4x4 const Identity;
};
