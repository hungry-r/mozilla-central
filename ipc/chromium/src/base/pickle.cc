// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"

#include <stdlib.h>

#include <limits>
#include <string>

//------------------------------------------------------------------------------

// static
const int Pickle::kPayloadUnit = 64;

// We mark a read only pickle with a special capacity_.
static const uint32 kCapacityReadOnly = (uint32) -1;

// Payload is uint32 aligned.

Pickle::Pickle()
    : header_(NULL),
      header_size_(sizeof(Header)),
      capacity_(0),
      variable_buffer_offset_(0) {
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle::Pickle(int header_size)
    : header_(NULL),
      header_size_(AlignInt(header_size, sizeof(uint32))),
      capacity_(0),
      variable_buffer_offset_(0) {
  DCHECK(static_cast<uint32>(header_size) >= sizeof(Header));
  DCHECK(header_size <= kPayloadUnit);
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle::Pickle(const char* data, int data_len)
    : header_(reinterpret_cast<Header*>(const_cast<char*>(data))),
      header_size_(data_len - header_->payload_size),
      capacity_(kCapacityReadOnly),
      variable_buffer_offset_(0) {
  DCHECK(header_size_ >= sizeof(Header));
  DCHECK(header_size_ == AlignInt(header_size_, sizeof(uint32)));
}

Pickle::Pickle(const Pickle& other)
    : header_(NULL),
      header_size_(other.header_size_),
      capacity_(0),
      variable_buffer_offset_(other.variable_buffer_offset_) {
  uint32 payload_size = header_size_ + other.header_->payload_size;
  bool resized = Resize(payload_size);
  CHECK(resized);  // Realloc failed.
  memcpy(header_, other.header_, payload_size);
}

Pickle::~Pickle() {
  if (capacity_ != kCapacityReadOnly)
    free(header_);
}

Pickle& Pickle::operator=(const Pickle& other) {
  if (header_size_ != other.header_size_ && capacity_ != kCapacityReadOnly) {
    free(header_);
    header_ = NULL;
    header_size_ = other.header_size_;
  }
  bool resized = Resize(other.header_size_ + other.header_->payload_size);
  CHECK(resized);  // Realloc failed.
  memcpy(header_, other.header_, header_size_ + other.header_->payload_size);
  variable_buffer_offset_ = other.variable_buffer_offset_;
  return *this;
}

bool Pickle::ReadBool(void** iter, bool* result) const {
  DCHECK(iter);

  int tmp;
  if (!ReadInt(iter, &tmp))
    return false;
  DCHECK(0 == tmp || 1 == tmp);
  *result = tmp ? true : false;
  return true;
}

bool Pickle::ReadInt16(void** iter, int16* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt16(void** iter, uint16* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadInt(void** iter, int* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  // TODO(jar) bug 1129285: Pickle should be cleaned up, and not dependent on
  // alignment.
  // Next line is otherwise the same as: memcpy(result, *iter, sizeof(*result));
  *result = *reinterpret_cast<int*>(*iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadLong(void** iter, long* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int64 bigResult = 0;
  if (!IteratorHasRoomFor(*iter, sizeof(bigResult)))
    return false;

  // TODO(jar) bug 1129285: Pickle should be cleaned up, and not dependent on
  // alignment.
  memcpy(&bigResult, *iter, sizeof(bigResult));
  DCHECK(bigResult <= LONG_MAX && bigResult >= LONG_MIN);
  *result = static_cast<long>(bigResult);

  UpdateIter(iter, sizeof(bigResult));
  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadULong(void** iter, unsigned long* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  uint64 bigResult = 0;
  if (!IteratorHasRoomFor(*iter, sizeof(bigResult)))
    return false;

  // TODO(jar) bug 1129285: Pickle should be cleaned up, and not dependent on
  // alignment.
  memcpy(&bigResult, *iter, sizeof(bigResult));
  DCHECK(bigResult <= ULONG_MAX);
  *result = static_cast<unsigned long>(bigResult);

  UpdateIter(iter, sizeof(bigResult));
  return true;
}

bool Pickle::ReadLength(void** iter, int* result) const {
  if (!ReadInt(iter, result))
    return false;
  return ((*result) >= 0);
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadSize(void** iter, size_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  uint64 bigResult = 0;
  if (!IteratorHasRoomFor(*iter, sizeof(bigResult)))
    return false;

  // TODO(jar) bug 1129285: Pickle should be cleaned up, and not dependent on
  // alignment.
  memcpy(&bigResult, *iter, sizeof(bigResult));
  DCHECK(bigResult <= std::numeric_limits<size_t>::max());
  *result = static_cast<size_t>(bigResult);

  UpdateIter(iter, sizeof(bigResult));
  return true;
}

bool Pickle::ReadInt32(void** iter, int32* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt32(void** iter, uint32* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadInt64(void** iter, int64* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt64(void** iter, uint64* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadDouble(void** iter, double* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadIntPtr(void** iter, intptr_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int64 bigResult = 0;
  if (!IteratorHasRoomFor(*iter, sizeof(bigResult)))
    return false;

  memcpy(&bigResult, *iter, sizeof(bigResult));
  DCHECK(bigResult <= std::numeric_limits<intptr_t>::max() && bigResult >= std::numeric_limits<intptr_t>::min());
  *result = static_cast<intptr_t>(bigResult);

  UpdateIter(iter, sizeof(bigResult));
  return true;
}

bool Pickle::ReadUnsignedChar(void** iter, unsigned char* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadString(void** iter, std::string* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int len;
  if (!ReadLength(iter, &len))
    return false;
  if (!IteratorHasRoomFor(*iter, len))
    return false;

  char* chars = reinterpret_cast<char*>(*iter);
  result->assign(chars, len);

  UpdateIter(iter, len);
  return true;
}

bool Pickle::ReadWString(void** iter, std::wstring* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int len;
  if (!ReadLength(iter, &len))
    return false;
  if (!IteratorHasRoomFor(*iter, len * sizeof(wchar_t)))
    return false;

  wchar_t* chars = reinterpret_cast<wchar_t*>(*iter);
  result->assign(chars, len);

  UpdateIter(iter, len * sizeof(wchar_t));
  return true;
}

bool Pickle::ReadString16(void** iter, string16* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int len;
  if (!ReadLength(iter, &len))
    return false;
  if (!IteratorHasRoomFor(*iter, len))
    return false;

  char16* chars = reinterpret_cast<char16*>(*iter);
  result->assign(chars, len);

  UpdateIter(iter, len * sizeof(char16));
  return true;
}

bool Pickle::ReadBytes(void** iter, const char** data, int length) const {
  DCHECK(iter);
  DCHECK(data);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, length))
    return false;

  *data = reinterpret_cast<const char*>(*iter);

  UpdateIter(iter, length);
  return true;
}

bool Pickle::ReadData(void** iter, const char** data, int* length) const {
  DCHECK(iter);
  DCHECK(data);
  DCHECK(length);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!ReadLength(iter, length))
    return false;

  return ReadBytes(iter, data, *length);
}

char* Pickle::BeginWrite(uint32 length) {
  // write at a uint32-aligned offset from the beginning of the header
  uint32 offset = AlignInt(header_->payload_size, sizeof(uint32));
  uint32 new_size = offset + AlignInt(length, sizeof(uint32));
  uint32 needed_size = header_size_ + new_size;

  if (needed_size > capacity_ && !Resize(std::max(capacity_ * 2, needed_size)))
    return NULL;

#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(length, std::numeric_limits<uint32>::max());
#endif

  header_->payload_size = static_cast<uint32>(new_size);
  return payload() + offset;
}

void Pickle::EndWrite(char* dest, int length) {
  // Zero-pad to keep tools like purify from complaining about uninitialized
  // memory.
  if (length % sizeof(uint32))
    memset(dest + length, 0, sizeof(uint32) - (length % sizeof(uint32)));
}

bool Pickle::WriteBytes(const void* data, int data_len) {
  DCHECK(capacity_ != kCapacityReadOnly) << "oops: pickle is readonly";

  char* dest = BeginWrite(data_len);
  if (!dest)
    return false;

  memcpy(dest, data, data_len);

  EndWrite(dest, data_len);
  return true;
}

bool Pickle::WriteString(const std::string& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(), static_cast<int>(value.size()));
}

bool Pickle::WriteWString(const std::wstring& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(),
                    static_cast<int>(value.size() * sizeof(wchar_t)));
}

bool Pickle::WriteString16(const string16& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(),
                    static_cast<int>(value.size()) * sizeof(char16));
}

bool Pickle::WriteData(const char* data, int length) {
  return WriteInt(length) && WriteBytes(data, length);
}

char* Pickle::BeginWriteData(int length) {
  DCHECK_EQ(variable_buffer_offset_, 0U) <<
    "There can only be one variable buffer in a Pickle";

  if (!WriteInt(length))
    return false;

  char *data_ptr = BeginWrite(length);
  if (!data_ptr)
    return NULL;

  variable_buffer_offset_ =
      data_ptr - reinterpret_cast<char*>(header_) - sizeof(int);

  // EndWrite doesn't necessarily have to be called after the write operation,
  // so we call it here to pad out what the caller will eventually write.
  EndWrite(data_ptr, length);
  return data_ptr;
}

void Pickle::TrimWriteData(int new_length) {
  DCHECK(variable_buffer_offset_ != 0);

  // Fetch the the variable buffer size
  int* cur_length = reinterpret_cast<int*>(
      reinterpret_cast<char*>(header_) + variable_buffer_offset_);

  if (new_length < 0 || new_length > *cur_length) {
    NOTREACHED() << "Invalid length in TrimWriteData.";
    return;
  }

  // Update the payload size and variable buffer size
  header_->payload_size -= (*cur_length - new_length);
  *cur_length = new_length;
}

bool Pickle::Resize(uint32 new_capacity) {
  new_capacity = AlignInt(new_capacity, kPayloadUnit);

  void* p = realloc(header_, new_capacity);
  if (!p)
    return false;

  header_ = reinterpret_cast<Header*>(p);
  capacity_ = new_capacity;
  return true;
}

// static
const char* Pickle::FindNext(uint32 header_size,
                             const char* start,
                             const char* end) {
  DCHECK(header_size == AlignInt(header_size, sizeof(uint32)));
  DCHECK(header_size <= static_cast<uint32>(kPayloadUnit));

  const Header* hdr = reinterpret_cast<const Header*>(start);
  const char* payload_base = start + header_size;
  const char* payload_end = payload_base + hdr->payload_size;
  if (payload_end < payload_base)
    return NULL;

  return (payload_end > end) ? NULL : payload_end;
}
