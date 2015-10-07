
#pragma once

#include <algorithm>
#include <assert.h>

namespace Network
{
	class SmartData
	{
		size_t			m_offset;
		size_t			m_size;
		unsigned char	*m_data;
		bool			m_deleteOnRelease;

		/** Checks if we have enough space to write the requested bytes.
			If not, makes the memory grow by reallocating.
		@remarks
			Each time the container grows, it grows by 50% + requestedLength
		*/
		void growToFit( size_t requestedLength )
		{
			if( m_offset + requestedLength > m_size )
			{
				assert( m_deleteOnRelease && "Can't write to a buffer that doesn't belong to us!!!" );
				if( !m_deleteOnRelease )
					throw "Can't write to a buffer that doesn't belong to us!!!";

				//Reallocate
				size_t newSize = m_offset + (m_offset >> 1) + requestedLength;
				unsigned char *tmpBuffer = new unsigned char[newSize];
				memcpy( tmpBuffer, m_data, m_size );
				delete m_data;
				m_data = tmpBuffer;
				m_size = newSize;
			}
		}

		void copy( const SmartData &copy )
		{
			if( m_deleteOnRelease )
				delete m_data;

			m_offset	= 0;
			m_size		= copy.m_size;

			if( m_size )
			{
				m_data				= new unsigned char[m_size];
				m_deleteOnRelease	= true;
				memcpy( m_data, copy.m_data, m_size );
			}
			else
			{
				m_data				= 0;
				m_deleteOnRelease	= false;
			}
		}

	public:
		SmartData() :
			m_offset( 0 ),
			m_size( 0 ),
			m_data( 0 ),
			m_deleteOnRelease( false )
		{
		}

		/** Main constructor
		@param
			Estimated size (in bytes) that the smart data will grow to. Failing to feed an
			accurate estimation could result in costly memory reallocations or consuming
			too much memory.
		*/
		SmartData( size_t estimatedSize ) :
			m_offset( 0 ),
			m_size( estimatedSize ),
			m_data( 0 ),
			m_deleteOnRelease( true )
		{
			m_data = new unsigned char[m_size];
		}

		/** Constructor when there is an external array of data to feed, and we just want to
			encapsulate this pointer.
		@param
			Pointer to the data
		@param
			Size of this data
		@param
			True if we should have our own memory copy. False if the pointer is shared.
			When False, the memory will NOT be deleted when the destructor is called.
			When False, make sure the pointer doesn't become a dangling ptr.
		*/
		SmartData( void *data, size_t length, bool copy ) :
			m_offset( 0 ),
			m_size( length ),
			m_data( reinterpret_cast<unsigned char*>( data ) ),
			m_deleteOnRelease( copy )
		{
			if( copy )
			{
				m_data = new unsigned char[m_size];
				memcpy( m_data, data, m_size );
			}
		}

		SmartData( const SmartData &copy ) :
			m_offset( 0 ),
			m_size( 0 ),
			m_data( 0 ),
			m_deleteOnRelease( false )
		{
			this->copy( copy );
		}

		~SmartData()
		{
			if( m_deleteOnRelease )
				delete m_data;
			m_data = 0;
		}

		void* getBasePtr()					{ return m_data; }
		void* getCurrentPtr()				{ return m_data + m_offset; }
		size_t getOffset() const			{ return m_offset; }

		size_t getCapacity() const			{ return m_size; }

		const void* getBasePtr() const		{ return m_data; }
		const void* getCurrentPtr() const	{ return m_data + m_offset; }

		SmartData& operator = ( const SmartData &right )
		{
			this->copy( right );
			return *this;
		}

		/** Steals the ownership from another smart data so that when that object gets destroyed it
			doesn't free the pointer, we will.
		*/
		void stealOwnershipFrom( SmartData &original )
		{
			assert( original.m_deleteOnRelease &&
					"Stealing ownership from SmartData that don't own their buffers!" );
			*this = original;
			original.m_deleteOnRelease = false;
		}

		void seekSet( size_t pos )			{ m_offset = std::min( pos, m_size ); }
		void seekCur( int pos )
		{
			m_offset += pos;
			m_offset = std::min( m_offset, m_size );
		}

		/** Appends the input data at the end of ours, both starting from the cursor they're
			currently pointing at, by the amount in bytes specified in the second parameter
		@remarks
			Advances BOTH cursors
		@param
			The data to append to the end of ours, starting from our cursor (not m_size!)
		@param
			The amount of data to transfer from 'right', in bytes. right.m_offset + size
			should be smaller than the size (we do safety checks, + assert in debug mode)
		*/
		void appendAtCursor( SmartData &right, size_t size )
		{
			assert( right.m_offset + size <= right.m_size );

			size = std::min( right.m_size - right.m_offset, size );
			growToFit( size );

			memcpy( m_data + m_offset, right.m_data + right.m_offset, size );
			this->m_offset += size;
			right.m_offset += size;
		}

		/** Appends the input data at the end of ours, both starting from the cursor they're
			currently pointing at, by the amount in bytes specified in the second parameter
		@remarks
			Advances only OUR cursor, since input param is const
		@param
			The data to append to the end of ours, starting from our cursor (not m_size!)
		@param
			The amount of data to transfer from 'right', in bytes. right.m_offset + size
			should be smaller than the size (we do safety checks, + assert in debug mode)
		*/
		void appendAtCursor( const SmartData &right, size_t size )
		{
			assert( right.m_offset + size <= right.m_size );

			size = std::min( right.m_size - right.m_offset, size );
			growToFit( size );

			memcpy( m_data + m_offset, right.m_data + right.m_offset, size );
			this->m_offset += size;
		}

		/// Write by copying a raw pointer in memory. Advances the cursor
		void write( const unsigned char *data, size_t length )
		{
			growToFit( length );
			memcpy( m_data + m_offset, data, length );
			m_offset += length;
		}

		/// Write a single value. Advances the cursor
		template <typename T> void write( const T &val )
		{
			growToFit( sizeof( T ) );
			memcpy( m_data + m_offset, &val, sizeof( T ) );
			m_offset += sizeof( T );
		}

		/// Reads a single value, without advancing the cursor
		template <typename T> void peek( T &outVal ) const
		{
			assert( m_offset + sizeof( T ) <= m_size );
			memcpy( &outVal, m_data + m_offset, sizeof( T ) );
		}

		/// Reads a single value using function's return, without advancing the cursor.
		template <typename T> T peek() const
		{
			T retVal;
			assert( m_offset + sizeof( T ) <= m_size );
			memcpy( &retVal, m_data + m_offset, sizeof( T ) );
			return retVal;
		}

		/// Reads a single value. Advances the cursor
		template <typename T> void read( T &outVal )
		{
			assert( m_offset + sizeof( T ) <= m_size );
			memcpy( &outVal, m_data + m_offset, sizeof( T ) );
			m_offset += sizeof( T );
		}

		/// Reads a single value using function's return. Advances the cursor
		template <typename T> T read()
		{
			T retVal;
			assert( m_offset + sizeof( T ) <= m_size );
			memcpy( &retVal, m_data + m_offset, sizeof( T ) );
			m_offset += sizeof( T );
			return retVal;
		}

		/// Read by copying a raw pointer in memory. Advances the cursor
		void read( unsigned char *data, size_t length )
		{
			assert( m_offset + length <= m_size );
			memcpy( data, m_data + m_offset, length );
			m_offset += length;
		}
	};
}
