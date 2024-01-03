/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
                                                                          size_t char_offset, size_t *entry_offset_byte_rtn)
{
    int i;
    struct aesd_buffer_entry *entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, buffer, i)
    {
        if (i >= buffer->in_offs)
        {
            if (char_offset < entry->size)
            {
                *entry_offset_byte_rtn = char_offset;
                return entry;
            }
            else
            {
                char_offset -= entry->size;
            }
        }
    }

    AESD_CIRCULAR_BUFFER_FOREACH(entry, buffer, i)
    {
        if (i < buffer->in_offs)
        {
            if (char_offset < entry->size)
            {
                *entry_offset_byte_rtn = char_offset;
                return entry;
            }
            else
            {
                char_offset -= entry->size;
            }
        }
    }
    return NULL;
}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
 * If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
 * new start location.
 * Any necessary locking must be handled by the caller
 * Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
 */
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    // if buffer is full, overwrite oldest entry
    // return pointer to overwritten entry
    const char *old_buffprt = NULL;
    if (buffer->entry[buffer->in_offs].buffptr != NULL)
    {
        old_buffprt = buffer->entry[buffer->in_offs].buffptr;
        buffer->total_size -= buffer->entry[buffer->in_offs].size;
    }

    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;
    buffer->in_offs++;
    buffer->total_size += add_entry->size;

    if (buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        buffer->in_offs = 0;
    }

    if (buffer->full == true)
    {
        buffer->out_offs++;
        if (buffer->out_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        {
            buffer->out_offs = 0;
        }
    }
    else if (buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }
    return old_buffprt;
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
    buffer->full = false;
    buffer->in_offs = 0;
    buffer->out_offs = 0;
    buffer->total_size = 0;
}
