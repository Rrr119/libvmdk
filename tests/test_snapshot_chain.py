#!/usr/bin/env python3
"""Tests VMDK snapshot chain reading through the public C API."""

import ctypes
import os
import tempfile
import unittest

from generate_snapshot_test_images import generate_images


def find_library():
    """Finds the built libvmdk shared library."""
    candidates = [
        os.environ.get("LIBVMDK_LIBRARY"),
        os.path.join("libvmdk", ".libs", "libvmdk.so"),
        os.path.join("libvmdk", ".libs", "libvmdk.dylib"),
    ]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    raise unittest.SkipTest("missing built libvmdk shared library")


class SnapshotChainTests(unittest.TestCase):
    """Tests snapshot chain discovery and time-based reads."""

    @classmethod
    def setUpClass(cls):
        cls.library = ctypes.CDLL(find_library())
        cls.handle_type = ctypes.c_void_p
        cls.error_type = ctypes.c_void_p
        cls.temp_directory = tempfile.TemporaryDirectory()
        cls.image_directory = cls.temp_directory.name
        generate_images(cls.image_directory)

        cls._set_signature("libvmdk_handle_initialize", [ctypes.POINTER(cls.handle_type), ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_free", [ctypes.POINTER(cls.handle_type), ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_open_snapshot_chain", [cls.handle_type, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_open_snapshot_chain_at_index", [cls.handle_type, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_open_snapshot_chain_at_timestamp", [cls.handle_type, ctypes.c_char_p, ctypes.c_uint64, ctypes.c_int, ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_get_number_of_snapshots", [cls.handle_type, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_get_selected_snapshot_index", [cls.handle_type, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_get_snapshot_timestamp", [cls.handle_type, ctypes.c_int, ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_get_snapshot_content_identifier", [cls.handle_type, ctypes.c_int, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_get_utf8_snapshot_filename_size", [cls.handle_type, ctypes.c_int, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_get_utf8_snapshot_filename", [cls.handle_type, ctypes.c_int, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(cls.error_type)], ctypes.c_int)
        cls._set_signature("libvmdk_handle_read_buffer_at_offset", [cls.handle_type, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int64, ctypes.POINTER(cls.error_type)], ctypes.c_ssize_t)
        cls._set_signature("libvmdk_error_backtrace_sprint", [cls.error_type, ctypes.c_char_p, ctypes.c_size_t], ctypes.c_int)
        cls._set_signature("libvmdk_error_free", [ctypes.POINTER(cls.error_type)], None)

    @classmethod
    def tearDownClass(cls):
        cls.temp_directory.cleanup()

    @classmethod
    def _set_signature(cls, name, argument_types, result_type):
        function = getattr(cls.library, name)
        function.argtypes = argument_types
        function.restype = result_type

    def _new_handle(self):
        handle = self.handle_type()
        error = self.error_type()
        self.assertEqual(
            self.library.libvmdk_handle_initialize(
                ctypes.byref(handle), ctypes.byref(error)
            ),
            1,
        )
        return handle, error

    def _raise_error(self, error):
        error_string = ctypes.create_string_buffer(8192)
        self.library.libvmdk_error_backtrace_sprint(
            error, error_string, len(error_string)
        )
        self.fail(error_string.value.decode("utf-8", errors="replace"))

    def _open_handle(self, timestamp=None, snapshot_index=None):
        handle, error = self._new_handle()
        filename = os.path.join(self.image_directory, "snapshot2.vmdk").encode()

        if timestamp is not None:
            result = self.library.libvmdk_handle_open_snapshot_chain_at_timestamp(
                handle, filename, timestamp, 1, ctypes.byref(error)
            )
        elif snapshot_index is not None:
            result = self.library.libvmdk_handle_open_snapshot_chain_at_index(
                handle, filename, snapshot_index, 1, ctypes.byref(error)
            )
        else:
            result = self.library.libvmdk_handle_open_snapshot_chain(
                handle, filename, 1, ctypes.byref(error)
            )
        if result != 1:
            self._raise_error(error)
        return handle

    def _free_handle(self, handle):
        error = self.error_type()
        self.assertEqual(
            self.library.libvmdk_handle_free(ctypes.byref(handle), ctypes.byref(error)),
            1,
        )

    def _read_grain(self, handle, grain_index):
        data = ctypes.create_string_buffer(8192)
        error = self.error_type()
        read_count = self.library.libvmdk_handle_read_buffer_at_offset(
            handle, data, len(data), grain_index * len(data), ctypes.byref(error)
        )
        if read_count != len(data):
            self._raise_error(error)
        return data.raw

    def test_latest_snapshot_chain(self):
        """Tests sparse and stream optimized images in the active chain."""
        handle = self._open_handle()
        try:
            number_of_snapshots = ctypes.c_int()
            selected_snapshot_index = ctypes.c_int()

            self.assertEqual(
                self.library.libvmdk_handle_get_number_of_snapshots(
                    handle,
                    ctypes.byref(number_of_snapshots),
                    None,
                ),
                1,
            )
            self.assertEqual(number_of_snapshots.value, 3)
            self.assertEqual(
                self.library.libvmdk_handle_get_selected_snapshot_index(
                    handle,
                    ctypes.byref(selected_snapshot_index),
                    None,
                ),
                1,
            )
            self.assertEqual(selected_snapshot_index.value, 2)
            self.assertEqual(self._read_grain(handle, 0), b"T" * 8192)
            self.assertEqual(self._read_grain(handle, 1), b"M" * 8192)
            self.assertEqual(self._read_grain(handle, 2), b"B" * 8192)
            self.assertEqual(self._read_grain(handle, 3), b"\x00" * 8192)
        finally:
            self._free_handle(handle)

    def test_snapshot_chain_metadata(self):
        """Tests descriptor and snapshot chain metadata."""
        handle = self._open_handle()
        try:
            expected_timestamps = [1000, 2000, 3000]
            expected_identifiers = [0x11111111, 0x22222222, 0x33333333]

            for snapshot_index in range(3):
                timestamp = ctypes.c_uint64()
                content_identifier = ctypes.c_uint32()
                filename_size = ctypes.c_size_t()

                self.assertEqual(
                    self.library.libvmdk_handle_get_snapshot_timestamp(
                        handle,
                        snapshot_index,
                        ctypes.byref(timestamp),
                        None,
                    ),
                    1,
                )
                self.assertEqual(timestamp.value, expected_timestamps[snapshot_index])
                self.assertEqual(
                    self.library.libvmdk_handle_get_snapshot_content_identifier(
                        handle,
                        snapshot_index,
                        ctypes.byref(content_identifier),
                        None,
                    ),
                    1,
                )
                self.assertEqual(
                    content_identifier.value, expected_identifiers[snapshot_index]
                )
                self.assertEqual(
                    self.library.libvmdk_handle_get_utf8_snapshot_filename_size(
                        handle,
                        snapshot_index,
                        ctypes.byref(filename_size),
                        None,
                    ),
                    1,
                )
                filename = ctypes.create_string_buffer(filename_size.value)
                self.assertEqual(
                    self.library.libvmdk_handle_get_utf8_snapshot_filename(
                        handle,
                        snapshot_index,
                        filename,
                        filename_size.value,
                        None,
                    ),
                    1,
                )
                self.assertTrue(filename.value.endswith(b".vmdk"))
        finally:
            self._free_handle(handle)

    def test_timestamp_selects_snapshot(self):
        """Tests selecting a historical snapshot by timestamp."""
        handle = self._open_handle(timestamp=2500)
        try:
            selected_snapshot_index = ctypes.c_int()
            self.assertEqual(
                self.library.libvmdk_handle_get_selected_snapshot_index(
                    handle,
                    ctypes.byref(selected_snapshot_index),
                    None,
                ),
                1,
            )
            self.assertEqual(selected_snapshot_index.value, 1)
            self.assertEqual(self._read_grain(handle, 0), b"B" * 8192)
            self.assertEqual(self._read_grain(handle, 1), b"M" * 8192)
            self.assertEqual(self._read_grain(handle, 2), b"B" * 8192)
        finally:
            self._free_handle(handle)

    def test_index_selects_base_snapshot(self):
        """Tests selecting the base snapshot by index."""
        handle = self._open_handle(snapshot_index=0)
        try:
            self.assertEqual(self._read_grain(handle, 0), b"B" * 8192)
            self.assertEqual(self._read_grain(handle, 1), b"\x00" * 8192)
            self.assertEqual(self._read_grain(handle, 2), b"B" * 8192)
        finally:
            self._free_handle(handle)

    def test_timestamp_before_chain(self):
        """Tests a timestamp older than the base snapshot."""
        handle, error = self._new_handle()
        filename = os.path.join(self.image_directory, "snapshot2.vmdk").encode()
        result = self.library.libvmdk_handle_open_snapshot_chain_at_timestamp(
            handle, filename, 500, 1, ctypes.byref(error)
        )
        self.assertEqual(result, 0)
        self._free_handle(handle)


if __name__ == "__main__":
    unittest.main()
