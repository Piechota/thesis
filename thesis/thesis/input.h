#pragma once
#include "pch.h"
#include "types.h"

class CInputManager;
extern CInputManager GInputManager;

enum EKeys
{
	MSG_DESTROY,
	MSG_CLOSE,
	K_ESCAPE,
	K_F2,
	K_SPACE = 32,
	K_W = 'w',
	K_S = 's',
	K_D = 'd',
	K_A = 'a',
	K_LESS = 44,
	K_MORE = 46
};


typedef TPair<char, bool> tKeyState;

class IInputObserver
{
public:
	virtual void Input( EKeys key ) = 0;
};

class CInputManager
{
private:
	std::vector<IInputObserver*> m_observers;
	std::vector<tKeyState> m_keys;
	HWND m_hWnd;

	void SetKey( char const& key, bool const& value )
	{
		for ( tKeyState& pair : m_keys )
		{
			if ( pair.m_key == key )
			{
				pair.m_value = value;
			}
		}
	}

public:
	void Init()
	{
		m_keys.push_back( tKeyState( 'W', false ) );
		m_keys.push_back( tKeyState( 'A', false ) );
		m_keys.push_back( tKeyState( 'S', false ) );
		m_keys.push_back( tKeyState( 'D', false ) );
		m_keys.push_back( tKeyState( ' ', false ) );
	}
	bool IsKeyDown( char const& key )
	{
		for ( tKeyState const& pair : m_keys )
		{
			if ( pair.m_key == key )
			{
				return pair.m_value;
			}
		}

		return false;
	}
	void AddObserver( IInputObserver* observer )
	{
		m_observers.push_back( observer );
	}
	static LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch ( message )
		{
		case WM_DESTROY:
			for ( IInputObserver* observer : GInputManager.m_observers )
			{
				observer->Input( MSG_DESTROY );
			}
			break;
		case WM_CLOSE:
			for ( IInputObserver* observer : GInputManager.m_observers )
			{
				observer->Input( MSG_CLOSE );
			}
			break;
		case WM_KEYDOWN:
		{
			switch ( wParam )
			{
			case VK_ESCAPE:
				for ( IInputObserver* observer : GInputManager.m_observers )
					observer->Input( K_ESCAPE );
				break;
			case VK_F2:
				for ( IInputObserver* observer : GInputManager.m_observers )
					observer->Input( K_F2 );
				break;
			case VK_SPACE:
				GInputManager.SetKey( static_cast<char const>(wParam), true );
				for ( IInputObserver* observer : GInputManager.m_observers )
					observer->Input( K_SPACE );
				break;
			default:
				GInputManager.SetKey( static_cast<char const>(wParam), true );
				break;
			}
			break;
		}
		case WM_KEYUP:
		{
			switch ( wParam )
			{
			default:
				GInputManager.SetKey( static_cast<char const>(wParam), false );
				break;
			}
			break;
		}
		case WM_CHAR:
		{
			for ( IInputObserver* observer : GInputManager.m_observers )
				observer->Input( (EKeys)(wParam) );
		}
		break;
		}

		return DefWindowProc( hWnd, message, wParam, lParam );
	}
	void GetMousePosition( Vec2i& point )
	{
		GetCursorPos( (POINT*)(&point) );
		ScreenToClient( m_hWnd, (POINT*)(&point) );
	}
	void SetMousePosition( Vec2i point )
	{
		ClientToScreen( m_hWnd, (POINT*)(&point) );
		SetCursorPos( point.x, point.y );
	}
	void SetShowMouse( bool show )
	{
		ShowCursor( show );
	}
	void SetHWND( HWND hWnd )
	{
		m_hWnd = hWnd;
	}
};
