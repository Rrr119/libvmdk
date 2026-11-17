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

#if !defined( _LIBVMDK_SNAPSHOT_H )
#define _LIBVMDK_SNAPSHOT_H

#include <common.h>
#include <types.h>

#include "libvmdk_libcerror.h"

#if defined( __cplusplus )
extern "C" {
#endif

typedef struct libvmdk_snapshot libvmdk_snapshot_t;

struct libvmdk_snapshot
{
	/* The descriptor filename
	 */
	system_character_t *filename;

	/* The descriptor filename size
	 */
	size_t filename_size;

	/* The UTF-8 encoded descriptor filename
	 */
	uint8_t *utf8_filename;

	/* The UTF-8 encoded descriptor filename size
	 */
	size_t utf8_filename_size;

	/* The modification time
	 */
	uint64_t modification_time;

	/* The content identifier
	 */
	uint32_t content_identifier;

	/* The parent content identifier
	 */
	uint32_t parent_content_identifier;

	/* Value to indicate the parent content identifier has been set
	 */
	uint8_t parent_content_identifier_set;

	/* The disk type
	 */
	int disk_type;

	/* The media size
	 */
	size64_t media_size;
};

int libvmdk_snapshot_initialize(
     libvmdk_snapshot_t **snapshot,
     libcerror_error_t **error );

int libvmdk_snapshot_free(
     libvmdk_snapshot_t **snapshot,
     libcerror_error_t **error );

int libvmdk_snapshot_set_filename(
     libvmdk_snapshot_t *snapshot,
     const system_character_t *filename,
     size_t filename_size,
     libcerror_error_t **error );

int libvmdk_snapshot_get_utf8_filename_size(
     libvmdk_snapshot_t *snapshot,
     size_t *utf8_string_size,
     libcerror_error_t **error );

int libvmdk_snapshot_get_utf8_filename(
     libvmdk_snapshot_t *snapshot,
     uint8_t *utf8_string,
     size_t utf8_string_size,
     libcerror_error_t **error );

int libvmdk_snapshot_get_utf16_filename_size(
     libvmdk_snapshot_t *snapshot,
     size_t *utf16_string_size,
     libcerror_error_t **error );

int libvmdk_snapshot_get_utf16_filename(
     libvmdk_snapshot_t *snapshot,
     uint16_t *utf16_string,
     size_t utf16_string_size,
     libcerror_error_t **error );

#if defined( __cplusplus )
}
#endif

#endif /* !defined( _LIBVMDK_SNAPSHOT_H ) */
