/*
 * Snapshot functions
 *
 * Copyright (C) 2009-2026, Joachim Metz <joachim.metz@gmail.com>
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <memory.h>
#include <system_string.h>
#include <types.h>

#include "libvmdk_libcerror.h"
#include "libvmdk_libuna.h"
#include "libvmdk_snapshot.h"
#include "libvmdk_system_string.h"

/* Creates a snapshot
 * Make sure the value snapshot is referencing, is set to NULL
 * Returns 1 if successful or -1 on error
 */
int libvmdk_snapshot_initialize(
     libvmdk_snapshot_t **snapshot,
     libcerror_error_t **error )
{
	static char *function = "libvmdk_snapshot_initialize";

	if( snapshot == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshot.",
		 function );

		return( -1 );
	}
	if( *snapshot != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid snapshot value already set.",
		 function );

		return( -1 );
	}
	*snapshot = memory_allocate_structure(
	             libvmdk_snapshot_t );

	if( *snapshot == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create snapshot.",
		 function );

		goto on_error;
	}
	if( memory_set(
	     *snapshot,
	     0,
	     sizeof( libvmdk_snapshot_t ) ) == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear snapshot.",
		 function );

		memory_free(
		 *snapshot );

		*snapshot = NULL;

		return( -1 );
	}
	return( 1 );

on_error:
	if( *snapshot != NULL )
	{
		memory_free(
		 *snapshot );

		*snapshot = NULL;
	}
	return( -1 );
}

/* Frees a snapshot
 * Returns 1 if successful or -1 on error
 */
int libvmdk_snapshot_free(
     libvmdk_snapshot_t **snapshot,
     libcerror_error_t **error )
{
	static char *function = "libvmdk_snapshot_free";

	if( snapshot == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshot.",
		 function );

		return( -1 );
	}
	if( *snapshot != NULL )
	{
		if( ( *snapshot )->filename != NULL )
		{
			memory_free(
			 ( *snapshot )->filename );
		}
		if( ( *snapshot )->utf8_filename != NULL )
		{
			memory_free(
			 ( *snapshot )->utf8_filename );
		}
		memory_free(
		 *snapshot );

		*snapshot = NULL;
	}
	return( 1 );
}

/* Sets the descriptor filename
 * Returns 1 if successful or -1 on error
 */
int libvmdk_snapshot_set_filename(
     libvmdk_snapshot_t *snapshot,
     const system_character_t *filename,
     size_t filename_size,
     libcerror_error_t **error )
{
	static char *function   = "libvmdk_snapshot_set_filename";
	size_t utf8_string_size = 0;

	if( snapshot == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshot.",
		 function );

		return( -1 );
	}
	if( filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid filename.",
		 function );

		return( -1 );
	}
	if( ( filename_size == 0 )
	 || ( filename_size > ( (size_t) MEMORY_MAXIMUM_ALLOCATION_SIZE / sizeof( system_character_t ) ) ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid filename size value out of bounds.",
		 function );

		return( -1 );
	}
	if( snapshot->filename != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid snapshot - filename value already set.",
		 function );

		return( -1 );
	}
	snapshot->filename = system_string_allocate(
	                      filename_size );

	if( snapshot->filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create filename.",
		 function );

		return( -1 );
	}
	if( system_string_copy(
	     snapshot->filename,
	     filename,
	     filename_size ) == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_COPY_FAILED,
		 "%s: unable to copy filename.",
		 function );

		goto on_error;
	}
	snapshot->filename_size = filename_size;

	if( libvmdk_system_string_size_to_narrow_string(
	     filename,
	     filename_size,
	     &utf8_string_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine UTF-8 filename size.",
		 function );

		goto on_error;
	}
	if( utf8_string_size > (size_t) MEMORY_MAXIMUM_ALLOCATION_SIZE )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid UTF-8 filename size value exceeds maximum.",
		 function );

		goto on_error;
	}
	snapshot->utf8_filename = (uint8_t *) memory_allocate(
	                                      sizeof( uint8_t ) * utf8_string_size );

	if( snapshot->utf8_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create UTF-8 filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_system_string_copy_to_narrow_string(
	     filename,
	     filename_size,
	     (char *) snapshot->utf8_filename,
	     utf8_string_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy filename to UTF-8 filename.",
		 function );

		goto on_error;
	}
	snapshot->utf8_filename_size = utf8_string_size;

	return( 1 );

on_error:
	if( snapshot->filename != NULL )
	{
		memory_free(
		 snapshot->filename );

		snapshot->filename = NULL;
	}
	if( snapshot->utf8_filename != NULL )
	{
		memory_free(
		 snapshot->utf8_filename );

		snapshot->utf8_filename = NULL;
	}
	snapshot->filename_size      = 0;
	snapshot->utf8_filename_size = 0;

	return( -1 );
}

/* Retrieves the size of the UTF-8 encoded descriptor filename
 * The returned size includes the end of string character
 * Returns 1 if successful or -1 on error
 */
int libvmdk_snapshot_get_utf8_filename_size(
     libvmdk_snapshot_t *snapshot,
     size_t *utf8_string_size,
     libcerror_error_t **error )
{
	static char *function = "libvmdk_snapshot_get_utf8_filename_size";

	if( snapshot == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshot.",
		 function );

		return( -1 );
	}
	if( utf8_string_size == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid UTF-8 string size.",
		 function );

		return( -1 );
	}
	*utf8_string_size = snapshot->utf8_filename_size;

	return( 1 );
}

/* Retrieves the UTF-8 encoded descriptor filename
 * The size should include the end of string character
 * Returns 1 if successful or -1 on error
 */
int libvmdk_snapshot_get_utf8_filename(
     libvmdk_snapshot_t *snapshot,
     uint8_t *utf8_string,
     size_t utf8_string_size,
     libcerror_error_t **error )
{
	static char *function = "libvmdk_snapshot_get_utf8_filename";

	if( snapshot == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshot.",
		 function );

		return( -1 );
	}
	if( utf8_string == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid UTF-8 string.",
		 function );

		return( -1 );
	}
	if( snapshot->utf8_filename_size > utf8_string_size )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_TOO_SMALL,
		 "%s: UTF-8 string is too small.",
		 function );

		return( -1 );
	}
	if( memory_copy(
	     utf8_string,
	     snapshot->utf8_filename,
	     snapshot->utf8_filename_size ) == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_COPY_FAILED,
		 "%s: unable to copy UTF-8 filename.",
		 function );

		return( -1 );
	}
	return( 1 );
}

/* Retrieves the size of the UTF-16 encoded descriptor filename
 * The returned size includes the end of string character
 * Returns 1 if successful or -1 on error
 */
int libvmdk_snapshot_get_utf16_filename_size(
     libvmdk_snapshot_t *snapshot,
     size_t *utf16_string_size,
     libcerror_error_t **error )
{
	static char *function = "libvmdk_snapshot_get_utf16_filename_size";

	if( snapshot == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshot.",
		 function );

		return( -1 );
	}
	if( libuna_utf16_string_size_from_utf8_stream(
	     snapshot->utf8_filename,
	     snapshot->utf8_filename_size,
	     utf16_string_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine UTF-16 filename size.",
		 function );

		return( -1 );
	}
	return( 1 );
}

/* Retrieves the UTF-16 encoded descriptor filename
 * The size should include the end of string character
 * Returns 1 if successful or -1 on error
 */
int libvmdk_snapshot_get_utf16_filename(
     libvmdk_snapshot_t *snapshot,
     uint16_t *utf16_string,
     size_t utf16_string_size,
     libcerror_error_t **error )
{
	static char *function = "libvmdk_snapshot_get_utf16_filename";

	if( snapshot == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshot.",
		 function );

		return( -1 );
	}
	if( libuna_utf16_string_copy_from_utf8_stream(
	     utf16_string,
	     utf16_string_size,
	     snapshot->utf8_filename,
	     snapshot->utf8_filename_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy UTF-8 filename to UTF-16 filename.",
		 function );

		return( -1 );
	}
	return( 1 );
}
