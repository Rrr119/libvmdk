/*
 * Handle functions
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
#include <narrow_string.h>
#include <system_string.h>
#include <types.h>
#include <wide_string.h>

#if defined( HAVE_SYS_STAT_H ) || defined( WINAPI )
#include <sys/stat.h>
#endif

#include "libvmdk_debug.h"
#include "libvmdk_definitions.h"
#include "libvmdk_descriptor_file.h"
#include "libvmdk_extent_descriptor.h"
#include "libvmdk_extent_file.h"
#include "libvmdk_extent_table.h"
#include "libvmdk_extent_values.h"
#include "libvmdk_handle.h"
#include "libvmdk_grain_data.h"
#include "libvmdk_grain_table.h"
#include "libvmdk_io_handle.h"
#include "libvmdk_libbfio.h"
#include "libvmdk_libcdata.h"
#include "libvmdk_libcerror.h"
#include "libvmdk_libcfile.h"
#include "libvmdk_libcnotify.h"
#include "libvmdk_libcpath.h"
#include "libvmdk_libcthreads.h"
#include "libvmdk_libfcache.h"
#include "libvmdk_libfdata.h"
#include "libvmdk_snapshot.h"
#include "libvmdk_system_string.h"

#define LIBVMDK_MAXIMUM_SNAPSHOT_CHAIN_DEPTH	1024

static int libvmdk_internal_handle_get_file_modification_time(
     const system_character_t *filename,
     uint64_t *modification_time,
     libcerror_error_t **error );

static int libvmdk_internal_handle_resolve_parent_filename(
     const system_character_t *filename,
     const uint8_t *parent_filename,
     size_t parent_filename_size,
     system_character_t **resolved_filename,
     size_t *resolved_filename_size,
     libcerror_error_t **error );

static int libvmdk_internal_handle_discover_snapshot_chain(
     const system_character_t *filename,
     int access_flags,
     libcdata_array_t **snapshots_array,
     libcerror_error_t **error );

static int libvmdk_internal_handle_open_snapshot_chain(
     libvmdk_handle_t *handle,
     libcdata_array_t *snapshots_array,
     int snapshot_index,
     int access_flags,
     libcerror_error_t **error );

/* Creates a handle
 * Make sure the value handle is referencing, is set to NULL
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_initialize(
     libvmdk_handle_t **handle,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_initialize";

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( *handle != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid handle value already set.",
		 function );

		return( -1 );
	}
	internal_handle = memory_allocate_structure(
	                   libvmdk_internal_handle_t );

	if( internal_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create handle.",
		 function );

		goto on_error;
	}
	if( memory_set(
	     internal_handle,
	     0,
	     sizeof( libvmdk_internal_handle_t ) ) == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear handle.",
		 function );

		memory_free(
		 internal_handle );

		return( -1 );
	}
	if( libvmdk_io_handle_initialize(
	     &( internal_handle->io_handle ),
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create IO handle.",
		 function );

		goto on_error;
	}
	if( libcdata_array_initialize(
	     &( internal_handle->extent_values_array ),
	     0,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create extent values array.",
		 function );

		goto on_error;
	}
	if( libvmdk_extent_table_initialize(
	     &( internal_handle->extent_table ),
	     internal_handle->io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create extent table.",
		 function );

		goto on_error;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_initialize(
	     &( internal_handle->read_write_lock ),
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to initialize read/write lock.",
		 function );

		goto on_error;
	}
#endif
	internal_handle->maximum_number_of_open_handles = LIBBFIO_POOL_UNLIMITED_NUMBER_OF_OPEN_HANDLES;

	*handle = (libvmdk_handle_t *) internal_handle;

	return( 1 );

on_error:
	if( internal_handle != NULL )
	{
		if( internal_handle->extent_table != NULL )
		{
			libvmdk_extent_table_free(
			 &( internal_handle->extent_table ),
			 NULL );
		}
		if( internal_handle->extent_values_array != NULL )
		{
			libcdata_array_free(
			 &( internal_handle->extent_values_array ),
			 NULL,
			 NULL );
		}
		if( internal_handle->io_handle != NULL )
		{
			libvmdk_io_handle_free(
			 &( internal_handle->io_handle ),
			 NULL );
		}
		memory_free(
		 internal_handle );
	}
	return( -1 );
}

/* Frees a handle
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_free(
     libvmdk_handle_t **handle,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_free";
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( *handle != NULL )
	{
		internal_handle = (libvmdk_internal_handle_t *) *handle;

		if( ( internal_handle->extent_data_file_io_pool != NULL )
		 || ( internal_handle->descriptor_file != NULL ) )
		{
			if( libvmdk_handle_close(
			     *handle,
			     error ) != 0 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_IO,
				 LIBCERROR_IO_ERROR_CLOSE_FAILED,
				 "%s: unable to close handle.",
				 function );

				result = -1;
			}
		}
		*handle = NULL;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
		if( libcthreads_read_write_lock_free(
		     &( internal_handle->read_write_lock ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free read/write lock.",
			 function );

			result = -1;
		}
#endif
		if( libvmdk_extent_table_free(
		     &( internal_handle->extent_table ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free extent table.",
			 function );

			result = -1;
		}
		if( libcdata_array_free(
		     &( internal_handle->extent_values_array ),
		     (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_extent_values_free,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free extent values array.",
			 function );

			result = -1;
		}
		if( internal_handle->snapshots_array != NULL )
		{
			if( libcdata_array_free(
			     &( internal_handle->snapshots_array ),
			     (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
				 "%s: unable to free snapshots array.",
				 function );

				result = -1;
			}
		}
		if( libvmdk_io_handle_free(
		     &( internal_handle->io_handle ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free IO handle.",
			 function );

			result = -1;
		}
		memory_free(
		 internal_handle );
	}
	return( result );
}

/* Signals the handle to abort its current activity
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_signal_abort(
     libvmdk_handle_t *handle,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_signal_abort";

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->io_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing IO handle.",
		 function );

		return( -1 );
	}
	internal_handle->io_handle->abort = 1;

	return( 1 );
}

/* Opens a set of VMDK file(s)
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open(
     libvmdk_handle_t *handle,
     const char *filename,
     int access_flags,
     libcerror_error_t **error )
{
	libbfio_handle_t *file_io_handle           = NULL;
	libvmdk_extent_values_t *extent_values     = NULL;
	libvmdk_internal_handle_t *internal_handle = NULL;
	const char *data_files_path_end            = NULL;
	static char *function                      = "libvmdk_handle_open";
	size_t data_files_path_length              = 0;
	size_t filename_length                     = 0;
	int number_of_extents                      = 0;
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

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
	if( ( ( access_flags & LIBVMDK_ACCESS_FLAG_READ ) == 0 )
	 && ( ( access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) == 0 ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported access flags.",
		 function );

		return( -1 );
	}
	if( ( access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) != 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: write access currently not supported.",
		 function );

		return( -1 );
	}
	filename_length = narrow_string_length(
	                   filename );

	if( libbfio_file_initialize(
	     &file_io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create file IO handle.",
		 function );

		goto on_error;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libbfio_handle_set_track_offsets_read(
	     file_io_handle,
	     1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set track offsets read in file IO handle.",
		 function );

		goto on_error;
	}
#endif
	if( libbfio_file_set_name(
	     file_io_handle,
	     filename,
	     filename_length + 1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set name in file IO handle.",
		 function );

		goto on_error;
	}
	if( libvmdk_handle_open_file_io_handle(
	     handle,
	     file_io_handle,
	     access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open handle using a file IO handle.",
		 function );

		goto on_error;
	}
	if( libbfio_handle_free(
	     &file_io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
		 "%s: unable to free file IO handle.",
		 function );

		goto on_error;
	}
	/* Note that extent file names can be renamed hence for a single monolithic sparse
	 * or stream optimized image the filename instead of the extent data filename in
	 * the descriptor file is used.
	 */
/* TODO thread lock */
	if( ( internal_handle->disk_type == LIBVMDK_DISK_TYPE_MONOLITHIC_SPARSE )
	 || ( internal_handle->disk_type == LIBVMDK_DISK_TYPE_STREAM_OPTIMIZED ) )
	{
		if( libcdata_array_get_number_of_entries(
		     internal_handle->extent_values_array,
		     &number_of_extents,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve number of extents.",
			 function );

			goto on_error;
		}
		if( number_of_extents == 1 )
		{
			if( libcdata_array_get_entry_by_index(
			     internal_handle->extent_values_array,
			     0,
			     (intptr_t **) &extent_values,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
				 "%s: unable to retrieve extent values: 0 from array.",
				 function );

				goto on_error;
			}
			if( extent_values == NULL )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
				 "%s: missing extent values: 0.",
				 function );

				goto on_error;
			}
			if( extent_values->type == LIBVMDK_EXTENT_TYPE_SPARSE )
			{
				if( libvmdk_extent_values_set_alternate_filename(
				     extent_values,
				     filename,
				     filename_length,
				     error ) != 1 )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_RUNTIME,
					 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
					 "%s: unable to set alternate filename in extent values.",
					 function );

					goto on_error;
				}
#if defined( HAVE_DEBUG_OUTPUT )
				if( libcnotify_verbose != 0 )
				{
					libcnotify_printf(
					 "%s: alternate filename\t\t\t\t\t: %" PRIs_SYSTEM "\n",
					 function,
					 extent_values->alternate_filename );
				}
#endif
			}
		}
	}
	data_files_path_end = narrow_string_search_character_reverse(
	                       filename,
	                       (int) LIBCPATH_SEPARATOR,
	                       filename_length + 1 );

	if( data_files_path_end != NULL )
	{
		data_files_path_length = (size_t) ( data_files_path_end - filename ) + 1;
	}
	if( data_files_path_length > 0 )
	{
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
		if( libcthreads_read_write_lock_grab_for_write(
		     internal_handle->read_write_lock,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to grab read/write lock for writing.",
			 function );

			return( -1 );
		}
#endif
		if( libvmdk_extent_table_set_data_files_path(
		     internal_handle->extent_table,
		     filename,
		     data_files_path_length,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set data files path in extent table.",
			 function );

			result = -1;
		}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
		if( libcthreads_read_write_lock_release_for_write(
		     internal_handle->read_write_lock,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to release read/write lock for writing.",
			 function );

			return( -1 );
		}
#endif
	}
	return( result );

on_error:
	if( file_io_handle != NULL )
	{
		libbfio_handle_free(
		 &file_io_handle,
		 NULL );
	}
	return( -1 );
}

#if defined( HAVE_WIDE_CHARACTER_TYPE )

/* Opens a set of VMDK file(s)
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_wide(
     libvmdk_handle_t *handle,
     const wchar_t *filename,
     int access_flags,
     libcerror_error_t **error )
{
	libbfio_handle_t *file_io_handle           = NULL;
	libvmdk_extent_values_t *extent_values     = NULL;
	libvmdk_internal_handle_t *internal_handle = NULL;
	const wchar_t *data_files_path_end         = NULL;
	static char *function                      = "libvmdk_handle_open_wide";
	size_t data_files_path_length              = 0;
	size_t filename_length                     = 0;
	int number_of_extents                      = 0;
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

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
	if( ( ( access_flags & LIBVMDK_ACCESS_FLAG_READ ) == 0 )
	 && ( ( access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) == 0 ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported access flags.",
		 function );

		return( -1 );
	}
	if( ( access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) != 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: write access currently not supported.",
		 function );

		return( -1 );
	}
	filename_length = wide_string_length(
	                   filename );

	if( libbfio_file_initialize(
	     &file_io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create file IO handle.",
		 function );

		goto on_error;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libbfio_handle_set_track_offsets_read(
	     file_io_handle,
	     1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set track offsets read in file IO handle.",
		 function );

		goto on_error;
	}
#endif
	if( libbfio_file_set_name_wide(
	     file_io_handle,
	     filename,
	     filename_length + 1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set name in file IO handle.",
		 function );

		goto on_error;
	}
	if( libvmdk_handle_open_file_io_handle(
	     handle,
	     file_io_handle,
	     access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open handle using a file IO handle.",
		 function );

		goto on_error;
	}
	if( libbfio_handle_free(
	     &file_io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
		 "%s: unable to free file IO handle.",
		 function );

		goto on_error;
	}
	/* Note that extent file names can be renamed hence for a single monolithic sparse
	 * or stream optimized image the filename instead of the extent data filename in
	 * the descriptor file is used.
	 */
/* TODO thread lock */
	if( ( internal_handle->disk_type == LIBVMDK_DISK_TYPE_MONOLITHIC_SPARSE )
	 || ( internal_handle->disk_type == LIBVMDK_DISK_TYPE_STREAM_OPTIMIZED ) )
	{
		if( libcdata_array_get_number_of_entries(
		     internal_handle->extent_values_array,
		     &number_of_extents,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve number of extents.",
			 function );

			goto on_error;
		}
		if( number_of_extents == 1 )
		{
			if( libcdata_array_get_entry_by_index(
			     internal_handle->extent_values_array,
			     0,
			     (intptr_t **) &extent_values,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
				 "%s: unable to retrieve extent values: 0 from array.",
				 function );

				goto on_error;
			}
			if( extent_values == NULL )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
				 "%s: missing extent values: 0.",
				 function );

				goto on_error;
			}
			if( extent_values->type == LIBVMDK_EXTENT_TYPE_SPARSE )
			{
				if( libvmdk_extent_values_set_alternate_filename_wide(
				     extent_values,
				     filename,
				     filename_length,
				     error ) != 1 )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_RUNTIME,
					 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
					 "%s: unable to set alternate filename in extent values.",
					 function );

					goto on_error;
				}
#if defined( HAVE_DEBUG_OUTPUT )
				if( libcnotify_verbose != 0 )
				{
					libcnotify_printf(
					 "%s: alternate filename\t\t\t\t\t: %" PRIs_SYSTEM "\n",
					 function,
					 extent_values->alternate_filename );
				}
#endif
			}
		}
	}
	data_files_path_end = wide_string_search_character_reverse(
	                       filename,
	                       (wint_t) LIBCPATH_SEPARATOR,
	                       filename_length + 1 );

	if( data_files_path_end != NULL )
	{
		data_files_path_length = (size_t) ( data_files_path_end - filename ) + 1;
	}
	if( data_files_path_length > 0 )
	{
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
		if( libcthreads_read_write_lock_grab_for_write(
		     internal_handle->read_write_lock,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to grab read/write lock for writing.",
			 function );

			return( -1 );
		}
#endif
		if( libvmdk_extent_table_set_data_files_path_wide(
		     internal_handle->extent_table,
		     filename,
		     data_files_path_length,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set data files path in extent table.",
			 function );

			result = -1;
		}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
		if( libcthreads_read_write_lock_release_for_write(
		     internal_handle->read_write_lock,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to release read/write lock for writing.",
			 function );

			return( -1 );
		}
#endif
	}
	return( result );

on_error:
	if( file_io_handle != NULL )
	{
		libbfio_handle_free(
		 &file_io_handle,
		 NULL );
	}
	return( -1 );
}

#endif /* defined( HAVE_WIDE_CHARACTER_TYPE ) */

/* Opens a set of VMDK file(s) using a Basic File IO (bfio) handle
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_file_io_handle(
     libvmdk_handle_t *handle,
     libbfio_handle_t *file_io_handle,
     int access_flags,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_open_file_io_handle";
	uint8_t file_io_handle_opened_in_library   = 0;
	int bfio_access_flags                      = 0;
	int file_io_handle_is_open                 = 0;
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->descriptor_file != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid handle - descriptor file already set.",
		 function );

		return( -1 );
	}
	if( internal_handle->io_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing IO handle.",
		 function );

		return( -1 );
	}
	if( file_io_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file IO handle.",
		 function );

		return( -1 );
	}
	if( ( ( access_flags & LIBVMDK_ACCESS_FLAG_READ ) == 0 )
	 && ( ( access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) == 0 ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported access flags.",
		 function );

		return( -1 );
	}
	if( ( access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) != 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: write access currently not supported.",
		 function );

		return( -1 );
	}
	if( ( access_flags & LIBVMDK_ACCESS_FLAG_READ ) != 0 )
	{
		bfio_access_flags = LIBBFIO_ACCESS_FLAG_READ;
	}
	file_io_handle_is_open = libbfio_handle_is_open(
	                          file_io_handle,
	                          error );

	if( file_io_handle_is_open == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open file.",
		 function );

		goto on_error;
	}
	else if( file_io_handle_is_open == 0 )
	{
		if( libbfio_handle_open(
		     file_io_handle,
		     bfio_access_flags,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_OPEN_FAILED,
			 "%s: unable to open file IO handle.",
			 function );

			goto on_error;
		}
		file_io_handle_opened_in_library = 1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		goto on_error;
	}
#endif
	if( libvmdk_internal_handle_open_read(
	     internal_handle,
	     file_io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_READ_FAILED,
		 "%s: unable to read from file IO handle.",
		 function );

		result = -1;
	}
	else
	{
		internal_handle->access_flags = access_flags;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( file_io_handle_opened_in_library != 0 )
	{
		if( libbfio_handle_close(
		     file_io_handle,
		     error ) != 0 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_CLOSE_FAILED,
			 "%s: unable to close file IO handle.",
			 function );

			goto on_error;
		}
	}
	return( result );

on_error:
	if( file_io_handle_opened_in_library != 0 )
	{
		libbfio_handle_close(
		 file_io_handle,
		 error );
	}
	return( -1 );
}

/* Retrieves the modification time of a file
 * Returns 1 if successful or -1 on error
 */
static int libvmdk_internal_handle_get_file_modification_time(
     const system_character_t *filename,
     uint64_t *modification_time,
     libcerror_error_t **error )
{
#if defined( WINAPI )
	struct _stat64 file_stat;
#else
	char *narrow_filename = NULL;
	struct stat file_stat;
#endif
	static char *function = "libvmdk_internal_handle_get_file_modification_time";

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
	if( modification_time == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid modification time.",
		 function );

		return( -1 );
	}
#if defined( WINAPI )
	if( _wstat64(
	     filename,
	     &file_stat ) != 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to retrieve file status of: %" PRIs_SYSTEM ".",
		 function,
		 filename );

		return( -1 );
	}
#else
	{
		size_t narrow_filename_size = 0;

		if( libvmdk_system_string_size_to_narrow_string(
		     filename,
		     system_string_length(
		      filename ) + 1,
		     &narrow_filename_size,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_CONVERSION,
			 LIBCERROR_CONVERSION_ERROR_GENERIC,
			 "%s: unable to determine narrow filename size.",
			 function );

			return( -1 );
		}
		narrow_filename = narrow_string_allocate(
		                   narrow_filename_size );

		if( narrow_filename == NULL )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_MEMORY,
			 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to create narrow filename.",
			 function );

			return( -1 );
		}
		if( libvmdk_system_string_copy_to_narrow_string(
		     filename,
		     system_string_length(
		      filename ) + 1,
		     narrow_filename,
		     narrow_filename_size,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_CONVERSION,
			 LIBCERROR_CONVERSION_ERROR_GENERIC,
			 "%s: unable to copy narrow filename.",
			 function );

			memory_free(
			 narrow_filename );

			return( -1 );
		}
	}
	if( stat(
	     narrow_filename,
	     &file_stat ) != 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to retrieve file status of: %" PRIs_SYSTEM ".",
		 function,
		 filename );

		memory_free(
		 narrow_filename );

		return( -1 );
	}
	memory_free(
	 narrow_filename );
#endif
	*modification_time = (uint64_t) file_stat.st_mtime;

	return( 1 );
}

/* Resolves a parent filename relative to a descriptor filename
 * Returns 1 if successful or -1 on error
 */
static int libvmdk_internal_handle_resolve_parent_filename(
     const system_character_t *filename,
     const uint8_t *parent_filename,
     size_t parent_filename_size,
     system_character_t **resolved_filename,
     size_t *resolved_filename_size,
     libcerror_error_t **error )
{
	system_character_t *parent_filename_system = NULL;
	static char *function                      = "libvmdk_internal_handle_resolve_parent_filename";
	size_t directory_name_length               = 0;
	size_t filename_length                     = 0;
	size_t parent_filename_system_size         = 0;
	int parent_filename_is_absolute            = 0;

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
	if( parent_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid parent filename.",
		 function );

		return( -1 );
	}
	if( ( parent_filename_size == 0 )
	 || ( parent_filename_size > ( (size_t) MEMORY_MAXIMUM_ALLOCATION_SIZE - 1 ) ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid parent filename size value out of bounds.",
		 function );

		return( -1 );
	}
	if( resolved_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid resolved filename.",
		 function );

		return( -1 );
	}
	if( *resolved_filename != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid resolved filename value already set.",
		 function );

		return( -1 );
	}
	if( resolved_filename_size == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid resolved filename size.",
		 function );

		return( -1 );
	}
	if( libvmdk_system_string_size_from_narrow_string(
	     (const char *) parent_filename,
	     parent_filename_size,
	     &parent_filename_system_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine parent filename size.",
		 function );

		goto on_error;
	}
	parent_filename_system = system_string_allocate(
	                          parent_filename_system_size );

	if( parent_filename_system == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create parent filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_system_string_copy_from_narrow_string(
	     parent_filename_system,
	     parent_filename_system_size,
	     (const char *) parent_filename,
	     parent_filename_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy parent filename.",
		 function );

		goto on_error;
	}
	parent_filename_system_size = system_string_length(
	                               parent_filename_system ) + 1;

#if defined( WINAPI )
	if( ( parent_filename_system[ 0 ] == (system_character_t) '\\' )
	 || ( parent_filename_system[ 0 ] == (system_character_t) '/' )
	 || ( ( parent_filename_system_size > 2 )
	  &&  ( parent_filename_system[ 1 ] == (system_character_t) ':' ) ) )
#else
	if( parent_filename_system[ 0 ] == (system_character_t) '/' )
#endif
	{
		parent_filename_is_absolute = 1;
	}
	if( parent_filename_is_absolute != 0 )
	{
		*resolved_filename = system_string_allocate(
		                      parent_filename_system_size );

		if( *resolved_filename == NULL )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_MEMORY,
			 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to create resolved filename.",
			 function );

			goto on_error;
		}
		if( system_string_copy(
		     *resolved_filename,
		     parent_filename_system,
		     parent_filename_system_size ) == NULL )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_MEMORY,
			 LIBCERROR_MEMORY_ERROR_COPY_FAILED,
			 "%s: unable to copy resolved filename.",
			 function );

			goto on_error;
		}
		*resolved_filename_size = parent_filename_system_size;
	}
	else
	{
		filename_length = system_string_length(
		                  filename ) + 1;

		while( filename_length > 0 )
		{
			if( filename[ filename_length - 1 ] == (system_character_t) LIBCPATH_SEPARATOR )
			{
				directory_name_length = filename_length;

				break;
			}
			filename_length--;
		}
		if( directory_name_length == 0 )
		{
			*resolved_filename = parent_filename_system;

			parent_filename_system = NULL;

			*resolved_filename_size = parent_filename_system_size;
		}
		else
		{
#if defined( HAVE_WIDE_SYSTEM_CHARACTER )
			if( libcpath_path_join_wide(
			     resolved_filename,
			     resolved_filename_size,
			     filename,
			     directory_name_length,
			     parent_filename_system,
			     parent_filename_system_size - 1,
			     error ) != 1 )
#else
			if( libcpath_path_join(
			     resolved_filename,
			     resolved_filename_size,
			     filename,
			     directory_name_length,
			     parent_filename_system,
			     parent_filename_system_size - 1,
			     error ) != 1 )
#endif
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
				 "%s: unable to join parent filename.",
				 function );

				goto on_error;
			}
		}
	}
	if( parent_filename_system != NULL )
	{
		memory_free(
		 parent_filename_system );
	}
	return( 1 );

on_error:
	if( *resolved_filename != NULL )
	{
		memory_free(
		 *resolved_filename );

		*resolved_filename = NULL;
	}
	if( parent_filename_system != NULL )
	{
		memory_free(
		 parent_filename_system );
	}
	return( -1 );
}

/* Discovers the snapshot chain from a descriptor
 * The snapshots are stored from the base snapshot to the provided snapshot
 * Returns 1 if successful or -1 on error
 */
static int libvmdk_internal_handle_discover_snapshot_chain(
     const system_character_t *filename,
     int access_flags,
     libcdata_array_t **snapshots_array,
     libcerror_error_t **error )
{
	libcdata_array_t *safe_snapshots_array    = NULL;
	libvmdk_handle_t *snapshot_handle         = NULL;
	libvmdk_snapshot_t *snapshot              = NULL;
	system_character_t *current_filename      = NULL;
	system_character_t *parent_filename       = NULL;
	uint8_t *parent_filename_utf8             = NULL;
	static char *function                     = "libvmdk_internal_handle_discover_snapshot_chain";
	size_t current_filename_size              = 0;
	size_t parent_filename_size               = 0;
	size_t parent_filename_utf8_size          = 0;
	uint64_t modification_time                = 0;
	uint32_t content_identifier               = 0;
	uint32_t expected_content_identifier      = 0;
	uint32_t parent_content_identifier        = 0;
	uint8_t expected_content_identifier_set   = 0;
	int chain_depth                           = 0;
	int result                                = 0;

	if( snapshots_array == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshots array.",
		 function );

		return( -1 );
	}
	if( *snapshots_array != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid snapshots array value already set.",
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
	current_filename_size = system_string_length(
	                         filename ) + 1;

	current_filename = system_string_allocate(
	                    current_filename_size );

	if( current_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create current filename.",
		 function );

		goto on_error;
	}
	if( system_string_copy(
	     current_filename,
	     filename,
	     current_filename_size ) == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_COPY_FAILED,
		 "%s: unable to copy current filename.",
		 function );

		goto on_error;
	}
	if( libcdata_array_initialize(
	     &safe_snapshots_array,
	     0,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create snapshots array.",
		 function );

		goto on_error;
	}
	for( chain_depth = 0;
	     chain_depth < LIBVMDK_MAXIMUM_SNAPSHOT_CHAIN_DEPTH;
	     chain_depth++ )
	{
		if( libvmdk_handle_initialize(
		     &snapshot_handle,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
			 "%s: unable to initialize snapshot handle.",
			 function );

			goto on_error;
		}
#if defined( HAVE_WIDE_SYSTEM_CHARACTER )
		result = libvmdk_handle_open_wide(
		          snapshot_handle,
		          current_filename,
		          access_flags,
		          error );
#else
		result = libvmdk_handle_open(
		          snapshot_handle,
		          current_filename,
		          access_flags,
		          error );
#endif
		if( result != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_OPEN_FAILED,
			 "%s: unable to open snapshot: %" PRIs_SYSTEM ".",
			 function,
			 current_filename );

			goto on_error;
		}
		if( libvmdk_handle_get_content_identifier(
		     snapshot_handle,
		     &content_identifier,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot content identifier.",
			 function );

			goto on_error;
		}
		if( ( expected_content_identifier_set != 0 )
		 && ( content_identifier != expected_content_identifier ) )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
			 "%s: mismatch between parent content identifier: 0x%08" PRIx32 " and snapshot content identifier: 0x%08" PRIx32 ".",
			 function,
			 expected_content_identifier,
			 content_identifier );

			goto on_error;
		}
		if( libvmdk_snapshot_initialize(
		     &snapshot,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
			 "%s: unable to create snapshot.",
			 function );

			goto on_error;
		}
		if( libvmdk_snapshot_set_filename(
		     snapshot,
		     current_filename,
		     system_string_length(
		      current_filename ) + 1,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set snapshot filename.",
			 function );

			goto on_error;
		}
		if( libvmdk_internal_handle_get_file_modification_time(
		     current_filename,
		     &modification_time,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot modification time.",
			 function );

			goto on_error;
		}
		snapshot->modification_time     = modification_time;
		snapshot->content_identifier    = content_identifier;

		if( libvmdk_handle_get_disk_type(
		     snapshot_handle,
		     &( snapshot->disk_type ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot disk type.",
			 function );

			goto on_error;
		}
		if( libvmdk_handle_get_media_size(
		     snapshot_handle,
		     &( snapshot->media_size ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot media size.",
			 function );

			goto on_error;
		}
		result = libvmdk_handle_get_parent_content_identifier(
		          snapshot_handle,
		          &parent_content_identifier,
		          error );

		if( result == -1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot parent content identifier.",
			 function );

			goto on_error;
		}
		else if( result != 0 )
		{
			snapshot->parent_content_identifier     = parent_content_identifier;
			snapshot->parent_content_identifier_set = 1;
		}
		if( libcdata_array_prepend_entry(
		     safe_snapshots_array,
		     (intptr_t *) snapshot,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_APPEND_FAILED,
			 "%s: unable to prepend snapshot.",
			 function );

			goto on_error;
		}
		snapshot = NULL;

		if( ( result == 0 )
		 || ( parent_content_identifier == 0xffffffffUL ) )
		{
			expected_content_identifier_set = 0;

			break;
		}
		result = libvmdk_handle_get_utf8_parent_filename_size(
		          snapshot_handle,
		          &parent_filename_utf8_size,
		          error );
		if( result == -1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot parent filename size.",
			 function );

			goto on_error;
		}
		else if( ( result == 0 )
		      || ( parent_filename_utf8_size == 0 ) )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: missing parent filename for snapshot.",
			 function );

			goto on_error;
		}
		parent_filename_utf8 = (uint8_t *) memory_allocate(
		                                    sizeof( uint8_t ) * parent_filename_utf8_size );

		if( parent_filename_utf8 == NULL )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_MEMORY,
			 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to create parent filename.",
			 function );

			goto on_error;
		}
		result = libvmdk_handle_get_utf8_parent_filename(
		          snapshot_handle,
		          parent_filename_utf8,
		          parent_filename_utf8_size,
		          error );
		if( result != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot parent filename.",
			 function );

			goto on_error;
		}
		if( libvmdk_handle_free(
		     &snapshot_handle,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free snapshot handle.",
			 function );

			goto on_error;
		}
		if( libvmdk_internal_handle_resolve_parent_filename(
		     current_filename,
		     parent_filename_utf8,
		     parent_filename_utf8_size,
		     &parent_filename,
		     &parent_filename_size,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
			 "%s: unable to resolve parent filename.",
			 function );

			goto on_error;
		}
		memory_free(
		 parent_filename_utf8 );

		parent_filename_utf8 = NULL;

		if( system_string_compare(
		     current_filename,
		     parent_filename,
		     parent_filename_size ) == 0 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
			 "%s: snapshot chain contains a cycle.",
			 function );

			goto on_error;
		}
		memory_free(
		 current_filename );

		current_filename      = parent_filename;
		current_filename_size = parent_filename_size;
		parent_filename       = NULL;

		expected_content_identifier     = parent_content_identifier;
		expected_content_identifier_set = 1;
	}
	if( chain_depth >= LIBVMDK_MAXIMUM_SNAPSHOT_CHAIN_DEPTH )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: snapshot chain exceeds maximum depth.",
		 function );

		goto on_error;
	}
	if( snapshot_handle != NULL )
	{
		libvmdk_handle_free(
		 &snapshot_handle,
		 NULL );
	}
	if( current_filename != NULL )
	{
		memory_free(
		 current_filename );
	}
	*snapshots_array = safe_snapshots_array;

	return( 1 );

on_error:
	if( snapshot != NULL )
	{
		libvmdk_snapshot_free(
		 &snapshot,
		 NULL );
	}
	if( snapshot_handle != NULL )
	{
		libvmdk_handle_free(
		 &snapshot_handle,
		 NULL );
	}
	if( parent_filename_utf8 != NULL )
	{
		memory_free(
		 parent_filename_utf8 );
	}
	if( parent_filename != NULL )
	{
		memory_free(
		 parent_filename );
	}
	if( current_filename != NULL )
	{
		memory_free(
		 current_filename );
	}
	if( safe_snapshots_array != NULL )
	{
		libcdata_array_free(
		 &safe_snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );
	}
	return( -1 );
}

/* Opens a snapshot chain at a specific snapshot index
 * Returns 1 if successful or -1 on error
 */
static int libvmdk_internal_handle_open_snapshot_chain(
     libvmdk_handle_t *handle,
     libcdata_array_t *snapshots_array,
     int snapshot_index,
     int access_flags,
     libcerror_error_t **error )
{
	libvmdk_handle_t *current_handle           = NULL;
	libvmdk_handle_t *parent_handle            = NULL;
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_internal_handle_open_snapshot_chain";
	int parent_snapshot_index                  = 0;
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( snapshots_array == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshots array.",
		 function );

		return( -1 );
	}
	if( snapshot_index < 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: snapshot index value out of bounds.",
		 function );

		return( -1 );
	}
	if( internal_handle->descriptor_file != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid handle - descriptor file already set.",
		 function );

		return( -1 );
	}
	if( libcdata_array_get_entry_by_index(
	     snapshots_array,
	     snapshot_index,
	     (intptr_t **) &snapshot,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot: %d.",
		 function,
		 snapshot_index );

		return( -1 );
	}
#if defined( HAVE_WIDE_SYSTEM_CHARACTER )
	result = libvmdk_handle_open_wide(
	          handle,
	          snapshot->filename,
	          access_flags,
	          error );
#else
	result = libvmdk_handle_open(
	          handle,
	          snapshot->filename,
	          access_flags,
	          error );
#endif
	if( result != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open snapshot.",
		 function );

		goto on_error;
	}
	if( libvmdk_handle_open_extent_data_files(
	     handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open snapshot extent data files.",
		 function );

		goto on_error;
	}
	current_handle = handle;

	for( parent_snapshot_index = snapshot_index - 1;
	     parent_snapshot_index >= 0;
	     parent_snapshot_index-- )
	{
		if( libvmdk_handle_initialize(
		     &parent_handle,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
			 "%s: unable to initialize parent handle.",
			 function );

			goto on_error;
		}
		if( libcdata_array_get_entry_by_index(
		     snapshots_array,
		     parent_snapshot_index,
		     (intptr_t **) &snapshot,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve parent snapshot: %d.",
			 function,
			 parent_snapshot_index );

			goto on_error;
		}
#if defined( HAVE_WIDE_SYSTEM_CHARACTER )
		result = libvmdk_handle_open_wide(
		          parent_handle,
		          snapshot->filename,
		          access_flags,
		          error );
#else
		result = libvmdk_handle_open(
		          parent_handle,
		          snapshot->filename,
		          access_flags,
		          error );
#endif
		if( result != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_OPEN_FAILED,
			 "%s: unable to open parent snapshot.",
			 function );

			goto on_error;
		}
		if( libvmdk_handle_open_extent_data_files(
		     parent_handle,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_OPEN_FAILED,
			 "%s: unable to open parent snapshot extent data files.",
			 function );

			goto on_error;
		}
		if( libvmdk_handle_set_parent_handle(
		     current_handle,
		     parent_handle,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set parent snapshot handle.",
			 function );

			goto on_error;
		}
		( (libvmdk_internal_handle_t *) current_handle )->parent_handle_created_in_library = 1;

		current_handle = parent_handle;
		parent_handle  = NULL;
	}
	return( 1 );

on_error:
	if( parent_handle != NULL )
	{
		libvmdk_handle_free(
		 &parent_handle,
		 NULL );
	}
	if( ( internal_handle->parent_handle != NULL )
	 && ( internal_handle->parent_handle_created_in_library != 0 ) )
	{
		libvmdk_handle_free(
		 &( internal_handle->parent_handle ),
		 NULL );

		internal_handle->parent_handle_created_in_library = 0;
	}
	if( internal_handle->descriptor_file != NULL )
	{
		libvmdk_handle_close(
		 handle,
		 NULL );
	}
	return( -1 );
}

/* Opens a VMDK snapshot chain
 * Snapshot 0 is the base snapshot and the last snapshot is the provided snapshot
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_snapshot_chain(
     libvmdk_handle_t *handle,
     const char *filename,
     int access_flags,
     libcerror_error_t **error )
{
	libcdata_array_t *snapshots_array          = NULL;
	libvmdk_internal_handle_t *internal_handle = NULL;
	system_character_t *system_filename        = NULL;
	static char *function                      = "libvmdk_handle_open_snapshot_chain";
	size_t system_filename_size                = 0;
	int number_of_snapshots                    = 0;
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( libvmdk_system_string_size_from_narrow_string(
	     filename,
	     narrow_string_length(
	      filename ) + 1,
	     &system_filename_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine filename size.",
		 function );

		goto on_error;
	}
	system_filename = system_string_allocate(
	                   system_filename_size );

	if( system_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_system_string_copy_from_narrow_string(
	     system_filename,
	     system_filename_size,
	     filename,
	     narrow_string_length(
	      filename ) + 1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_discover_snapshot_chain(
	     system_filename,
	     access_flags,
	     &snapshots_array,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to discover snapshot chain.",
		 function );

		goto on_error;
	}
	if( libcdata_array_get_number_of_entries(
	     snapshots_array,
	     &number_of_snapshots,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of snapshots.",
		 function );

		goto on_error;
	}
	result = libvmdk_internal_handle_open_snapshot_chain(
	          handle,
	          snapshots_array,
	          number_of_snapshots - 1,
	          access_flags,
	          error );

	if( result != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open snapshot chain.",
		 function );

		goto on_error;
	}
	internal_handle->snapshots_array        = snapshots_array;
	internal_handle->selected_snapshot_index = number_of_snapshots - 1;
	snapshots_array                         = NULL;

	memory_free(
	 system_filename );

	return( 1 );

on_error:
	if( snapshots_array != NULL )
	{
		libcdata_array_free(
		 &snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );
	}
	if( system_filename != NULL )
	{
		memory_free(
		 system_filename );
	}
	return( -1 );
}

#if defined( HAVE_WIDE_CHARACTER_TYPE )

/* Opens a VMDK snapshot chain
 * Snapshot 0 is the base snapshot and the last snapshot is the provided snapshot
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_snapshot_chain_wide(
     libvmdk_handle_t *handle,
     const wchar_t *filename,
     int access_flags,
     libcerror_error_t **error )
{
	libcdata_array_t *snapshots_array          = NULL;
	libvmdk_internal_handle_t *internal_handle = NULL;
	system_character_t *system_filename        = NULL;
	static char *function                      = "libvmdk_handle_open_snapshot_chain_wide";
	size_t system_filename_size                = 0;
	int number_of_snapshots                    = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( libvmdk_system_string_size_from_wide_string(
	     filename,
	     wide_string_length(
	      filename ) + 1,
	     &system_filename_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine filename size.",
		 function );

		goto on_error;
	}
	system_filename = system_string_allocate(
	                   system_filename_size );

	if( system_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_system_string_copy_from_wide_string(
	     system_filename,
	     system_filename_size,
	     filename,
	     wide_string_length(
	      filename ) + 1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_discover_snapshot_chain(
	     system_filename,
	     access_flags,
	     &snapshots_array,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to discover snapshot chain.",
		 function );

		goto on_error;
	}
	if( libcdata_array_get_number_of_entries(
	     snapshots_array,
	     &number_of_snapshots,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of snapshots.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_open_snapshot_chain(
	     handle,
	     snapshots_array,
	     number_of_snapshots - 1,
	     access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open snapshot chain.",
		 function );

		goto on_error;
	}
	internal_handle->snapshots_array         = snapshots_array;
	internal_handle->selected_snapshot_index = number_of_snapshots - 1;
	snapshots_array                          = NULL;

	memory_free(
	 system_filename );

	return( 1 );

on_error:
	if( snapshots_array != NULL )
	{
		libcdata_array_free(
		 &snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );
	}
	if( system_filename != NULL )
	{
		memory_free(
		 system_filename );
	}
	return( -1 );
}

#endif /* defined( HAVE_WIDE_CHARACTER_TYPE ) */

/* Opens a VMDK snapshot chain at a specific snapshot index
 * Snapshot 0 is the base snapshot
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_snapshot_chain_at_index(
     libvmdk_handle_t *handle,
     const char *filename,
     int snapshot_index,
     int access_flags,
     libcerror_error_t **error )
{
	libcdata_array_t *snapshots_array          = NULL;
	libvmdk_internal_handle_t *internal_handle = NULL;
	system_character_t *system_filename        = NULL;
	static char *function                      = "libvmdk_handle_open_snapshot_chain_at_index";
	size_t system_filename_size                = 0;
	int number_of_snapshots                    = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( libvmdk_system_string_size_from_narrow_string(
	     filename,
	     narrow_string_length(
	      filename ) + 1,
	     &system_filename_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine filename size.",
		 function );

		goto on_error;
	}
	system_filename = system_string_allocate(
	                   system_filename_size );

	if( system_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_system_string_copy_from_narrow_string(
	     system_filename,
	     system_filename_size,
	     filename,
	     narrow_string_length(
	      filename ) + 1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_discover_snapshot_chain(
	     system_filename,
	     access_flags,
	     &snapshots_array,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to discover snapshot chain.",
		 function );

		goto on_error;
	}
	if( libcdata_array_get_number_of_entries(
	     snapshots_array,
	     &number_of_snapshots,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of snapshots.",
		 function );

		goto on_error;
	}
	if( ( snapshot_index < 0 )
	 || ( snapshot_index >= number_of_snapshots ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: snapshot index value out of bounds.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_open_snapshot_chain(
	     handle,
	     snapshots_array,
	     snapshot_index,
	     access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open snapshot chain at index: %d.",
		 function,
		 snapshot_index );

		goto on_error;
	}
	internal_handle->snapshots_array         = snapshots_array;
	internal_handle->selected_snapshot_index = snapshot_index;
	snapshots_array                          = NULL;

	memory_free(
	 system_filename );

	return( 1 );

on_error:
	if( snapshots_array != NULL )
	{
		libcdata_array_free(
		 &snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );
	}
	if( system_filename != NULL )
	{
		memory_free(
		 system_filename );
	}
	return( -1 );
}

#if defined( HAVE_WIDE_CHARACTER_TYPE )

/* Opens a VMDK snapshot chain at a specific snapshot index
 * Snapshot 0 is the base snapshot
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_snapshot_chain_at_index_wide(
     libvmdk_handle_t *handle,
     const wchar_t *filename,
     int snapshot_index,
     int access_flags,
     libcerror_error_t **error )
{
	libcdata_array_t *snapshots_array          = NULL;
	libvmdk_internal_handle_t *internal_handle = NULL;
	system_character_t *system_filename        = NULL;
	static char *function                      = "libvmdk_handle_open_snapshot_chain_at_index_wide";
	size_t system_filename_size                = 0;
	int number_of_snapshots                    = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( libvmdk_system_string_size_from_wide_string(
	     filename,
	     wide_string_length(
	      filename ) + 1,
	     &system_filename_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine filename size.",
		 function );

		goto on_error;
	}
	system_filename = system_string_allocate(
	                   system_filename_size );

	if( system_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_system_string_copy_from_wide_string(
	     system_filename,
	     system_filename_size,
	     filename,
	     wide_string_length(
	      filename ) + 1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_discover_snapshot_chain(
	     system_filename,
	     access_flags,
	     &snapshots_array,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to discover snapshot chain.",
		 function );

		goto on_error;
	}
	if( libcdata_array_get_number_of_entries(
	     snapshots_array,
	     &number_of_snapshots,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of snapshots.",
		 function );

		goto on_error;
	}
	if( ( snapshot_index < 0 )
	 || ( snapshot_index >= number_of_snapshots ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: snapshot index value out of bounds.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_open_snapshot_chain(
	     handle,
	     snapshots_array,
	     snapshot_index,
	     access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open snapshot chain at index: %d.",
		 function,
		 snapshot_index );

		goto on_error;
	}
	internal_handle->snapshots_array         = snapshots_array;
	internal_handle->selected_snapshot_index = snapshot_index;
	snapshots_array                          = NULL;

	memory_free(
	 system_filename );

	return( 1 );

on_error:
	if( snapshots_array != NULL )
	{
		libcdata_array_free(
		 &snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );
	}
	if( system_filename != NULL )
	{
		memory_free(
		 system_filename );
	}
	return( -1 );
}

#endif /* defined( HAVE_WIDE_CHARACTER_TYPE ) */

/* Opens a VMDK snapshot chain at a specific timestamp
 * The most recent snapshot with a modification time at or before the timestamp is used
 * Returns 1 if successful, 0 if no snapshot was found or -1 on error
 */
int libvmdk_handle_open_snapshot_chain_at_timestamp(
     libvmdk_handle_t *handle,
     const char *filename,
     uint64_t timestamp,
     int access_flags,
     libcerror_error_t **error )
{
	libcdata_array_t *snapshots_array = NULL;
	libvmdk_snapshot_t *snapshot      = NULL;
	system_character_t *system_filename = NULL;
	static char *function             = "libvmdk_handle_open_snapshot_chain_at_timestamp";
	size_t system_filename_size       = 0;
	int number_of_snapshots           = 0;
	int selected_snapshot_index       = -1;
	int snapshot_index                = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	if( libvmdk_system_string_size_from_narrow_string(
	     filename,
	     narrow_string_length(
	      filename ) + 1,
	     &system_filename_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine filename size.",
		 function );

		goto on_error;
	}
	system_filename = system_string_allocate(
	                   system_filename_size );

	if( system_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_system_string_copy_from_narrow_string(
	     system_filename,
	     system_filename_size,
	     filename,
	     narrow_string_length(
	      filename ) + 1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_discover_snapshot_chain(
	     system_filename,
	     access_flags,
	     &snapshots_array,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to discover snapshot chain.",
		 function );

		goto on_error;
	}
	if( libcdata_array_get_number_of_entries(
	     snapshots_array,
	     &number_of_snapshots,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of snapshots.",
		 function );

		goto on_error;
	}
	for( snapshot_index = 0;
	     snapshot_index < number_of_snapshots;
	     snapshot_index++ )
	{
		if( libcdata_array_get_entry_by_index(
		     snapshots_array,
		     snapshot_index,
		     (intptr_t **) &snapshot,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot: %d.",
			 function,
			 snapshot_index );

			goto on_error;
		}
		if( ( snapshot->modification_time != 0 )
		 && ( snapshot->modification_time <= timestamp ) )
		{
			selected_snapshot_index = snapshot_index;
		}
	}
	if( selected_snapshot_index < 0 )
	{
		memory_free(
		 system_filename );

		libcdata_array_free(
		 &snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );

		return( 0 );
	}
	if( libvmdk_internal_handle_open_snapshot_chain(
	     handle,
	     snapshots_array,
	     selected_snapshot_index,
	     access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open snapshot chain at timestamp: %" PRIu64 ".",
		 function,
		 timestamp );

		goto on_error;
	}
	( (libvmdk_internal_handle_t *) handle )->snapshots_array         = snapshots_array;
	( (libvmdk_internal_handle_t *) handle )->selected_snapshot_index = selected_snapshot_index;
	snapshots_array                                                   = NULL;

	memory_free(
	 system_filename );

	return( 1 );

on_error:
	if( snapshots_array != NULL )
	{
		libcdata_array_free(
		 &snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );
	}
	if( system_filename != NULL )
	{
		memory_free(
		 system_filename );
	}
	return( -1 );
}

#if defined( HAVE_WIDE_CHARACTER_TYPE )

/* Opens a VMDK snapshot chain at a specific timestamp
 * The most recent snapshot with a modification time at or before the timestamp is used
 * Returns 1 if successful, 0 if no snapshot was found or -1 on error
 */
int libvmdk_handle_open_snapshot_chain_at_timestamp_wide(
     libvmdk_handle_t *handle,
     const wchar_t *filename,
     uint64_t timestamp,
     int access_flags,
     libcerror_error_t **error )
{
	libcdata_array_t *snapshots_array  = NULL;
	libvmdk_snapshot_t *snapshot       = NULL;
	system_character_t *system_filename = NULL;
	static char *function              = "libvmdk_handle_open_snapshot_chain_at_timestamp_wide";
	size_t system_filename_size        = 0;
	int number_of_snapshots            = 0;
	int selected_snapshot_index        = -1;
	int snapshot_index                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	if( libvmdk_system_string_size_from_wide_string(
	     filename,
	     wide_string_length(
	      filename ) + 1,
	     &system_filename_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to determine filename size.",
		 function );

		goto on_error;
	}
	system_filename = system_string_allocate(
	                   system_filename_size );

	if( system_filename == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_system_string_copy_from_wide_string(
	     system_filename,
	     system_filename_size,
	     filename,
	     wide_string_length(
	      filename ) + 1,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_CONVERSION,
		 LIBCERROR_CONVERSION_ERROR_GENERIC,
		 "%s: unable to copy filename.",
		 function );

		goto on_error;
	}
	if( libvmdk_internal_handle_discover_snapshot_chain(
	     system_filename,
	     access_flags,
	     &snapshots_array,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to discover snapshot chain.",
		 function );

		goto on_error;
	}
	if( libcdata_array_get_number_of_entries(
	     snapshots_array,
	     &number_of_snapshots,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of snapshots.",
		 function );

		goto on_error;
	}
	for( snapshot_index = 0;
	     snapshot_index < number_of_snapshots;
	     snapshot_index++ )
	{
		if( libcdata_array_get_entry_by_index(
		     snapshots_array,
		     snapshot_index,
		     (intptr_t **) &snapshot,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve snapshot: %d.",
			 function,
			 snapshot_index );

			goto on_error;
		}
		if( ( snapshot->modification_time != 0 )
		 && ( snapshot->modification_time <= timestamp ) )
		{
			selected_snapshot_index = snapshot_index;
		}
	}
	if( selected_snapshot_index < 0 )
	{
		memory_free(
		 system_filename );

		libcdata_array_free(
		 &snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );

		return( 0 );
	}
	if( libvmdk_internal_handle_open_snapshot_chain(
	     handle,
	     snapshots_array,
	     selected_snapshot_index,
	     access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open snapshot chain at timestamp: %" PRIu64 ".",
		 function,
		 timestamp );

		goto on_error;
	}
	( (libvmdk_internal_handle_t *) handle )->snapshots_array         = snapshots_array;
	( (libvmdk_internal_handle_t *) handle )->selected_snapshot_index = selected_snapshot_index;
	snapshots_array                                                   = NULL;

	memory_free(
	 system_filename );

	return( 1 );

on_error:
	if( snapshots_array != NULL )
	{
		libcdata_array_free(
		 &snapshots_array,
		 (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		 NULL );
	}
	if( system_filename != NULL )
	{
		memory_free(
		 system_filename );
	}
	return( -1 );
}

#endif /* defined( HAVE_WIDE_CHARACTER_TYPE ) */

/* Opens the extent data files
 * If the extent data filenames were not set explicitly this function assumes the extent data files
 * are in the same location as the descriptor file
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_extent_data_files(
     libvmdk_handle_t *handle,
     libcerror_error_t **error )
{
	libbfio_pool_t *file_io_pool                  = NULL;
	libvmdk_extent_values_t *extent_values        = NULL;
	libvmdk_internal_handle_t *internal_handle    = NULL;
	system_character_t *extent_data_file_location = NULL;
	system_character_t *extent_data_file_path     = NULL;
	static char *function                         = "libvmdk_handle_open_extent_data_files";
	size_t extent_data_file_location_size         = 0;
	int extent_index                              = 0;
	int number_of_extents                         = 0;
	int result                                    = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( ( ( internal_handle->access_flags & LIBVMDK_ACCESS_FLAG_READ ) == 0 )
	 && ( ( internal_handle->access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) == 0 ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported access flags.",
		 function );

		return( -1 );
	}
	if( ( internal_handle->access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) != 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: write access currently not supported.",
		 function );

		return( -1 );
	}
	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
	if( internal_handle->extent_data_file_io_pool != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid handle - extent data file IO pool already exists.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( libcdata_array_get_number_of_entries(
	     internal_handle->extent_values_array,
	     &number_of_extents,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of extents.",
		 function );

		goto on_error;
	}
	if( number_of_extents == 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid number of extents value out of bounds.",
		 function );

		return( -1 );
	}
	if( libbfio_pool_initialize(
	     &file_io_pool,
	     number_of_extents,
	     internal_handle->maximum_number_of_open_handles,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create file IO pool.",
		 function );

		goto on_error;
	}
	for( extent_index = 0;
	     extent_index < number_of_extents;
	     extent_index++ )
	{
		if( libcdata_array_get_entry_by_index(
		     internal_handle->extent_values_array,
		     extent_index,
		     (intptr_t **) &extent_values,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve extent values: %d from array.",
			 function,
			 extent_index );

			goto on_error;
		}
		if( extent_values == NULL )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: missing extent values: %d.",
			 function,
			 extent_index );

			goto on_error;
		}
		if( extent_values->type != LIBVMDK_EXTENT_TYPE_ZERO )
		{
#if defined( HAVE_WIDE_SYSTEM_CHARACTER )
			result = libvmdk_extent_table_get_extent_data_file_path_wide(
			          internal_handle->extent_table,
			          extent_values,
			          &extent_data_file_location,
			          &extent_data_file_location_size,
			          error );
#else
			result = libvmdk_extent_table_get_extent_data_file_path(
			          internal_handle->extent_table,
			          extent_values,
			          &extent_data_file_location,
			          &extent_data_file_location_size,
			          error );
#endif
			if( result != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
				 "%s: unable to create extent data file location.",
				 function );

				goto on_error;
			}
			extent_data_file_path = extent_data_file_location;

#if defined( HAVE_WIDE_SYSTEM_CHARACTER )
			result = libcfile_file_exists_wide(
			          extent_data_file_path,
			          error );
#else
			result = libcfile_file_exists(
			          extent_data_file_path,
			          error );
#endif
			if( result == -1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
				 "%s: unable to determine if extent: %d data file exists.",
				 function,
				 extent_index );

				goto on_error;
			}
			else if( ( result == 0 )
			      && ( extent_values->alternate_filename != NULL ) )
			{
				memory_free(
				 extent_data_file_location );

				extent_data_file_location      = NULL;
				extent_data_file_location_size = 0;

				extent_data_file_path = extent_values->alternate_filename;
			}
			/* Note that the open extent data file function will initialize extent_data_file_io_pool
			 */
#if defined( HAVE_WIDE_SYSTEM_CHARACTER )
			result = libvmdk_handle_open_extent_data_file_wide(
				  internal_handle,
				  file_io_pool,
				  extent_index,
				  extent_data_file_path,
				  error );
#else
			result = libvmdk_handle_open_extent_data_file(
				  internal_handle,
				  file_io_pool,
				  extent_index,
				  extent_data_file_path,
				  error );
#endif
			if( result != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_IO,
				 LIBCERROR_IO_ERROR_OPEN_FAILED,
				 "%s: unable to open extent data file: %" PRIs_SYSTEM ".",
				 function,
				 extent_data_file_path );

				goto on_error;
			}
			if( extent_data_file_location != NULL )
			{
				memory_free(
				 extent_data_file_location );

				extent_data_file_location = NULL;
			}
		}
	}
	if( libvmdk_internal_handle_open_read_extent_data_files(
	     internal_handle,
	     file_io_pool,
	     error ) != 1 )
	{
                libcerror_error_set(
                 error,
                 LIBCERROR_ERROR_DOMAIN_RUNTIME,
                 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
                 "%s: unable to read extent data files.",
                 function );

                goto on_error;
	}
	internal_handle->extent_data_file_io_pool                    = file_io_pool;
	internal_handle->extent_data_file_io_pool_created_in_library = 1;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( 1 );

on_error:
	if( file_io_pool != NULL )
	{
		libbfio_pool_close_all(
		 file_io_pool,
		 NULL );
		libbfio_pool_free(
		 &file_io_pool,
		 NULL );
	}
	if( extent_data_file_location != NULL )
	{
		memory_free(
		 extent_data_file_location );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	libcthreads_read_write_lock_release_for_write(
	 internal_handle->read_write_lock,
	 NULL );
#endif
	return( -1 );
}

/* Opens the extent data files using a Basic File IO (bfio) pool
 * This function assumes the extent data files are in same order as defined by the descriptor file
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_extent_data_files_file_io_pool(
     libvmdk_handle_t *handle,
     libbfio_pool_t *file_io_pool,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_open_extent_data_files_file_io_pool";
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( ( ( internal_handle->access_flags & LIBVMDK_ACCESS_FLAG_READ ) == 0 )
	 && ( ( internal_handle->access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) == 0 ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported access flags.",
		 function );

		return( -1 );
	}
	if( ( internal_handle->access_flags & LIBVMDK_ACCESS_FLAG_WRITE ) != 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: write access currently not supported.",
		 function );

		return( -1 );
	}
	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
	if( internal_handle->extent_data_file_io_pool != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid handle - extent data file IO pool already exists.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( libvmdk_internal_handle_open_read_extent_data_files(
	     internal_handle,
	     file_io_pool,
	     error ) != 1 )
	{
                libcerror_error_set(
                 error,
                 LIBCERROR_ERROR_DOMAIN_RUNTIME,
                 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
                 "%s: unable to read extent data files.",
                 function );

                result = -1;
	}
	else
	{
		internal_handle->extent_data_file_io_pool = file_io_pool;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Opens a specific extent data file
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_extent_data_file(
     libvmdk_internal_handle_t *internal_handle,
     libbfio_pool_t *file_io_pool,
     int extent_index,
     const char *filename,
     libcerror_error_t **error )
{
	libbfio_handle_t *file_io_handle = NULL;
	static char *function            = "libvmdk_handle_open_extent_data_file";
	size_t filename_length           = 0;
	int bfio_access_flags            = 0;

	if( internal_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	if( ( internal_handle->access_flags & LIBVMDK_ACCESS_FLAG_READ ) != 0 )
	{
		bfio_access_flags = LIBBFIO_ACCESS_FLAG_READ;
	}
	if( libbfio_file_initialize(
	     &file_io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create file IO handle.",
		 function );

		goto on_error;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libbfio_handle_set_track_offsets_read(
	     file_io_handle,
	     1,
	     error ) != 1 )
	{
                libcerror_error_set(
                 error,
                 LIBCERROR_ERROR_DOMAIN_RUNTIME,
                 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
                 "%s: unable to set track offsets read in file IO handle.",
                 function );

                goto on_error;
	}
#endif
	filename_length = narrow_string_length(
	                   filename );

	if( libbfio_file_set_name(
	     file_io_handle,
	     filename,
	     filename_length + 1,
	     error ) != 1 )
	{
                libcerror_error_set(
                 error,
                 LIBCERROR_ERROR_DOMAIN_RUNTIME,
                 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
                 "%s: unable to set filename in file IO handle.",
                 function );

                goto on_error;
	}
	if( libbfio_handle_open(
	     file_io_handle,
	     bfio_access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open file IO handle.",
		 function );

		goto on_error;
	}
	if( libbfio_pool_set_handle(
	     file_io_pool,
	     extent_index,
	     file_io_handle,
	     bfio_access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set file IO handle: %d in pool.",
		 function,
		 extent_index );

		goto on_error;
	}
	return( 1 );

on_error:
	if( file_io_handle != NULL )
	{
		libbfio_handle_free(
		 &file_io_handle,
		 NULL );
	}
	return( -1 );
}

#if defined( HAVE_WIDE_CHARACTER_TYPE )

/* Opens a specific extent data file
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_open_extent_data_file_wide(
     libvmdk_internal_handle_t *internal_handle,
     libbfio_pool_t *file_io_pool,
     int extent_index,
     const wchar_t *filename,
     libcerror_error_t **error )
{
	libbfio_handle_t *file_io_handle = NULL;
	static char *function            = "libvmdk_handle_open_extent_data_file_wide";
	size_t filename_length           = 0;
	int bfio_access_flags            = 0;

	if( internal_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	if( ( internal_handle->access_flags & LIBVMDK_ACCESS_FLAG_READ ) != 0 )
	{
		bfio_access_flags = LIBBFIO_ACCESS_FLAG_READ;
	}
	if( libbfio_file_initialize(
	     &file_io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create file IO handle.",
		 function );

                goto on_error;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libbfio_handle_set_track_offsets_read(
	     file_io_handle,
	     1,
	     error ) != 1 )
	{
                libcerror_error_set(
                 error,
                 LIBCERROR_ERROR_DOMAIN_RUNTIME,
                 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
                 "%s: unable to set track offsets read in file IO handle.",
                 function );

                goto on_error;
	}
#endif
	filename_length = wide_string_length(
	                   filename );

	if( libbfio_file_set_name_wide(
	     file_io_handle,
	     filename,
	     filename_length + 1,
	     error ) != 1 )
	{
                libcerror_error_set(
                 error,
                 LIBCERROR_ERROR_DOMAIN_RUNTIME,
                 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
                 "%s: unable to set filename in file IO handle.",
                 function );

                goto on_error;
	}
	if( libbfio_handle_open(
	     file_io_handle,
	     bfio_access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_OPEN_FAILED,
		 "%s: unable to open file IO handle.",
		 function );

		goto on_error;
	}
	if( libbfio_pool_set_handle(
	     file_io_pool,
	     extent_index,
	     file_io_handle,
	     bfio_access_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set file IO handle: %d in pool.",
		 function,
		 extent_index );

		goto on_error;
	}
	return( 1 );

on_error:
	if( file_io_handle != NULL )
	{
		libbfio_handle_free(
		 &file_io_handle,
		 NULL );
	}
	return( -1 );
}

#endif /* defined( HAVE_WIDE_CHARACTER_TYPE ) */

/* Closes the VMDK file(s)
 * Returns 0 if successful or -1 on error
 */
int libvmdk_handle_close(
     libvmdk_handle_t *handle,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_close";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid file - missing descriptor file.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( internal_handle->extent_data_file_io_pool_created_in_library != 0 )
	{
		if( libbfio_pool_close_all(
		     internal_handle->extent_data_file_io_pool,
		     error ) != 0 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_CLOSE_FAILED,
			 "%s: unable to close all files in extent data file IO pool.",
			 function );

			result = -1;
		}
		if( libbfio_pool_free(
		     &( internal_handle->extent_data_file_io_pool ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free extent data file IO pool.",
			 function );

			result = -1;
		}
		internal_handle->extent_data_file_io_pool_created_in_library = 0;
	}
	internal_handle->extent_data_file_io_pool = NULL;
	internal_handle->current_offset           = 0;

	if( libvmdk_io_handle_clear(
	     internal_handle->io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
		 "%s: unable to clear IO handle.",
		 function );

		result = -1;
	}
	if( internal_handle->descriptor_file != NULL )
	{
		if( libvmdk_descriptor_file_free(
		     &( internal_handle->descriptor_file ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free descriptor file.",
			 function );

			result = -1;
		}
	}
	if( internal_handle->grain_table != NULL )
	{
		if( libvmdk_grain_table_free(
		     &( internal_handle->grain_table ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free grain table.",
			 function );

			result = -1;
		}
	}
	if( internal_handle->grains_cache != NULL )
	{
		if( libfcache_cache_free(
		     &( internal_handle->grains_cache ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free grains cache.",
			 function );

			result = -1;
		}
	}
	if( libvmdk_extent_table_clear(
	     internal_handle->extent_table,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
		 "%s: unable to clear extent table.",
		 function );

		result = -1;
	}
	if( internal_handle->snapshots_array != NULL )
	{
		if( libcdata_array_free(
		     &( internal_handle->snapshots_array ),
		     (int (*)(intptr_t **, libcerror_error_t **)) &libvmdk_snapshot_free,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free snapshots array.",
			 function );

			result = -1;
		}
	}
	internal_handle->selected_snapshot_index = 0;

	if( ( internal_handle->parent_handle != NULL )
	 && ( internal_handle->parent_handle_created_in_library != 0 ) )
	{
		if( libvmdk_handle_free(
		     &( internal_handle->parent_handle ),
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free parent handle.",
			 function );

			result = -1;
		}
		internal_handle->parent_handle_created_in_library = 0;
	}
	internal_handle->parent_handle = NULL;
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Opens a handle for reading
 * Returns 1 if successful or -1 on error
 */
int libvmdk_internal_handle_open_read(
     libvmdk_internal_handle_t *internal_handle,
     libbfio_handle_t *file_io_handle,
     libcerror_error_t **error )
{
	libvmdk_extent_file_t *extent_file = NULL;
	uint8_t *descriptor_data           = NULL;
	static char *function              = "libvmdk_internal_handle_open_read";
	uint8_t file_type                  = 0;
	int result                         = 0;

	if( internal_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->io_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing IO handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->descriptor_file != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid handle - descriptor file already set.",
		 function );

		return( -1 );
	}
	result = libvmdk_internal_handle_open_read_signature(
		  file_io_handle,
		  &file_type,
		  error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_READ_FAILED,
		 "%s: unable to read signature.",
		 function );

		goto on_error;
	}
	else if( result == 0 )
	{
		file_type = LIBVMDK_FILE_TYPE_RAW_DATA;
	}
	switch( file_type )
	{
		case LIBVMDK_FILE_TYPE_DESCRIPTOR_FILE:
			if( libvmdk_descriptor_file_initialize(
			     &( internal_handle->descriptor_file ),
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
				 "%s: unable to create descriptor file.",
				 function );

				goto on_error;
			}
			if( libvmdk_descriptor_file_read_file_io_handle(
			     internal_handle->descriptor_file,
			     file_io_handle,
			     internal_handle->extent_values_array,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_IO,
				 LIBCERROR_IO_ERROR_READ_FAILED,
				 "%s: unable to read descriptor file.",
				 function );

				goto on_error;
			}
			break;

		case LIBVMDK_FILE_TYPE_VMDK_SPARSE_DATA:
			if( libvmdk_extent_file_initialize(
			     &extent_file,
			     internal_handle->io_handle,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
				 "%s: unable to create extent file.",
				 function );

				goto on_error;
			}
			if( libvmdk_extent_file_read_file_header_file_io_handle(
			     extent_file,
			     file_io_handle,
			     0,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_IO,
				 LIBCERROR_IO_ERROR_READ_FAILED,
				 "%s: unable to read extent file header.",
				 function );

				goto on_error;
			}
			if( extent_file->descriptor_size > 0 )
			{
				if( libvmdk_descriptor_file_initialize(
				     &( internal_handle->descriptor_file ),
				     error ) != 1 )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_RUNTIME,
					 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
					 "%s: unable to create descriptor file.",
					 function );

					goto on_error;
				}
				if( extent_file->descriptor_size > (size64_t) MEMORY_MAXIMUM_ALLOCATION_SIZE )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_RUNTIME,
					 LIBCERROR_RUNTIME_ERROR_VALUE_EXCEEDS_MAXIMUM,
					 "%s: invalid extent file - descriptor size value exceeds maximum allocation size.",
					 function );

					goto on_error;
				}
				descriptor_data = (uint8_t *) memory_allocate(
							       sizeof( uint8_t ) * (size_t) extent_file->descriptor_size );

				if( descriptor_data == NULL )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_MEMORY,
					 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
					 "%s: unable to create descriptor data.",
					 function );

					goto on_error;
				}
				if( libvmdk_extent_file_read_descriptor_data_file_io_handle(
				     extent_file,
				     file_io_handle,
				     descriptor_data,
				     (size_t) extent_file->descriptor_size,
				     error ) != 1 )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_IO,
					 LIBCERROR_IO_ERROR_READ_FAILED,
					 "%s: unable to read descriptor data.",
					 function );

					goto on_error;
				}
				if( libvmdk_descriptor_file_read_string(
				     internal_handle->descriptor_file,
				     (char *) descriptor_data,
				     (size_t) extent_file->descriptor_size,
				     internal_handle->extent_values_array,
				     error ) != 1 )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_IO,
					 LIBCERROR_IO_ERROR_READ_FAILED,
					 "%s: unable to read descriptor from string.",
					 function );

					goto on_error;
				}
				memory_free(
				 descriptor_data );

				descriptor_data = NULL;
			}
			if( libvmdk_extent_file_free(
			     &extent_file,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
				 "%s: unable to free extent file.",
				 function );

				goto on_error;
			}
			break;

		case LIBVMDK_FILE_TYPE_RAW_DATA:
		case LIBVMDK_FILE_TYPE_COWD_SPARSE_DATA:
		default:
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
			 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
			 "%s: unsupported file type.",
			 function );

			goto on_error;
	}
	if( libvmdk_descriptor_file_get_disk_type(
	     internal_handle->descriptor_file,
	     &( internal_handle->disk_type ),
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve disk type.",
		 function );

		goto on_error;
	}
	if( libvmdk_descriptor_file_get_media_size(
	     internal_handle->descriptor_file,
	     &( internal_handle->io_handle->media_size ),
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve media size.",
		 function );

		goto on_error;
	}
	return( 1 );

on_error:
	if( descriptor_data != NULL )
	{
		memory_free(
		 descriptor_data );
	}
	if( extent_file != NULL )
	{
		libvmdk_extent_file_free(
		 &extent_file,
		 NULL );
	}
	if( internal_handle->descriptor_file != NULL )
	{
		libvmdk_descriptor_file_free(
		 &( internal_handle->descriptor_file ),
		 NULL );
	}
	return( -1 );
}

/* Reads the extent data files
 * Returns 1 if successful or -1 on error
 */
int libvmdk_internal_handle_open_read_extent_data_files(
     libvmdk_internal_handle_t *internal_handle,
     libbfio_pool_t *file_io_pool,
     libcerror_error_t **error )
{
	libvmdk_extent_file_t *extent_file     = NULL;
	libvmdk_extent_values_t *extent_values = NULL;
	static char *function                  = "libvmdk_internal_handle_open_read_extent_data_files";
	size64_t extent_file_size              = 0;
	int extent_index                       = 0;
	int number_of_extents                  = 0;
	int number_of_file_io_handles          = 0;

	if( internal_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->io_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing IO handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
	if( internal_handle->grain_table != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid handle - grain table already set.",
		 function );

		return( -1 );
	}
	if( internal_handle->grains_cache != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid handle - grains cache already set.",
		 function );

		return( -1 );
	}
	if( libcdata_array_get_number_of_entries(
	     internal_handle->extent_values_array,
	     &number_of_extents,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of extents.",
		 function );

		goto on_error;
	}
	if( number_of_extents == 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: missing extents.",
		 function );

		goto on_error;
	}
	if( libbfio_pool_get_number_of_handles(
	     file_io_pool,
	     &number_of_file_io_handles,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve the number of file IO handles.",
		 function );

		return( -1 );
	}
	if( number_of_extents != number_of_file_io_handles )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: mismatch between number of file IO handles in pool and number of extents in metadata.",
		 function );

		goto on_error;
	}
	if( libvmdk_extent_table_initialize_extents(
	     internal_handle->extent_table,
	     number_of_extents,
	     internal_handle->disk_type,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to initialize extent table extents.",
		 function );

		goto on_error;
	}
	if( libvmdk_grain_table_initialize(
	     &( internal_handle->grain_table ),
	     internal_handle->io_handle,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create grain table.",
		 function );

		goto on_error;
	}
	if( libfcache_cache_initialize(
	     &( internal_handle->grains_cache ),
	     LIBVMDK_MAXIMUM_CACHE_ENTRIES_GRAINS,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create grains cache.",
		 function );

		goto on_error;
	}
	for( extent_index = 0;
	     extent_index < number_of_extents;
	     extent_index++ )
	{
		if( libcdata_array_get_entry_by_index(
		     internal_handle->extent_values_array,
		     extent_index,
		     (intptr_t **) &extent_values,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve extent values: %d from array.",
			 function,
			 extent_index );

			goto on_error;
		}
		if( libbfio_pool_get_size(
		     file_io_pool,
		     extent_index,
		     &extent_file_size,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve size of file IO pool entry: %d.",
			 function,
			 extent_index );

			goto on_error;
		}
		if( libvmdk_extent_table_set_extent_by_extent_values(
		     internal_handle->extent_table,
		     extent_values,
		     extent_index,
		     extent_index,
		     extent_file_size,
		     extent_values->offset,
		     extent_values->size,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set extent: %d in table.",
			 function,
			 extent_index );

			goto on_error;
		}
		if( ( extent_values->type == LIBVMDK_EXTENT_TYPE_SPARSE )
		 || ( extent_values->type == LIBVMDK_EXTENT_TYPE_VMFS_SPARSE ) )
		{
			if( libvmdk_extent_file_initialize(
			     &extent_file,
			     internal_handle->io_handle,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
				 "%s: unable to create extent file: %d.",
				 function,
				 extent_index );

				goto on_error;
			}
#if defined( HAVE_DEBUG_OUTPUT )
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "Reading extent file: %d file header:\n",
				 extent_index );
			}
#endif
			if( libvmdk_extent_file_read_file_header(
			     extent_file,
			     file_io_pool,
			     extent_index,
			     0,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_IO,
				 LIBCERROR_IO_ERROR_READ_FAILED,
				 "%s: unable to read extent file: %d header.",
				 function,
				 extent_index );

				goto on_error;
			}
			if( ( extent_file->file_type != LIBVMDK_FILE_TYPE_COWD_SPARSE_DATA )
			 && ( extent_file->file_type != LIBVMDK_FILE_TYPE_VMDK_SPARSE_DATA ) )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
				 "%s: extent file type not supported for extent type.",
				 function );

				goto on_error;
			}
			if( ( internal_handle->disk_type != LIBVMDK_DISK_TYPE_STREAM_OPTIMIZED )
			 && ( extent_file->file_type == LIBVMDK_FILE_TYPE_VMDK_SPARSE_DATA )
			 && ( ( extent_file->flags & LIBVMDK_FLAG_HAS_GRAIN_COMPRESSION ) != LIBVMDK_COMPRESSION_METHOD_NONE ) )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
				 "%s: grain compression not supported for disk type.",
				 function );

				goto on_error;
			}
			if( ( extent_file->file_type == LIBVMDK_FILE_TYPE_VMDK_SPARSE_DATA )
			 && ( extent_file->primary_grain_directory_offset == (off64_t) -1 )
			 && ( extent_file->compression_method == LIBVMDK_COMPRESSION_METHOD_DEFLATE ) )
			{
				if( libvmdk_extent_file_read_file_header(
				     extent_file,
				     file_io_pool,
				     extent_index,
				     extent_file_size - 1024,
				     error ) != 1 )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_IO,
					 LIBCERROR_IO_ERROR_READ_FAILED,
					 "%s: unable to read secondary extent file: %d header.",
					 function,
					 extent_index );

					goto on_error;
				}
			}
			if( extent_index == 0 )
			{
				internal_handle->io_handle->grain_size = extent_file->grain_size;
			}
			else if( extent_file->grain_size != internal_handle->io_handle->grain_size )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_INPUT,
				 LIBCERROR_INPUT_ERROR_VALUE_MISMATCH,
				 "%s: extent file: %d grain size mismatch.",
				 function,
				 extent_index );

				goto on_error;
			}
			if( extent_file->is_dirty != 0 )
			{
				internal_handle->io_handle->is_dirty = 1;
			}
#if defined( HAVE_DEBUG_OUTPUT )
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "Reading extent file: %d grain directories:\n",
				 extent_index );
			}
#endif
			if( libvmdk_extent_file_read_grain_directories(
			     extent_file,
			     file_io_pool,
			     extent_index,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_IO,
				 LIBCERROR_IO_ERROR_READ_FAILED,
				 "%s: unable to read extent file: %d grain directories.",
				 function,
				 extent_index );

				goto on_error;
			}
			if( libvmdk_extent_table_set_extent_storage_media_size_by_index(
			     internal_handle->extent_table,
			     extent_index,
			     extent_file->storage_media_size,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_APPEND_FAILED,
				 "%s: unable to set mapped range of element: %d in extent table.",
				 function,
				 extent_index );

				goto on_error;
			}
			if( libvmdk_extent_file_free(
			     &extent_file,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
				 "%s: unable to free extent file.",
				 function );

				goto on_error;
			}
		}
		else if( ( extent_values->type != LIBVMDK_EXTENT_TYPE_FLAT )
		      && ( extent_values->type != LIBVMDK_EXTENT_TYPE_VMFS_FLAT ) )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
			 "%s: unsupported extent type.",
			 function );

			goto on_error;
		}
		if( internal_handle->io_handle->abort == 1 )
		{
			goto on_error;
		}
	}
	return( 1 );

on_error:
	if( extent_file != NULL )
	{
		libvmdk_extent_file_free(
		 &extent_file,
		 NULL );
	}
	if( internal_handle->grains_cache != NULL )
	{
		libfcache_cache_free(
		 &( internal_handle->grains_cache ),
		 NULL );
	}
	if( internal_handle->grain_table != NULL )
	{
		libvmdk_grain_table_free(
		 &( internal_handle->grain_table ),
		 NULL );
	}
	return( -1 );
}

/* Reads the file signature and tries to determine the file type
 * Returns 1 if successful, 0 if no file type could be determined or -1 on error
 */
int libvmdk_internal_handle_open_read_signature(
     libbfio_handle_t *file_io_handle,
     uint8_t *file_type,
     libcerror_error_t **error )
{
	libcsplit_narrow_split_string_t *lines = NULL;
	uint8_t *signature                     = NULL;
	static char *function                  = "libvmdk_internal_handle_open_read_signature";
	ssize_t read_count                     = 0;
	int line_index                         = 0;
	int number_of_lines                    = 0;
	int result                             = 0;

	if( file_type == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file type.",
		 function );

		return( -1 );
	}
	signature = (uint8_t *) memory_allocate(
	                         sizeof( uint8_t ) * 2048 );

	if( signature == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create signature.",
		 function );

		goto on_error;
	}
	if( memory_set(
	     signature,
	     0,
	     sizeof( uint8_t ) * 2048 ) == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear signature.",
		 function );

		goto on_error;
	}
	read_count = libbfio_handle_read_buffer_at_offset(
	              file_io_handle,
	              signature,
	              32,
	              0,
	              error );

	if( read_count != (ssize_t) 32 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_READ_FAILED,
		 "%s: unable to read signature at offset: 0 (0x00000000).",
		 function );

		goto on_error;
	}
	if( memory_compare(
	     signature,
	     cowd_sparse_file_signature,
	     4 ) == 0 )
	{
		*file_type = LIBVMDK_FILE_TYPE_COWD_SPARSE_DATA;
		result     = 1;
	}
	else if( memory_compare(
	          signature,
	          vmdk_sparse_file_signature,
	          4 ) == 0 )
	{
		*file_type = LIBVMDK_FILE_TYPE_VMDK_SPARSE_DATA;
		result     = 1;
	}
	else if( signature[ 0 ] == '#' )
	{
		read_count = libbfio_handle_read_buffer(
		              file_io_handle,
		              &( signature[ 32 ] ),
		              2048 - 32,
		              error );

		if( read_count == -1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_READ_FAILED,
			 "%s: unable to read signature.",
			 function );

			goto on_error;
		}
		if( libcsplit_narrow_string_split(
		     (char *) signature,
		     32 + read_count,
		     '\n',
		     &lines,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
			 "%s: unable to split file data into lines.",
			 function );

			goto on_error;
		}
		if( libcsplit_narrow_split_string_get_number_of_segments(
		     lines,
		     &number_of_lines,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to retrieve number of lines.",
			 function );

			goto on_error;
		}
		result = libvmdk_descriptor_file_read_signature(
			  lines,
			  number_of_lines,
			  &line_index,
		          error );

		if( result == -1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_READ_FAILED,
			 "%s: unable to read descriptor file.",
			 function );

			goto on_error;
		}
		else if( result != 0 )
		{
			*file_type = LIBVMDK_FILE_TYPE_DESCRIPTOR_FILE;
		}
		if( libcsplit_narrow_split_string_free(
		     &lines,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free lines.",
			 function );

			goto on_error;
		}
	}
	memory_free(
	 signature );

	return( result );

on_error:
	if( lines != NULL )
	{
		libcsplit_narrow_split_string_free(
		 &lines,
		 NULL );
	}
	if( signature != NULL )
	{
		memory_free(
		 signature );
	}
	return( -1 );
}

/* Reads (media) data at the current offset into a buffer using a Basic File IO (bfio) pool
 * This function is not multi-thread safe acquire write lock before call
 * Returns the number of bytes read or -1 on error
 */
ssize_t libvmdk_internal_handle_read_buffer_from_file_io_pool(
         libvmdk_internal_handle_t *internal_handle,
         libbfio_pool_t *file_io_pool,
         void *buffer,
         size_t buffer_size,
         libcerror_error_t **error )
{
	libvmdk_grain_data_t *grain_data = NULL;
	static char *function            = "libvmdk_internal_handle_read_buffer_from_file_io_pool";
	size_t buffer_offset             = 0;
	size_t read_size                 = 0;
	ssize_t read_count               = 0;
	off64_t grain_data_offset        = 0;
	uint64_t grain_index             = 0;
	int grain_is_sparse              = 0;

	if( internal_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->io_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing IO handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->current_offset < 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid handle - invalid IO handle - current offset value out of bounds.",
		 function );

		return( -1 );
	}
	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
	if( ( internal_handle->descriptor_file->parent_content_identifier_set != 0 )
	 && ( internal_handle->descriptor_file->parent_content_identifier != 0xffffffffUL ) )
	{
		if( internal_handle->parent_handle == NULL )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: invalid handle - missing parent handle.",
			 function );

			return( -1 );
		}
	}
	if( buffer == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid buffer.",
		 function );

		return( -1 );
	}
	if( buffer_size > (size_t) SSIZE_MAX )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid element data size value exceeds maximum.",
		 function );

		return( -1 );
	}
	internal_handle->io_handle->abort = 0;

	if( buffer_size == 0 )
	{
		return( 0 );
	}
	if( (size64_t) internal_handle->current_offset >= internal_handle->io_handle->media_size )
	{
		return( 0 );
	}
	if( internal_handle->extent_table->extent_files_stream != NULL )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: requested offset\t\t\t\t: %" PRIi64 " (0x%08" PRIx64 ")\n",
			 function,
			 internal_handle->current_offset,
			 internal_handle->current_offset );
		}
#endif
		read_size = buffer_size;

		if( ( (size64_t) read_size > internal_handle->io_handle->media_size )
		 || ( (size64_t) internal_handle->current_offset > ( internal_handle->io_handle->media_size - read_size ) ) )
		{
			read_size = (size_t) ( internal_handle->io_handle->media_size - internal_handle->current_offset );
		}
		read_count = libfdata_stream_read_buffer_at_offset(
		              internal_handle->extent_table->extent_files_stream,
			      (intptr_t *) file_io_pool,
			      (uint8_t *) buffer,
			      read_size,
			      internal_handle->current_offset,
			      0,
			      error );

		if( read_count != (ssize_t) read_size )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_IO,
			 LIBCERROR_IO_ERROR_READ_FAILED,
			 "%s: unable to read buffer from extent files stream at offset: %" PRIi64 " (0x%" PRIx64 ").",
			 function,
			 internal_handle->current_offset,
			 internal_handle->current_offset );

			return( -1 );
		}
		internal_handle->current_offset += (off64_t) read_size;

		buffer_offset = read_size;
	}
	else
	{
		grain_index       = internal_handle->current_offset / internal_handle->io_handle->grain_size;
		grain_data_offset = (off64_t) ( internal_handle->current_offset % internal_handle->io_handle->grain_size );

		while( buffer_offset < buffer_size )
		{
			grain_is_sparse = libvmdk_grain_table_grain_is_sparse_at_offset(
					   internal_handle->grain_table,
					   grain_index,
					   file_io_pool,
					   internal_handle->extent_table,
					   internal_handle->current_offset,
					   error );

			if( grain_is_sparse == -1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
				 "%s: unable to determine if the grain: %" PRIu64 " is sparse.",
				 function,
				 grain_index );

				return( -1 );
			}
			read_size = (size_t) ( internal_handle->io_handle->grain_size - grain_data_offset );

			if( read_size > ( buffer_size - buffer_offset ) )
			{
				read_size = buffer_size - buffer_offset;
			}
			if( ( (size64_t) read_size > internal_handle->io_handle->media_size )
			 || ( (size64_t) internal_handle->current_offset > ( internal_handle->io_handle->media_size - read_size ) ) )
			{
				read_size = (size_t) ( internal_handle->io_handle->media_size - internal_handle->current_offset );
			}
			if( grain_is_sparse != 0 )
			{
				if( internal_handle->parent_handle == NULL )
				{
					if( memory_set(
					     &( ( (uint8_t *) buffer )[ buffer_offset ] ),
					     0,
					     read_size ) == NULL )
					{
						libcerror_error_set(
						 error,
						 LIBCERROR_ERROR_DOMAIN_MEMORY,
						 LIBCERROR_MEMORY_ERROR_SET_FAILED,
						 "%s: unable to fill buffer with sparse grain.",
						 function );

						return( -1 );
					}
				}
				else
				{
/* TODO do we need grain offset or current offset ? */
					if( libvmdk_handle_seek_offset(
					     internal_handle->parent_handle,
					     internal_handle->current_offset,
					     SEEK_SET,
					     error ) == -1 )
					{
						libcerror_error_set(
						 error,
						 LIBCERROR_ERROR_DOMAIN_IO,
						 LIBCERROR_IO_ERROR_SEEK_FAILED,
						 "%s: unable to seek grain offset: %" PRIi64 " (0x%08" PRIx64 ") in parent.",
						 function,
						 internal_handle->current_offset,
						 internal_handle->current_offset );

						return( -1 );
					}
					read_count = libvmdk_handle_read_buffer(
						      internal_handle->parent_handle,
						      &( ( (uint8_t *) buffer )[ buffer_offset ] ),
						      read_size,
						      error );

					if( read_count != (ssize_t) read_size )
					{
						libcerror_error_set(
						 error,
						 LIBCERROR_ERROR_DOMAIN_IO,
						 LIBCERROR_IO_ERROR_READ_FAILED,
						 "%s: unable to read grain data from parent.",
						 function );

						return( -1 );
					}
				}
			}
			else
			{
				if( libvmdk_grain_table_get_grain_data_at_offset(
				     internal_handle->grain_table,
				     grain_index,
				     file_io_pool,
				     internal_handle->extent_table,
				     internal_handle->grains_cache,
				     internal_handle->current_offset,
				     &grain_data,
				     &grain_data_offset,
				     error ) != 1 )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_RUNTIME,
					 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
					 "%s: unable to retrieve grain: %d data.",
					 function,
					 grain_index );

					return( -1 );
				}
				if( grain_data == NULL )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_RUNTIME,
					 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
					 "%s: missing grain: %d data.",
					 function,
					 grain_index );

					return( -1 );
				}
				if( (size64_t) grain_data_offset > grain_data->data_size )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_RUNTIME,
					 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
					 "%s: grain data offset value out of bounds.",
					 function );

					return( -1 );
				}
				if( read_size > (size_t) ( grain_data->data_size - grain_data_offset ) )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_RUNTIME,
					 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
					 "%s: read size value out of bounds.",
					 function );

					return( -1 );
				}
				if( memory_copy(
				     &( ( (uint8_t *) buffer )[ buffer_offset ] ),
				     &( ( grain_data->data )[ grain_data_offset ] ),
				     read_size ) == NULL )
				{
					libcerror_error_set(
					 error,
					 LIBCERROR_ERROR_DOMAIN_MEMORY,
					 LIBCERROR_MEMORY_ERROR_COPY_FAILED,
					 "%s: unable to copy grain data to buffer.",
					 function );

					return( -1 );
				}
			}
			buffer_offset    += read_size;
			grain_index      += 1;
			grain_data_offset = 0;

			internal_handle->current_offset += (off64_t) read_size;

			if( (size64_t) internal_handle->current_offset >= internal_handle->io_handle->media_size )
			{
				break;
			}
			if( internal_handle->io_handle->abort != 0 )
			{
				break;
			}
		}
	}
	return( (ssize_t) buffer_offset );
}

/* Reads (media) data at the current offset into a buffer
 * Returns the number of bytes read or -1 on error
 */
ssize_t libvmdk_handle_read_buffer(
         libvmdk_handle_t *handle,
         void *buffer,
         size_t buffer_size,
         libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_read_buffer";
	ssize_t read_count                         = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->extent_data_file_io_pool == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing extent data file IO pool.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	read_count = libvmdk_internal_handle_read_buffer_from_file_io_pool(
		      internal_handle,
		      internal_handle->extent_data_file_io_pool,
		      buffer,
		      buffer_size,
		      error );

	if( read_count < 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_READ_FAILED,
		 "%s: unable to read buffer.",
		 function );

		read_count = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( read_count );
}

/* Reads (media) data at a specific offset
 * Returns the number of bytes read or -1 on error
 */
ssize_t libvmdk_handle_read_buffer_at_offset(
         libvmdk_handle_t *handle,
         void *buffer,
         size_t buffer_size,
         off64_t offset,
         libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_read_buffer_at_offset";
	ssize_t read_count                         = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->extent_data_file_io_pool == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing extent data file IO pool.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( libvmdk_internal_handle_seek_offset(
	     internal_handle,
	     offset,
	     SEEK_SET,
	     error ) == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_SEEK_FAILED,
		 "%s: unable to seek offset.",
		 function );

		goto on_error;
	}
	read_count = libvmdk_internal_handle_read_buffer_from_file_io_pool(
		      internal_handle,
		      internal_handle->extent_data_file_io_pool,
		      buffer,
		      buffer_size,
		      error );

	if( read_count == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_READ_FAILED,
		 "%s: unable to read buffer.",
		 function );

		goto on_error;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( read_count );

on_error:
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	libcthreads_read_write_lock_release_for_write(
	 internal_handle->read_write_lock,
	 NULL );
#endif
	return( -1 );
}

/* Seeks a certain offset of the (media) data
 * This function is not multi-thread safe acquire write lock before call
 * Returns the offset if seek is successful or -1 on error
 */
off64_t libvmdk_internal_handle_seek_offset(
         libvmdk_internal_handle_t *internal_handle,
         off64_t offset,
         int whence,
         libcerror_error_t **error )
{
	static char *function = "libvmdk_internal_handle_seek_offset";

	if( internal_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->io_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing IO handle.",
		 function );

		return( -1 );
	}
	if( ( whence != SEEK_CUR )
	 && ( whence != SEEK_END )
	 && ( whence != SEEK_SET ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported whence.",
		 function );

		return( -1 );
	}
	if( whence == SEEK_CUR )
	{
		offset += internal_handle->current_offset;
	}
	else if( whence == SEEK_END )
	{
		offset += (off64_t) internal_handle->io_handle->media_size;
	}
	if( offset < 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid offset value out of bounds.",
		 function );

		return( -1 );
	}
	internal_handle->current_offset = offset;

	return( offset );
}

/* Seeks a certain offset of the (media) data
 * Returns the offset if seek is successful or -1 on error
 */
off64_t libvmdk_handle_seek_offset(
         libvmdk_handle_t *handle,
         off64_t offset,
         int whence,
         libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_seek_offset";

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->extent_data_file_io_pool == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing extent data file IO pool.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	offset = libvmdk_internal_handle_seek_offset(
	          internal_handle,
	          offset,
	          whence,
	          error );

	if( offset == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_SEEK_FAILED,
		 "%s: unable to seek offset.",
		 function );

		offset = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( offset );
}

/* Retrieves the current offset of the (media) data
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_get_offset(
     libvmdk_handle_t *handle,
     off64_t *offset,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_offset";

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->io_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing IO handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->extent_data_file_io_pool == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing extent data file IO pool.",
		 function );

		return( -1 );
	}
	if( offset == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid offset.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	*offset = internal_handle->current_offset;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( 1 );
}

/* Sets the maximum number of (concurrent) open file handles
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_set_maximum_number_of_open_handles(
     libvmdk_handle_t *handle,
     int maximum_number_of_open_handles,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_set_maximum_number_of_open_handles";
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( internal_handle->extent_data_file_io_pool != NULL )
	{
		if( libbfio_pool_set_maximum_number_of_open_handles(
		     internal_handle->extent_data_file_io_pool,
		     maximum_number_of_open_handles,
		     error ) != -1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set maximum number of open handles in extent data file IO pool.",
			 function );

			result = -1;
		}
	}
	if( result == 1 )
	{
		internal_handle->maximum_number_of_open_handles = maximum_number_of_open_handles;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Sets the parent handle
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_set_parent_handle(
     libvmdk_handle_t *handle,
     libvmdk_handle_t *parent_handle,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_set_parent_handle";
	uint32_t content_identifier                = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( libvmdk_handle_get_content_identifier(
	     parent_handle,
	     &content_identifier,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve content identifier from parent handle.",
		 function );

		goto on_error;
	}
	if( content_identifier != internal_handle->descriptor_file->parent_content_identifier )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: mismatch in content identifier.",
		 function );

		goto on_error;
	}
	internal_handle->parent_handle = parent_handle;
	internal_handle->parent_handle_created_in_library = 0;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( 1 );

on_error:
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	libcthreads_read_write_lock_release_for_write(
	 internal_handle->read_write_lock,
	 NULL );
#endif
	return( -1 );
}

/* Sets the path to the extent data files
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_set_extent_data_files_path(
     libvmdk_handle_t *handle,
     const char *path,
     size_t path_length,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_set_extent_data_files_path";
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( path == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		return( -1 );
	}
	if( path_length > (size_t) ( SSIZE_MAX - 1 ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid path length value exceeds maximum.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( libvmdk_extent_table_set_data_files_path(
	     internal_handle->extent_table,
	     path,
	     path_length,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set data files path in extent table.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

#if defined( HAVE_WIDE_CHARACTER_TYPE )

/* Sets the path to the extent data files
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_set_extent_data_files_path_wide(
     libvmdk_handle_t *handle,
     const wchar_t *path,
     size_t path_length,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_set_extent_data_files_path_wide";
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( path == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		return( -1 );
	}
	if( path_length > (size_t) ( SSIZE_MAX - 1 ) )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid path length value exceeds maximum.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	if( libvmdk_extent_table_set_data_files_path_wide(
	     internal_handle->extent_table,
	     path,
	     path_length,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set data files path in extent table.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_write(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for writing.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

#endif /* defined( HAVE_WIDE_CHARACTER_TYPE ) */

/* Retrieves the disk type
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_get_disk_type(
     libvmdk_handle_t *handle,
     int *disk_type,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_disk_type";

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
	if( disk_type == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid disk type.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	*disk_type = internal_handle->disk_type;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( 1 );
}

/* Retrieves the media size
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_get_media_size(
     libvmdk_handle_t *handle,
     size64_t *media_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_media_size";

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
	if( media_size == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid media size.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	*media_size = internal_handle->descriptor_file->media_size;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( 1 );
}

/* Retrieves the content identifier
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_get_content_identifier(
     libvmdk_handle_t *handle,
     uint32_t *content_identifier,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_content_identifier";

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
	if( content_identifier == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid content identifier.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	*content_identifier = internal_handle->descriptor_file->content_identifier;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( 1 );
}

/* Retrieves the parent content identifier
 * Returns 1 if successful, 0 if not available or -1 on error
 */
int libvmdk_handle_get_parent_content_identifier(
     libvmdk_handle_t *handle,
     uint32_t *parent_content_identifier,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_parent_content_identifier";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_descriptor_file_get_parent_content_identifier(
	          internal_handle->descriptor_file,
	          parent_content_identifier,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve parent content identifier.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the size of the UTF-8 encoded parent filename
 * The returned size includes the end of string character
 * Returns 1 if successful, 0 if not available or -1 on error
 */
int libvmdk_handle_get_utf8_parent_filename_size(
     libvmdk_handle_t *handle,
     size_t *utf8_string_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_utf8_parent_filename_size";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_descriptor_file_get_utf8_parent_filename_size(
	          internal_handle->descriptor_file,
	          utf8_string_size,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve UTF-8 parent filename size.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the UTF-8 encoded parent filename
 * The size should include the end of string character
 * Returns 1 if successful, 0 if not available or -1 on error
 */
int libvmdk_handle_get_utf8_parent_filename(
     libvmdk_handle_t *handle,
     uint8_t *utf8_string,
     size_t utf8_string_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_utf8_parent_filename";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_descriptor_file_get_utf8_parent_filename(
	          internal_handle->descriptor_file,
	          utf8_string,
	          utf8_string_size,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve UTF-8 parent filename.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the size of the UTF-16 encoded parent filename
 * The returned size includes the end of string character
 * Returns 1 if successful, 0 if not available or -1 on error
 */
int libvmdk_handle_get_utf16_parent_filename_size(
     libvmdk_handle_t *handle,
     size_t *utf16_string_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_utf16_parent_filename_size";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_descriptor_file_get_utf16_parent_filename_size(
	          internal_handle->descriptor_file,
	          utf16_string_size,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve UTF-16 parent filename size.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the UTF-16 encoded parent filename
 * The size should include the end of string character
 * Returns 1 if successful, 0 if not available or -1 on error
 */
int libvmdk_handle_get_utf16_parent_filename(
     libvmdk_handle_t *handle,
     uint16_t *utf16_string,
     size_t utf16_string_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_utf16_parent_filename";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_descriptor_file_get_utf16_parent_filename(
	          internal_handle->descriptor_file,
	          utf16_string,
	          utf16_string_size,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve UTF-16 parent filename.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the number of extents
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_get_number_of_extents(
     libvmdk_handle_t *handle,
     int *number_of_extents,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_number_of_extents";
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	if( libcdata_array_get_number_of_entries(
	     internal_handle->extent_values_array,
	     number_of_extents,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of extents.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves a specific extent descriptor
 * Returns 1 if successful or -1 on error
 */
int libvmdk_handle_get_extent_descriptor(
     libvmdk_handle_t *handle,
     int extent_index,
     libvmdk_extent_descriptor_t **extent_descriptor,
     libcerror_error_t **error )
{
	libvmdk_extent_values_t *extent_values     = NULL;
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_extent_descriptor";
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->descriptor_file == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing descriptor file.",
		 function );

		return( -1 );
	}
	if( extent_descriptor == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid extent descriptor.",
		 function );

		return( -1 );
	}
	if( *extent_descriptor != NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid extent descriptor value already set.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	if( libcdata_array_get_entry_by_index(
	     internal_handle->extent_values_array,
	     extent_index,
	     (intptr_t **) &extent_values,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve extent: %d from array.",
		 function,
		 extent_index );

		result = -1;
	}
	else if( libvmdk_extent_descriptor_initialize(
	          extent_descriptor,
	          extent_values,
	          error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create extent descriptor.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves a snapshot by index
 * Returns 1 if successful, 0 if no snapshot chain is available or -1 on error
 */
static int libvmdk_internal_handle_get_snapshot_by_index(
     libvmdk_internal_handle_t *internal_handle,
     int snapshot_index,
     libvmdk_snapshot_t **snapshot,
     libcerror_error_t **error )
{
	static char *function = "libvmdk_internal_handle_get_snapshot_by_index";

	if( internal_handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
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
	if( internal_handle->snapshots_array == NULL )
	{
		return( 0 );
	}
	if( snapshot_index < 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: snapshot index value out of bounds.",
		 function );

		return( -1 );
	}
	if( libcdata_array_get_entry_by_index(
	     internal_handle->snapshots_array,
	     snapshot_index,
	     (intptr_t **) snapshot,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot: %d.",
		 function,
		 snapshot_index );

		return( -1 );
	}
	return( 1 );
}

/* Retrieves the number of snapshots in the opened snapshot chain
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_number_of_snapshots(
     libvmdk_handle_t *handle,
     int *number_of_snapshots,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_number_of_snapshots";
	int result                                 = 1;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( number_of_snapshots == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid number of snapshots.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->snapshots_array == NULL )
	{
		return( 0 );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	if( libcdata_array_get_number_of_entries(
	     internal_handle->snapshots_array,
	     number_of_snapshots,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of snapshots.",
		 function );

		result = -1;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the selected snapshot index
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_selected_snapshot_index(
     libvmdk_handle_t *handle,
     int *snapshot_index,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	static char *function                      = "libvmdk_handle_get_selected_snapshot_index";

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( snapshot_index == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid snapshot index.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

	if( internal_handle->snapshots_array == NULL )
	{
		return( 0 );
	}
	*snapshot_index = internal_handle->selected_snapshot_index;

	return( 1 );
}

/* Retrieves the modification timestamp of a snapshot
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_snapshot_timestamp(
     libvmdk_handle_t *handle,
     int snapshot_index,
     uint64_t *timestamp,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_snapshot_timestamp";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( timestamp == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid timestamp.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		*timestamp = snapshot->modification_time;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the content identifier of a snapshot
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_snapshot_content_identifier(
     libvmdk_handle_t *handle,
     int snapshot_index,
     uint32_t *content_identifier,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_snapshot_content_identifier";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( content_identifier == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid content identifier.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		*content_identifier = snapshot->content_identifier;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the parent content identifier of a snapshot
 * Returns 1 if successful, 0 if not available or no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_snapshot_parent_content_identifier(
     libvmdk_handle_t *handle,
     int snapshot_index,
     uint32_t *parent_content_identifier,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_snapshot_parent_content_identifier";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( parent_content_identifier == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid parent content identifier.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		if( snapshot->parent_content_identifier_set == 0 )
		{
			result = 0;
		}
		else
		{
			*parent_content_identifier = snapshot->parent_content_identifier;
		}
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the disk type of a snapshot
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_snapshot_disk_type(
     libvmdk_handle_t *handle,
     int snapshot_index,
     int *disk_type,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_snapshot_disk_type";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( disk_type == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid disk type.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		*disk_type = snapshot->disk_type;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the media size of a snapshot
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_snapshot_media_size(
     libvmdk_handle_t *handle,
     int snapshot_index,
     size64_t *media_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_snapshot_media_size";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( media_size == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid media size.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		*media_size = snapshot->media_size;
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the size of the UTF-8 encoded filename of a snapshot
 * The returned size includes the end of string character
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_utf8_snapshot_filename_size(
     libvmdk_handle_t *handle,
     int snapshot_index,
     size_t *utf8_string_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_utf8_snapshot_filename_size";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
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
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		result = libvmdk_snapshot_get_utf8_filename_size(
		          snapshot,
		          utf8_string_size,
		          error );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the UTF-8 encoded filename of a snapshot
 * The size should include the end of string character
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_utf8_snapshot_filename(
     libvmdk_handle_t *handle,
     int snapshot_index,
     uint8_t *utf8_string,
     size_t utf8_string_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_utf8_snapshot_filename";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		result = libvmdk_snapshot_get_utf8_filename(
		          snapshot,
		          utf8_string,
		          utf8_string_size,
		          error );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the size of the UTF-16 encoded filename of a snapshot
 * The returned size includes the end of string character
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_utf16_snapshot_filename_size(
     libvmdk_handle_t *handle,
     int snapshot_index,
     size_t *utf16_string_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_utf16_snapshot_filename_size";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	if( utf16_string_size == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid UTF-16 string size.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		result = libvmdk_snapshot_get_utf16_filename_size(
		          snapshot,
		          utf16_string_size,
		          error );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

/* Retrieves the UTF-16 encoded filename of a snapshot
 * The size should include the end of string character
 * Returns 1 if successful, 0 if no snapshot chain was opened or -1 on error
 */
int libvmdk_handle_get_utf16_snapshot_filename(
     libvmdk_handle_t *handle,
     int snapshot_index,
     uint16_t *utf16_string,
     size_t utf16_string_size,
     libcerror_error_t **error )
{
	libvmdk_internal_handle_t *internal_handle = NULL;
	libvmdk_snapshot_t *snapshot               = NULL;
	static char *function                      = "libvmdk_handle_get_utf16_snapshot_filename";
	int result                                 = 0;

	if( handle == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libvmdk_internal_handle_t *) handle;

#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_grab_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to grab read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	result = libvmdk_internal_handle_get_snapshot_by_index(
	          internal_handle,
	          snapshot_index,
	          &snapshot,
	          error );

	if( result == -1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve snapshot.",
		 function );

		result = -1;
	}
	else if( result != 0 )
	{
		result = libvmdk_snapshot_get_utf16_filename(
		          snapshot,
		          utf16_string,
		          utf16_string_size,
		          error );
	}
#if defined( HAVE_LIBVMDK_MULTI_THREAD_SUPPORT )
	if( libcthreads_read_write_lock_release_for_read(
	     internal_handle->read_write_lock,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to release read/write lock for reading.",
		 function );

		return( -1 );
	}
#endif
	return( result );
}

