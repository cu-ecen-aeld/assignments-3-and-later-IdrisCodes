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

    size_t current_char_offset = char_offset;
    const uint8_t finish_position = buffer->in_offs + (buffer->full ? 1 : 0) + (buffer->in_offs <= buffer->out_offs ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1 : 0);
    const uint8_t start_position = buffer->out_offs;

    uint8_t local_index = start_position;

    while (local_index < finish_position)
    {
        if (buffer->entry[local_index % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size > current_char_offset)
        {
            /* Char must be in this string entry */
            if (entry_offset_byte_rtn != NULL)
            {
                *entry_offset_byte_rtn = current_char_offset;
            }
            return &buffer->entry[local_index % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];
        }
        else
        {
            current_char_offset -= buffer->entry[local_index % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
            ++local_index;
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
const char * aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    const char * replaced = buffer->entry[buffer->in_offs].buffptr ;
    /* Write to current "IN" location */
    buffer->entry[buffer->in_offs] = *add_entry;

    /* Increment In offset */
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    /* Adjust out offset */
    if (buffer->full == true)
    {
        /* Buffer is already full, out_offs must point
            to the oldest element in the buffer */
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    else if (buffer->in_offs == buffer->out_offs)
    {
        /* Next write will overwrite oldest element */
        buffer->full = true;
    }

    return replaced;
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}
