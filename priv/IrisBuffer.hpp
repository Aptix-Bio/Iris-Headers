/**
 * @file IrisBuffer.hpp
 * @author Ryan Landvater
 * @brief IrisBuffer reference counted datablock object header file
 * @version 0.1
 * @date 2024-10-23
 * 
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef IrisBuffer_hpp
#define IrisBuffer_hpp

// Self-contained: compiles with no other header first (StandaloneHeaderBuild_1.1).
#include <cstddef>
#include <IrisTypes.hpp>

//TODO: THIS IS CURRENTLY NOT THREAD SAFE! THERE CAN BE DATA RACES WITH THE CURRENT FORM. MUST UPDATE TO USE ATOMIC AND MUTEXES FOR THREAD SAFETY!

namespace Iris {
/**
 * @brief Private implementation of the reference counted data object used to wrap datablocks.
 *
 * It can either strong reference or weak reference the underlying data.
 * The buffer can also shift between weak and strong referrences if chosen;
 * however, this is very dangerous obviously and you need to ensure you
 * are tracking if you have switched from weak to strong or vice versa.
 *
 * \warning Use of the Buffer_t is given as a courtesy for developers
 * who want greater efficiency and control over datablocks. These methods
 * were created for the internal use by Iris Developers and come with some inherant risk.
 *
 * \warning Buffer_t is currently NOT thread safe. This will be fixed
 * in future updates. 
 */
class Buffer_t {
    BufferReferenceStrength         _strength   = REFERENCE_STRONG;
    size_t                          _capacity   = 0;
    size_t                          _size       = 0;
    void* const                     _data       = nullptr;
    
public:
    explicit Buffer_t     (BufferReferenceStrength) noexcept;
    explicit Buffer_t     (BufferReferenceStrength, size_t capacity) noexcept;
    explicit Buffer_t     (BufferReferenceStrength, const void* const data, size_t bytes) noexcept;
    Buffer_t              (const Buffer_t&) = delete;
    Buffer_t& operator =  (const Buffer_t&) = delete;
   ~Buffer_t              ();
   /**
    * @brief Returns pointer to the beginning of the underlying data block
    * 
    * @return void* const 
    */
    operator void* const                    () const;
    /**
     * @brief Operator to see if the buffer reference is valid
     * 
     * @return true if a valid data block backs this buffer
     * @return false if there is no valid data block backing this buffer
     */
    operator bool                           () const;
    /**
     * @brief Get the reference strength to the underlying data wrapped by the buffer.
     * 
     * @return REFERENCE_WEAK if the buffer only references the data and does not own it 
     * @return REFERENCE_STRONG if the buffer owns the data and controls the data lifetime
     */
    /// 
    BufferReferenceStrength get_strength    () const;
    /**
     * @brief Change the strength of the underlying reference. 
     * 
     * If switching to STRONG reference, this buffer will become responsible for the freeing
     * of the underlying data. Whatever mallocated the data is NO LONGER RESPONSIBLE
     * for FREEING the underlying data. 
     * If switched to WEAK reference, the buffer will give up the responsibility to free the data
     * and it now becomes the **responsibility of the program to avoid a memory leak**.
     * 
     * @param strength_to_assign REFERENCE_WEAK or REFERENCE_STRONG
     */
    Result      change_strength             (BufferReferenceStrength strength_to_assign);
    /**
     * @brief Returns pointer to the **beginning** of the underlying data block
     * 
     * This returns a pointer to the start of the datablock, not the start of unwritten data.
     * If you wish to write into the buffer, use prepare() or append().
     * \sa end(), prepare(), and append()
     * 
     * @return void* a pointer to the start of the internal data wrapped by the buffer.
     */
    void*       data                        () const;
    /**
     * @brief Returns a pointer to the next unwritten location in the buffer. 
     * 
     * This has no effect on the size of the buffer. If the buffer needs to be resized
     * use the append(size_t) method to expand the buffer. 
     * \sa append_(size_t)
     * \note If this extends beyond the capacity, a nullptr is returned.
     * 
     * @return void* that points to the next unwritten byte in the buffer.
     */
    void*       end                         () const;
    /**
     * @brief Extends the **capacity** of the buffer without affecting the size.
     * 
     * This extends the amount of data that the buffer can hold and may lead to a resizing of the
     * data block backing the buffer but without actually expanding the size of the committed data.
     * This is useful if you know how much data to expect but do not have that data yet. If you
     * wish to expand the size as well, see append(). 
     * \sa append()
     * 
     * \warning this may invalidate any prior reference to the data() pointer. Any local variables that
     * reference the data within this buffer may become immediately invalid. You may check
     * if resize changed the buffer by evaluating a comparison between data() before calling
     * and data() after calling.
     * 
     * @param amount_of_bytes_to_prepare the number of bytes by which to expand the buffer
     * @return a succeeded Result on successful extension of buffer capacity
     */
    Result      prepare                     (size_t amount_of_bytes_to_prepare);
    /**
     * @brief Expands the **size** of the buffer without writing data into the expanded space.
     * 
     * This expands the buffer size without actually writing any data into the new buffer space.
     * This is useful for streaming data where you may be aware of the size of incoming data but
     * need a space for that data (off the network card) to be copied. If this expansion goes
     * beyond available_bytes(), the data-block will be expanded similarly to prepare(size_t).
     * \sa prepare(size_t), available_bytes()
     * 
     * \warning this may invalidate any prior reference to the data() pointer. Any local variables that
     * reference the data within this buffer may become immediately invalid. You may check
     * if resize changed the buffer by evaluating a comparison between data() before calling
     * and data() after calling.
     * 
     * @param append_by_bytes the number of bytes by which to expand the buffer
     * @return void* to the beginning of **writable space** where new data should be added.
     */
    void*       append                      (size_t append_by_bytes);
    /**
     * @brief Appends the end of the buffer by copying data into the buffer
     * 
     * This works similarly to append(size_t) but performs a copy transaction, copying
     * the C-style array into the end of the current block.
     * 
     * \warning this may invalidate any prior reference to the data() pointer. Any local variables that
     * reference the data within this buffer may become immediately invalid. You may check
     * if resize changed the buffer by evaluating a comparison between data() before calling
     * and data() after calling.
     * 
     * @param data C-style pointer to data array
     * @param size Size of data in bytes
     * @return a succeeded Result on successful appending of data to end of buffer
     */
    Result      append                      (void* data, size_t size);
    /**
     * @brief Returns the current size of the buffer.
     * 
     * This is distinct from the buffer capacity
     * \sa capacity()
     * 
     * @return size_t number of bytes written to the buffer.
     */
    size_t      size                        () const;
    /**
     * @brief Set the size object
     * 
     * Assign the internal size of the complete buffer to the given size in bytes.
     * If you are simply adding some number of bytes to the buffer, use append(size) instead.
     * 
     * \note This is an unsafe method and generally should not be used without a really good reason.
     * 
     * \warning If the argument is less than the current buffer size, this will cut that data
     * to the given size. Unlike resize, the data will still persist but will be overwritten with
     * the next call of append. 
     * 
     * @param buffer_valid_size_bytes size in bytes to set the buffer's internal tracker
     * @return Result
     */
    Result       set_size                   (size_t buffer_valid_size_bytes);
    /**
     * @brief Returns the total capacity of the buffer (used and unused)
     * 
     * @return size_t capacity in bytes
     * @return a succeeded Result on successfully changing the internal size tracker
     * @return a failed Result (Domain::IRIS_HEADERS) on failure to change the tracker
     */
    size_t      capacity                    () const;
    /**
     * @brief Returns the remaining bytes within the buffer that are unused
     * 
     * This value is equivalent to capacity() - size(). 
     * 
     * @return size_t unused bytes in the current data block
     */
    size_t      available_bytes             () const;
    /**
     * @brief Resize the underlying datablock
     * 
     * This is fundamentally different from calling set_size(size_t) as it actually
     * changes the size of the backing data block by the kernel. Calling this method
     * can invalidate the underlying data pointer.
     * \note You should use prepare() 
     * \warning this may invalidate any prior reference to the data() pointer. Any local variables that
     * reference the data within this buffer may become immediately invalid. You may check
     * if resize changed the buffer by evaluating a comparison between data() before calling
     * and data() after calling.
     * 
     * @param new_buffer_capacity Size in bytes the buffer should be
     * @return a succeeded Result on successfully resizing the buffer object
     */
    Result      change_capacity             (size_t new_buffer_capacity);
    /**
     * @brief Shrinks the underlying data block to fit the used space
     * 
     * This is equivalent to calling resize(size()).
     * This will not invalidate any pointers and is generally a safe way
     * to reduce space consumption by buffers.
     * 
     * @return a succeeded Result on successfully resizing the buffer object
     */
    Result      shrink_to_fit               ();
};
} // END IRIS NAMESPACE

#endif /* IrisBuffer_hpp */
