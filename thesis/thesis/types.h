#pragma once

template<class Key, class Value>
struct TPair
{
	Key m_key;
	Value m_value;

	TPair() {}
	TPair( Key const& key, Value const& value )
		: m_key( key )
		, m_value( value )
	{}
};
