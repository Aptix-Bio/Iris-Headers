//
//  IrisCodec_Buffer.cpp
//  IrisCodec
//
//  Created by Ryan Landvater on 8/2/22.
//
#if IRIS_INTERNAL
#include "IrisCorePriv.hpp"
#else
#include "IrisCore.hpp"
#include "IrisBuffer.hpp"
    #if IRIS_DEBUG
    #include <assert.h>
    #endif
#endif
namespace Iris {
// MARK: - IRIS EXPOSED API
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
//  IRIS EXPOSED API                                        //
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
Buffer Create_strong_buffer ()
{
    return std::make_shared<Buffer_t>(REFERENCE_STRONG);
}
Buffer Create_strong_buffer (size_t bytes)
{
    return std::make_shared<Buffer_t>(REFERENCE_STRONG, bytes);
}
Buffer Copy_strong_buffer_from_data (const void* const data_ptr, size_t bytes)
{
    return std::make_shared<Buffer_t>(REFERENCE_STRONG, data_ptr, bytes);
}
Buffer  Wrap_weak_buffer_fom_data   (const void* const data_ref, size_t bytes)
{
    return std::make_shared<Buffer_t>(REFERENCE_WEAK, data_ref, bytes);
}
void*  Buffer_write_into_buffer (Buffer &__BUF, size_t& bytes)
{
    switch (__BUF->get_strength()) {
        case REFERENCE_WEAK:
            if (__BUF->size() < bytes) {
                throw std::runtime_error
                (std::string("Attempting to write " + 
                    std::to_string(bytes) + 
                    " bytes into a WEAK IrisCodec buffer with capacity "+
                    std::to_string(bytes) + 
                    " bytes.Either convert the buffer into a strong reference to allow expansion or wrap a larger allocation with sufficient space."
                ));
            }
            return __BUF->data();
        case REFERENCE_STRONG:
            return __BUF->append(bytes);
    } return nullptr;
}
Result Buffer_get_data (const Buffer &__BUF, void*& data, size_t& bytes)
{
    data    = __BUF->data();
    bytes   = __BUF->size();
    
    return Result::success();
}
// MARK: - IRIS INTERNALS
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
//  IRIS CODEC EXPOSED API                                  //
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
Buffer_t::Buffer_t (BufferReferenceStrength _ref) noexcept :
_strength   (_ref)
{
    // Only create a buffer with strong reference
    #if IRIS_DEBUG
    assert  (_ref == REFERENCE_STRONG);
    #endif
}
Buffer_t::Buffer_t (BufferReferenceStrength _ref, size_t __C) noexcept :
_strength   (_ref),
_capacity   (__C),
_data       (malloc(__C))
{
    // Only create a buffer with strong reference
    #if IRIS_DEBUG
    assert  (_ref == REFERENCE_STRONG);
    #endif
}
Buffer_t::Buffer_t (BufferReferenceStrength _ref, const void* const __D, size_t __S) noexcept :
_strength   (_ref),
_capacity   (__S),
_size       (__S),
_data       (nullptr)
{
    // Ensure a correct reference strength is assigned.
    #if IRIS_DEBUG
    assert  (static_cast<int>(_strength) < 2);
    #endif
    
    // Assign the correct
    switch (_strength) {
        case REFERENCE_STRONG:
            const_cast<void*&>(_data) = std::malloc(__S);
            std::memcpy(_data, __D, _size);
            break;
        case REFERENCE_WEAK:
            const_cast<void*&>(_data) = const_cast<void*&>(__D);
            break;
    }
}
Buffer_t::~Buffer_t()
{
    _capacity   = 0;
    _size       = 0;
    switch (_strength) {
        case REFERENCE_STRONG:
            if (_data) free(_data);
            return;
        case REFERENCE_WEAK:
            return;
    }
}
Buffer_t::operator void *const() const
{
    return _data;
}
Buffer_t::operator bool() const
{
    return _data;
}
BufferReferenceStrength Buffer_t::get_strength() const
{
    return _strength;
}
Result Buffer_t::change_strength(BufferReferenceStrength _assign)
{
    _strength = _assign;
    return Result::success();
}
void* Buffer_t::data() const
{
    return _data;
}
void* Buffer_t::end() const
{
    if (_size < _capacity)
        return static_cast<uint8_t*>(_data) + _size;
    return nullptr;
}
Result Buffer_t::prepare(size_t bytes)
{
    // If there are insufficient bytes, expand the buffer
    return change_capacity(_capacity + bytes);
}
void* Buffer_t::append(size_t __S)
{
    // COPY the old size of the image as we will change
    // the underlying data structure and need to check it.
    auto __OLD_SIZE = _size;
    
    // If there are insufficient bytes, expand the buffer
    if (available_bytes() < __S) {
        // A resize failure returns a null pointer
        if (change_capacity(_capacity + __S - available_bytes()).failed())
            return NULL;
        // Ensure we didn't make the buffer smaller.
        // resize can do that and that would be awkward...
        #if IRIS_DEBUG
        assert  (_capacity >= __OLD_SIZE && "Attempted to expand a buffer but actally reduced the size...");
        #endif
        if      (_capacity <  __OLD_SIZE)
            return NULL;
    }
    // Update the size. We can assume that any new
    // data will be written immediately
    _size += __S;
    return static_cast<uint8_t*>(_data)+__OLD_SIZE;
}
Result Buffer_t::append(void *__D, size_t __S)
{
    
    auto __OLD_SIZE = _size;
    
    // If there are insufficient bytes, expand the buffer
    if (available_bytes() < __S) {
        // A resize failure returns a null pointer
        if (Result grown = change_capacity(available_bytes() + __S); grown.failed())
            return grown;
        // Ensure we didn't make the buffer smaller.
        // resize can do that and that would be awkward...
        #if IRIS_DEBUG
        assert  (_size <= __OLD_SIZE);
        #endif
        if      (_size <= __OLD_SIZE)
            return Result::failure(Domain::IRIS_HEADERS, HeadersError::BUFFER_SIZE_NOT_GROWN,
                                   "append: the buffer's size " + std::to_string(_size) +
                                   " did not grow past " + std::to_string(__OLD_SIZE));
    }
    
    // Immediately write the new data to the buffer
    // and then update it's size.
    auto __ptr = static_cast<uint8_t*>(_data)+__OLD_SIZE;
    std::memcpy(__ptr, __D, __S);
    _size += __S;
    return Result::success();
}
size_t Buffer_t::size() const
{
    return _size;
}
Result Buffer_t::set_size(size_t __bytes)
{
    _size = __bytes;
    return Result::success();
}
size_t Buffer_t::capacity() const
{
    return _capacity;
}
size_t Buffer_t::available_bytes() const
{
    return _capacity > _size ? _capacity - _size : 0;
}
Result Buffer_t::change_capacity(size_t capacity)
{
    // If you hit this asssert, you are attempting to
    // resize a weak reference. You CANNOT do this as
    // it may invalidate the original pointer, which this
    // Buffer only has accesss privilage to.
    #if IRIS_DEBUG
    assert(_strength == REFERENCE_STRONG);
    #endif
    if (_strength == REFERENCE_WEAK)
        return Result::failure(Domain::IRIS_HEADERS, HeadersError::BUFFER_WEAK_REFERENCE_RESIZE,
                               "change_capacity: a weak buffer cannot be resized to " +
                               std::to_string(capacity) + " bytes");
    
    // IF this is unnessary, return true
    if (capacity == _capacity)
        return Result::success();
    
    // Else reallocate the pointer, invalidating the old one
    if (auto __ptr = std::realloc(_data, capacity)) {
        _size = (_size < capacity) ? _size : capacity;
        const_cast<size_t&> (_capacity) = capacity;
        const_cast<void*&>  (_data)     = __ptr;
        return Result::success();
    }
    return Result::failure(Domain::IRIS_HEADERS, HeadersError::BUFFER_REALLOCATION_FAILED,
                           "change_capacity: realloc to " + std::to_string(capacity) + " bytes failed");
}
Result Buffer_t::shrink_to_fit()
{
    return change_capacity(_size);
}
} // END IRIS NAMESPACE
