// ref: https://gist.github.com/irsl/ab7dcd17616e52c579f808f159115d4e

#ifndef ANANBOX_PARCEL_H
#define ANANBOX_PARCEL_H
#include <vector>

#include <optional>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t status_t;

// Parcel's object offset table is an array of these (binder_size_t in
// <linux/android/binder.h>). 64-bit targets are the only supported ones, so it
// is always 8 bytes; AnanboxFile.h exports sizeof() to the guest to verify.
typedef uint64_t binder_size_t;

class Parcel {
public:

    status_t            mError;
    uint8_t*            mData;
    size_t              mDataSize;
    size_t              mDataCapacity;
    mutable size_t mDataPos;
    binder_size_t*      mObjects;
    size_t              mObjectsSize;

};
#endif //ANANBOX_PARCEL_H
