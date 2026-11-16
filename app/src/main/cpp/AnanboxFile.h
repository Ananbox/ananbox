// Shared on-disk format between the ananbox app (host side) and the guest
// libbinder build (frameworks/native/libs/binder on the guest ROM).
//
// Keep this file byte-for-byte identical with the copy at
//   <guest tree>/frameworks/native/include/binder/AnanboxFile.h
// The header is plain C so it can be included from both C and C++ callers,
// and it contains no Android dependencies so it can be unit-tested on a host.
//
// File layout (all values host byte order; ananbox requires host and guest to
// run on the same architecture, so no endian conversion is needed):
//
//   struct ananbox_parcel_header
//   uint8_t  data[data_size]                 // first data_size bytes of Parcel::mData
//   uint8_t  objects[objects_count * objects_elem_size]
//                                            // first objects_count entries of Parcel::mObjects
//
// The objects array is the Parcel object *offset table*, not a flat array of
// flat_binder_object.  objects[i] is the byte offset (from the start of data)
// of the i-th flat_binder_object.  This mirrors binder/linux/android/binder.h.

#ifndef ANANBOX_FILE_H
#define ANANBOX_FILE_H

#include <stdint.h>

#define ANANBOX_PARCEL_MAGIC   0x414E4250u  /* "ANBP" */
#define ANANBOX_PARCEL_VERSION 1u

/* binder_object_index value meaning "there is no binder to replace". */
#define ANANBOX_NO_BINDER_INDEX 0xFFFFFFFFu

struct ananbox_parcel_header {
    uint32_t magic;               /* must be ANANBOX_PARCEL_MAGIC */
    uint32_t version;             /* must be ANANBOX_PARCEL_VERSION */
    int32_t  transact_code;       /* transaction code to use; <= 0 means unknown */
    uint32_t binder_object_index; /* index into the objects table to patch,
                                   * or ANANBOX_NO_BINDER_INDEX */
    uint32_t data_size;           /* bytes of parcel data that follow */
    uint32_t objects_count;       /* number of offset-table entries that follow */
    uint32_t objects_elem_size;   /* sizeof(binder_size_t) of the writer */
    uint32_t reserved;            /* must be zero; readers ignore */
} __attribute__((aligned(8)));

#endif /* ANANBOX_FILE_H */
