#!/usr/bin/env python3
"""Generates small VMDK snapshot chain test images."""

import argparse
import os
import struct
import zlib

SECTOR_SIZE = 512
MEDIA_SIZE = 64 * 1024
GRAIN_SIZE = 8 * 1024
GRAIN_SECTORS = GRAIN_SIZE // SECTOR_SIZE
MAX_SECTORS = MEDIA_SIZE // SECTOR_SIZE
NUMBER_OF_GRAIN_TABLE_ENTRIES = 512


def create_file_header(
    primary_grain_directory_sector,
    secondary_grain_directory_sector=0,
    descriptor_sector=0,
    descriptor_sectors=0,
    flags=1,
    compression_method=0,
):
    """Creates a VMDK sparse file header."""
    return struct.pack(
        "<4sIIQQQQIQQQB4sH433s",
        b"KDMV",
        1,
        flags,
        MAX_SECTORS,
        GRAIN_SECTORS,
        descriptor_sector,
        descriptor_sectors,
        NUMBER_OF_GRAIN_TABLE_ENTRIES,
        secondary_grain_directory_sector,
        primary_grain_directory_sector,
        0,
        0,
        b"\n \r\n",
        compression_method,
        b"\x00" * 433,
    )


def grain_data(byte_value):
    """Creates deterministic grain data."""
    return bytes([byte_value]) * GRAIN_SIZE


def create_sparse_extent(filename, grains):
    """Creates an uncompressed sparse extent with selected grains."""
    number_of_data_grains = len(grains)
    grain_table_sector = 1 + number_of_data_grains * GRAIN_SECTORS
    grain_directory_sector = grain_table_sector + 4
    file_size = (grain_directory_sector + 1) * SECTOR_SIZE

    data = bytearray(file_size)
    data[0:SECTOR_SIZE] = create_file_header(grain_directory_sector)

    grain_table = bytearray(NUMBER_OF_GRAIN_TABLE_ENTRIES * 4)

    for data_index, (grain_index, byte_value) in enumerate(sorted(grains.items())):
        data_sector = 1 + data_index * GRAIN_SECTORS
        grain_offset = data_sector * SECTOR_SIZE
        data[grain_offset : grain_offset + GRAIN_SIZE] = grain_data(byte_value)
        struct.pack_into("<I", grain_table, grain_index * 4, data_sector)

    grain_table_offset = grain_table_sector * SECTOR_SIZE
    data[grain_table_offset : grain_table_offset + len(grain_table)] = grain_table

    grain_directory = struct.pack("<I", grain_table_sector)
    grain_directory_offset = grain_directory_sector * SECTOR_SIZE
    data[grain_directory_offset : grain_directory_offset + len(grain_directory)] = (
        grain_directory
    )

    with open(filename, "wb") as file_object:
        file_object.write(data)


def create_descriptor(
    cid,
    parent_cid,
    extent_filename,
    create_type,
    parent_filename=None,
):
    """Creates a VMDK descriptor."""
    lines = [
        "# Disk DescriptorFile",
        "version=1",
        'encoding="UTF-8"',
        f"CID={cid:08x}",
        f"parentCID={parent_cid:08x}",
    ]
    if parent_filename:
        lines.append(f'parentFileNameHint="{parent_filename}"')
    lines.extend(
        [
            f'createType="{create_type}"',
            "",
            "# Extent description",
            f'RW {MAX_SECTORS} SPARSE "{extent_filename}"',
            "",
            "# The Disk Data Base",
            "#DDB",
            "",
            'ddb.virtualHWVersion = "4"',
            "",
        ]
    )
    return "\n".join(lines).encode("ascii")


def create_stream_optimized_extent(filename, descriptor_data, grains):
    """Creates a stream optimized compressed sparse extent with embedded descriptor."""
    descriptor_sectors = (len(descriptor_data) + SECTOR_SIZE - 1) // SECTOR_SIZE
    descriptor_sector = 1

    first_data_sector = descriptor_sector + descriptor_sectors
    data_sector = first_data_sector
    compressed_data = b""

    for grain_index, byte_value in sorted(grains.items()):
        compressed = zlib.compress(grain_data(byte_value), level=9)
        compressed_data = struct.pack("<QI", grain_index * GRAIN_SECTORS, len(compressed))
        compressed_data += compressed

    compressed_end = data_sector * SECTOR_SIZE + len(compressed_data)
    grain_table_marker_sector = (compressed_end + SECTOR_SIZE - 1) // SECTOR_SIZE
    grain_table_sector = grain_table_marker_sector + 1
    grain_table_offset = grain_table_sector * SECTOR_SIZE
    grain_directory_marker_sector = grain_table_sector + 4
    grain_directory_sector = grain_directory_marker_sector + 1
    grain_directory_offset = grain_directory_sector * SECTOR_SIZE
    footer_marker_sector = grain_directory_sector + 1
    footer_sector = footer_marker_sector + 1
    end_of_stream_sector = footer_sector + 1
    file_size = (end_of_stream_sector + 1) * SECTOR_SIZE

    data = bytearray(file_size)
    data[0:SECTOR_SIZE] = create_file_header(
        (1 << 64) - 1,
        flags=0x00030003,
        descriptor_sector=descriptor_sector,
        descriptor_sectors=descriptor_sectors,
        compression_method=1,
    )
    descriptor_offset = descriptor_sector * SECTOR_SIZE
    data[descriptor_offset : descriptor_offset + len(descriptor_data)] = descriptor_data

    data_offset = data_sector * SECTOR_SIZE
    data[data_offset : data_offset + len(compressed_data)] = compressed_data

    grain_table = bytearray(NUMBER_OF_GRAIN_TABLE_ENTRIES * 4)
    struct.pack_into("<I", grain_table, 0, data_sector)

    marker_offset = grain_table_marker_sector * SECTOR_SIZE
    data[marker_offset + 8 : marker_offset + 12] = struct.pack("<I", 1)
    data[grain_table_offset : grain_table_offset + len(grain_table)] = grain_table

    grain_directory = struct.pack("<I", grain_table_sector)
    marker_offset = grain_directory_marker_sector * SECTOR_SIZE
    data[marker_offset + 8 : marker_offset + 12] = struct.pack("<I", 2)
    data[grain_directory_offset : grain_directory_offset + len(grain_directory)] = (
        grain_directory
    )

    marker_offset = footer_marker_sector * SECTOR_SIZE
    data[marker_offset + 8 : marker_offset + 12] = struct.pack("<I", 3)
    footer_offset = footer_sector * SECTOR_SIZE
    data[footer_offset : footer_offset + SECTOR_SIZE] = create_file_header(
        grain_directory_sector,
        secondary_grain_directory_sector=grain_directory_sector,
        descriptor_sector=descriptor_sector,
        descriptor_sectors=descriptor_sectors,
        flags=0x00030003,
        compression_method=1,
    )
    marker_offset = end_of_stream_sector * SECTOR_SIZE
    data[marker_offset + 8 : marker_offset + 12] = struct.pack("<I", 0)

    with open(filename, "wb") as file_object:
        file_object.write(data)


def generate_images(output_directory):
    """Generates the snapshot chain test images."""
    os.makedirs(output_directory, exist_ok=True)

    base_cid = 0x11111111
    snapshot1_cid = 0x22222222
    snapshot2_cid = 0x33333333

    base_descriptor = create_descriptor(
        base_cid,
        0xFFFFFFFF,
        "base-s001.vmdk",
        "twoGbMaxExtentSparse",
    )
    with open(os.path.join(output_directory, "base.vmdk"), "wb") as file_object:
        file_object.write(base_descriptor)
    create_sparse_extent(
        os.path.join(output_directory, "base-s001.vmdk"),
        {0: ord("B"), 2: ord("B")},
    )

    snapshot1_descriptor = create_descriptor(
        snapshot1_cid,
        base_cid,
        "snapshot1-s001.vmdk",
        "twoGbMaxExtentSparse",
        parent_filename="base.vmdk",
    )
    with open(os.path.join(output_directory, "snapshot1.vmdk"), "wb") as file_object:
        file_object.write(snapshot1_descriptor)
    create_sparse_extent(
        os.path.join(output_directory, "snapshot1-s001.vmdk"),
        {1: ord("M")},
    )

    snapshot2_descriptor = create_descriptor(
        snapshot2_cid,
        snapshot1_cid,
        "snapshot2.vmdk",
        "streamOptimized",
        parent_filename="snapshot1.vmdk",
    )
    create_stream_optimized_extent(
        os.path.join(output_directory, "snapshot2.vmdk"),
        snapshot2_descriptor,
        {0: ord("T")},
    )

    timestamps = {
        "base.vmdk": 1000,
        "base-s001.vmdk": 1000,
        "snapshot1.vmdk": 2000,
        "snapshot1-s001.vmdk": 2000,
        "snapshot2.vmdk": 3000,
    }
    for filename, timestamp in timestamps.items():
        os.utime(os.path.join(output_directory, filename), (timestamp, timestamp))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "output_directory",
        nargs="?",
        default=os.path.join("tests", "data", "snapshot_chain"),
    )
    arguments = parser.parse_args()

    generate_images(arguments.output_directory)
