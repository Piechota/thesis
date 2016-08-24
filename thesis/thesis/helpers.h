#pragma once

__forceinline void CheckFailed( long res )
{
	if ( res < 0 )
	{
		throw;
	}
}	

FORCEINLINE wchar_t* Char2WChar( char const* c )
{
	size_t newsize = strlen( c ) + 1;
	wchar_t * wcstring = new wchar_t[newsize];

	// Convert char* string to a wchar_t* string.
	size_t convertedChars = 0;
	mbstowcs_s( &convertedChars, wcstring, newsize, c, _TRUNCATE );
	return wcstring;
}