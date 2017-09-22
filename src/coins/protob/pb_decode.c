/* pb_decode.c -- decode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb.h"
#include "pb_decode.h"

/**************************************
 * Declarations internal to this file *
 **************************************/

/* Iterator for pb_field_t list */
typedef struct {
    const pb_field_t *start; /* Start of the pb_field_t array */
    const pb_field_t *pos; /* Current position of the iterator */
    unsigned field_index; /* Zero-based index of the field. */
    unsigned required_field_index; /* Zero-based index that counts only the required fields */
    void *dest_struct; /* Pointer to the destination structure to decode to */
    void *pData; /* Pointer where to store current field value */
    void *pSize; /* Pointer where to store the size of current array field */
} pb_field_iterator_t;

typedef bool (*pb_decoder_t)(pb_istream_t *stream, const pb_field_t *field, void *dest);

static bool pb_decode_varint32(pb_istream_t *stream, uint32_t *dest);
static void pb_field_init(pb_field_iterator_t *iter, const pb_field_t *fields, void *dest_struct);
static bool pb_field_next(pb_field_iterator_t *iter);
static bool pb_field_find(pb_field_iterator_t *iter, uint32_t tag);
static bool decode_static_field(pb_istream_t *stream, pb_wire_type_t wire_type, pb_field_iterator_t *iter);
static bool decode_field(pb_istream_t *stream, pb_wire_type_t wire_type, pb_field_iterator_t *iter);
static void iter_from_extension(pb_field_iterator_t *iter, pb_extension_t *extension);
static bool default_extension_decoder(pb_istream_t *stream, pb_extension_t *extension, uint32_t tag, pb_wire_type_t wire_type);
static void pb_message_set_to_defaults(const pb_field_t fields[], void *dest_struct);
static bool pb_dec_varint(pb_istream_t *stream, const pb_field_t *field, void *dest);
static bool pb_dec_uvarint(pb_istream_t *stream, const pb_field_t *field, void *dest);
static bool pb_dec_svarint(pb_istream_t *stream, const pb_field_t *field, void *dest);
static bool pb_dec_fixed32(pb_istream_t *stream, const pb_field_t *field, void *dest);
static bool pb_dec_fixed64(pb_istream_t *stream, const pb_field_t *field, void *dest);
static bool pb_dec_bytes(pb_istream_t *stream, const pb_field_t *field, void *dest);
static bool pb_dec_string(pb_istream_t *stream, const pb_field_t *field, void *dest);
static bool pb_dec_submessage(pb_istream_t *stream, const pb_field_t *field, void *dest);
static bool pb_skip_varint(pb_istream_t *stream);
static bool pb_skip_string(pb_istream_t *stream);

/* --- Function pointers to field decoders ---
 * Order in the array must match pb_action_t LTYPE numbering.
 */
static const pb_decoder_t PB_DECODERS[PB_LTYPES_COUNT] = {
    &pb_dec_varint,
    &pb_dec_uvarint,
    &pb_dec_svarint,
    &pb_dec_fixed32,
    &pb_dec_fixed64,
    
    &pb_dec_bytes,
    &pb_dec_string,
    &pb_dec_submessage,
    NULL /* extensions */
};

/*******************************
 * pb_istream_t implementation *
 *******************************/

bool pb_read(pb_istream_t *stream, uint8_t *buf, size_t count)
{
	if (buf == NULL)
	{
		/* Skip input bytes */
		uint8_t tmp[8];
		while (count > 8)
		{
			if (!pb_read(stream, tmp, 8))
				return false;
			
			count -= 8;
		}
		
		return pb_read(stream, tmp, count);
	}

  if (stream->bytes_left < count)
    PB_RETURN_ERROR(stream, "end-of-stream");
    
  if (!stream->callback(stream, buf, count))
    PB_RETURN_ERROR(stream, "io error");
    
  stream->bytes_left -= count;
  return true;
}

/* Read a single byte from input stream. buf may not be NULL.
 * This is an optimization for the varint decoding. */
static bool pb_readbyte(pb_istream_t *stream, uint8_t *buf)
{
    if (stream->bytes_left == 0)
        PB_RETURN_ERROR(stream, "end-of-stream");

    if (!stream->callback(stream, buf, 1))
        PB_RETURN_ERROR(stream, "io error");

    stream->bytes_left--;    
    return true;    
}

/********************
 * Helper functions *
 ********************/

static bool pb_decode_varint32(pb_istream_t *stream, uint32_t *dest)
{
    uint8_t byte;
    uint32_t result;
    
    if (!pb_readbyte(stream, &byte))
        return false;
    
    if ((byte & 0x80) == 0)
    {
        /* Quick case, 1 byte value */
        result = byte;
    }
    else
    {
        /* Multibyte case */
        uint8_t bitpos = 7;
        result = byte & 0x7F;
        
        do
        {
            if (bitpos >= 32)
                PB_RETURN_ERROR(stream, "varint overflow");
            
            if (!pb_readbyte(stream, &byte))
                return false;
            
            result |= (uint32_t)(byte & 0x7F) << bitpos;
            bitpos = (uint8_t)(bitpos + 7);
        } while (byte & 0x80);
   }
   
   *dest = result;
   return true;
}

bool pb_decode_varint(pb_istream_t *stream, uint64_t *dest)
{
    uint8_t byte;
    uint8_t bitpos = 0;
    uint64_t result = 0;
    
    do
    {
        if (bitpos >= 64)
            PB_RETURN_ERROR(stream, "varint overflow");
        
        if (!pb_readbyte(stream, &byte))
            return false;

        result |= (uint64_t)(byte & 0x7F) << bitpos;
        bitpos = (uint8_t)(bitpos + 7);
    } while (byte & 0x80);
    
    *dest = result;
    return true;
}

bool pb_skip_varint(pb_istream_t *stream)
{
    uint8_t byte;
    do
    {
        if (!pb_read(stream, &byte, 1))
            return false;
    } while (byte & 0x80);
    return true;
}

bool pb_skip_string(pb_istream_t *stream)
{
    uint32_t length;
    if (!pb_decode_varint32(stream, &length))
        return false;
    
    return pb_read(stream, NULL, length);
}

bool pb_decode_tag(pb_istream_t *stream, pb_wire_type_t *wire_type, uint32_t *tag, bool *eof)
{
    uint32_t temp;
    *eof = false;
    *wire_type = (pb_wire_type_t) 0;
    *tag = 0;
    
    if (!pb_decode_varint32(stream, &temp))
    {
        if (stream->bytes_left == 0)
            *eof = true;

        return false;
    }
    
    if (temp == 0)
    {
        *eof = true; /* Special feature: allow 0-terminated messages. */
        return false;
    }
    
    *tag = temp >> 3;
    *wire_type = (pb_wire_type_t)(temp & 7);
    return true;
}

bool pb_skip_field(pb_istream_t *stream, pb_wire_type_t wire_type)
{
    switch (wire_type)
    {
        case PB_WT_VARINT: return pb_skip_varint(stream);
        case PB_WT_64BIT: return pb_read(stream, NULL, 8);
        case PB_WT_STRING: return pb_skip_string(stream);
        case PB_WT_32BIT: return pb_read(stream, NULL, 4);
        default: PB_RETURN_ERROR(stream, "invalid wire_type");
    }
}

/* Decode string length from stream and return a substream with limited length.
 * Remember to close the substream using pb_close_string_substream().
 */
bool pb_make_string_substream(pb_istream_t *stream, pb_istream_t *substream)
{
    uint32_t size;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    *substream = *stream;
    if (substream->bytes_left < size)
        PB_RETURN_ERROR(stream, "parent stream too short");
    
    substream->bytes_left = size;
    stream->bytes_left -= size;
    return true;
}

void pb_close_string_substream(pb_istream_t *stream, pb_istream_t *substream)
{
    stream->state = substream->state;
    stream->errmsg = substream->errmsg;
}

static void pb_field_init(pb_field_iterator_t *iter, const pb_field_t *fields, void *dest_struct)
{
    iter->start = iter->pos = fields;
    iter->field_index = 0;
    iter->required_field_index = 0;
    iter->pData = (char*)dest_struct + iter->pos->data_offset;
    iter->pSize = (char*)iter->pData + iter->pos->size_offset;
    iter->dest_struct = dest_struct;
}

static bool pb_field_next(pb_field_iterator_t *iter)
{
    bool notwrapped = true;
    size_t prev_size = iter->pos->data_size;
    
    if (PB_ATYPE(iter->pos->type) == PB_ATYPE_STATIC &&
        PB_HTYPE(iter->pos->type) == PB_HTYPE_REPEATED)
    {
        prev_size *= iter->pos->array_size;
    }
    else if (PB_ATYPE(iter->pos->type) == PB_ATYPE_POINTER)
    {
        prev_size = sizeof(void*);
    }
    
    if (iter->pos->tag == 0)
        return false; /* Only happens with empty message types */
    
    if (PB_HTYPE(iter->pos->type) == PB_HTYPE_REQUIRED)
        iter->required_field_index++;
    
    iter->pos++;
    iter->field_index++;
    if (iter->pos->tag == 0)
    {
        iter->pos = iter->start;
        iter->field_index = 0;
        iter->required_field_index = 0;
        iter->pData = iter->dest_struct;
        prev_size = 0;
        notwrapped = false;
    }
    
    iter->pData = (char*)iter->pData + prev_size + iter->pos->data_offset;
    iter->pSize = (char*)iter->pData + iter->pos->size_offset;
    return notwrapped;
}

static bool pb_field_find(pb_field_iterator_t *iter, uint32_t tag)
{
    unsigned start = iter->field_index;
    
    do {
        if (iter->pos->tag == tag &&
            PB_LTYPE(iter->pos->type) != PB_LTYPE_EXTENSION)
        {
            return true;
        }
        (void)pb_field_next(iter);
    } while (iter->field_index != start);
    
    return false;
}

/*************************
 * Decode a single field *
 *************************/

static bool decode_static_field(pb_istream_t *stream, pb_wire_type_t wire_type, pb_field_iterator_t *iter)
{
    pb_type_t type;
    pb_decoder_t func;
    
    type = iter->pos->type;
    func = PB_DECODERS[PB_LTYPE(type)];

    switch (PB_HTYPE(type))
    {
        case PB_HTYPE_REQUIRED:
            return func(stream, iter->pos, iter->pData);
            
        case PB_HTYPE_OPTIONAL:
            *(bool*)iter->pSize = true;
            return func(stream, iter->pos, iter->pData);
    
        case PB_HTYPE_REPEATED:
            if (wire_type == PB_WT_STRING
                && PB_LTYPE(type) <= PB_LTYPE_LAST_PACKABLE)
            {
                /* Packed array */
                bool status = true;
                size_t *size = (size_t*)iter->pSize;
                pb_istream_t substream;
                if (!pb_make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left > 0 && *size < iter->pos->array_size)
                {
                    void *pItem = (uint8_t*)iter->pData + iter->pos->data_size * (*size);
                    if (!func(&substream, iter->pos, pItem))
                    {
                        status = false;
                        break;
                    }
                    (*size)++;
                }
                pb_close_string_substream(stream, &substream);
                
                if (substream.bytes_left != 0)
                    PB_RETURN_ERROR(stream, "array overflow");
                
                return status;
            }
            else
            {
                /* Repeated field */
                size_t *size = (size_t*)iter->pSize;
                void *pItem = (uint8_t*)iter->pData + iter->pos->data_size * (*size);
                if (*size >= iter->pos->array_size)
                    PB_RETURN_ERROR(stream, "array overflow");
                
                (*size)++;
                return func(stream, iter->pos, pItem);
            }

        default:
            PB_RETURN_ERROR(stream, "invalid field type");
    }
}


static bool decode_pointer_field(pb_istream_t *stream, pb_wire_type_t wire_type, pb_field_iterator_t *iter)
{
    UNUSED(wire_type);
    UNUSED(iter);
    PB_RETURN_ERROR(stream, "no malloc support");
}

static bool decode_field(pb_istream_t *stream, pb_wire_type_t wire_type, pb_field_iterator_t *iter)
{
    switch (PB_ATYPE(iter->pos->type))
    {
        case PB_ATYPE_STATIC:
            return decode_static_field(stream, wire_type, iter);
        
        case PB_ATYPE_POINTER:
            return decode_pointer_field(stream, wire_type, iter);

        default:
            PB_RETURN_ERROR(stream, "invalid field type");
    }
}

static void iter_from_extension(pb_field_iterator_t *iter, pb_extension_t *extension)
{
    const pb_field_t *field = (const pb_field_t*)extension->type->arg;
    
    iter->start = field;
    iter->pos = field;
    iter->field_index = 0;
    iter->required_field_index = 0;
    iter->dest_struct = extension->dest;
    iter->pData = extension->dest;
    iter->pSize = &extension->found;
}

/* Default handler for extension fields. Expects a pb_field_t structure
 * in extension->type->arg. */
static bool default_extension_decoder(pb_istream_t *stream,
    pb_extension_t *extension, uint32_t tag, pb_wire_type_t wire_type)
{
    const pb_field_t *field = (const pb_field_t*)extension->type->arg;
    pb_field_iterator_t iter;
    
    if (field->tag != tag)
        return true;
    
    iter_from_extension(&iter, extension);
    return decode_field(stream, wire_type, &iter);
}

/* Try to decode an unknown field as an extension field. Tries each extension
 * decoder in turn, until one of them handles the field or loop ends. */
static bool decode_extension(pb_istream_t *stream,
    uint32_t tag, pb_wire_type_t wire_type, pb_field_iterator_t *iter)
{
    pb_extension_t *extension = *(pb_extension_t* const *)iter->pData;
    size_t pos = stream->bytes_left;
    
    while (extension != NULL && pos == stream->bytes_left)
    {
        bool status;
        if (extension->type->decode)
            status = extension->type->decode(stream, extension, tag, wire_type);
        else
            status = default_extension_decoder(stream, extension, tag, wire_type);

        if (!status)
            return false;
        
        extension = extension->next;
    }
    
    return true;
}

/* Step through the iterator until an extension field is found or until all
 * entries have been checked. There can be only one extension field per
 * message. Returns false if no extension field is found. */
static bool find_extension_field(pb_field_iterator_t *iter)
{
    unsigned start = iter->field_index;
    
    do {
        if (PB_LTYPE(iter->pos->type) == PB_LTYPE_EXTENSION)
            return true;
        (void)pb_field_next(iter);
    } while (iter->field_index != start);
    
    return false;
}

/* Initialize message fields to default values, recursively */
static void pb_message_set_to_defaults(const pb_field_t fields[], void *dest_struct)
{
    pb_field_iterator_t iter;
    pb_field_init(&iter, fields, dest_struct);
    
    do
    {
        pb_type_t type;
        type = iter.pos->type;
    
        /* Avoid crash on empty message types (zero fields) */
        if (iter.pos->tag == 0)
            continue;
        
        if (PB_ATYPE(type) == PB_ATYPE_STATIC)
        {
            if (PB_HTYPE(type) == PB_HTYPE_OPTIONAL)
            {
                /* Set has_field to false. Still initialize the optional field
                 * itself also. */
                *(bool*)iter.pSize = false;
            }
            else if (PB_HTYPE(type) == PB_HTYPE_REPEATED)
            {
                /* Set array count to 0, no need to initialize contents. */
                *(size_t*)iter.pSize = 0;
                continue;
            }
            
            if (PB_LTYPE(iter.pos->type) == PB_LTYPE_SUBMESSAGE)
            {
                /* Initialize submessage to defaults */
                pb_message_set_to_defaults((const pb_field_t *) iter.pos->ptr, iter.pData);
            }
            else if (iter.pos->ptr != NULL)
            {
                /* Initialize to default value */
                memcpy(iter.pData, iter.pos->ptr, iter.pos->data_size);
            }
            else
            {
                /* Initialize to zeros */
                memset(iter.pData, 0, iter.pos->data_size);
            }
        }
        else if (PB_ATYPE(type) == PB_ATYPE_POINTER)
        {
            /* Initialize the pointer to NULL. */
            *(void**)iter.pData = NULL;
            
            /* Initialize array count to 0. */
            if (PB_HTYPE(type) == PB_HTYPE_REPEATED)
            {
                *(size_t*)iter.pSize = 0;
            }
        }
    } while (pb_field_next(&iter));
}

/*********************
 * Decode all fields *
 *********************/

bool pb_decode_noinit(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct)
{
    uint8_t fields_seen[(PB_MAX_REQUIRED_FIELDS + 7) / 8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t extension_range_start = 0;
    pb_field_iterator_t iter;
    
    pb_field_init(&iter, fields, dest_struct);
    
    while (stream->bytes_left)
    {
        uint32_t tag;
        pb_wire_type_t wire_type;
        bool eof;

        if (!pb_decode_tag(stream, &wire_type, &tag, &eof))
        {
            if (eof)
                break;
            else
                return false;
        }
        
        if (!pb_field_find(&iter, tag))
        {
            /* No match found, check if it matches an extension. */
            if (tag >= extension_range_start)
            {
                if (!find_extension_field(&iter))
                    extension_range_start = (uint32_t)-1;
                else
                    extension_range_start = iter.pos->tag;
                
                if (tag >= extension_range_start)
                {
                    size_t pos = stream->bytes_left;
                
                    if (!decode_extension(stream, tag, wire_type, &iter))
                        return false;
                    
                    if (pos != stream->bytes_left)
                    {
                        /* The field was handled */
                        continue;                    
                    }
                }
            }
        
            /* No match found, skip data */
            if (!pb_skip_field(stream, wire_type))
                return false;
            continue;
        }
        
        if (PB_HTYPE(iter.pos->type) == PB_HTYPE_REQUIRED
            && iter.required_field_index < PB_MAX_REQUIRED_FIELDS)
        {
            fields_seen[iter.required_field_index >> 3] |= (uint8_t)(1 << (iter.required_field_index & 7));
        }
            
        if (!decode_field(stream, wire_type, &iter))
            return false;
    }
    
    /* Check that all required fields were present. */
    {
        /* First figure out the number of required fields by
         * seeking to the end of the field array. Usually we
         * are already close to end after decoding.
         */
        unsigned req_field_count;
        pb_type_t last_type;
        unsigned i;
        do {
            req_field_count = iter.required_field_index;
            last_type = iter.pos->type;
        } while (pb_field_next(&iter));
        
        /* Fixup if last field was also required. */
        if (PB_HTYPE(last_type) == PB_HTYPE_REQUIRED && iter.pos->tag != 0)
            req_field_count++;
        
        /* Check the whole bytes */
        for (i = 0; i < (req_field_count >> 3); i++)
        {
            if (fields_seen[i] != 0xFF)
                PB_RETURN_ERROR(stream, "missing required field");
        }
        
        /* Check the remaining bits */
        if (fields_seen[req_field_count >> 3] != (0xFF >> (8 - (req_field_count & 7))))
            PB_RETURN_ERROR(stream, "missing required field");
    }
    
    return true;
}

bool pb_decode(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct)
{
    bool status;
    pb_message_set_to_defaults(fields, dest_struct);
    status = pb_decode_noinit(stream, fields, dest_struct);
    
    return status;
}

bool pb_decode_delimited(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct)
{
    pb_istream_t substream;
    bool status;
    
    if (!pb_make_string_substream(stream, &substream))
        return false;
    
    status = pb_decode(&substream, fields, dest_struct);
    pb_close_string_substream(stream, &substream);
    return status;
}


/* Field decoders */

bool pb_decode_svarint(pb_istream_t *stream, int64_t *dest)
{
    uint64_t value;
    if (!pb_decode_varint(stream, &value))
        return false;
    
    if (value & 1)
        *dest = (int64_t)(~(value >> 1));
    else
        *dest = (int64_t)(value >> 1);
    
    return true;
}

bool pb_decode_fixed32(pb_istream_t *stream, void *dest)
{
    #ifdef __BIG_ENDIAN__
    uint8_t *bytes = (uint8_t*)dest;
    uint8_t lebytes[4];
    
    if (!pb_read(stream, lebytes, 4))
        return false;
    
    bytes[0] = lebytes[3];
    bytes[1] = lebytes[2];
    bytes[2] = lebytes[1];
    bytes[3] = lebytes[0];
    return true;
    #else
    return pb_read(stream, (uint8_t*)dest, 4);
    #endif   
}

bool pb_decode_fixed64(pb_istream_t *stream, void *dest)
{
    #ifdef __BIG_ENDIAN__
    uint8_t *bytes = (uint8_t*)dest;
    uint8_t lebytes[8];
    
    if (!pb_read(stream, lebytes, 8))
        return false;
    
    bytes[0] = lebytes[7];
    bytes[1] = lebytes[6];
    bytes[2] = lebytes[5];
    bytes[3] = lebytes[4];
    bytes[4] = lebytes[3];
    bytes[5] = lebytes[2];
    bytes[6] = lebytes[1];
    bytes[7] = lebytes[0];
    return true;
    #else
    return pb_read(stream, (uint8_t*)dest, 8);
    #endif   
}

static bool pb_dec_varint(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint64_t value;
    if (!pb_decode_varint(stream, &value))
        return false;
    
    switch (field->data_size)
    {
        case 1: *(int8_t*)dest = (int8_t)value; break;
        case 2: *(int16_t*)dest = (int16_t)value; break;
        case 4: *(int32_t*)dest = (int32_t)value; break;
        case 8: *(int64_t*)dest = (int64_t)value; break;
        default: PB_RETURN_ERROR(stream, "invalid data_size");
    }
    
    return true;
}

static bool pb_dec_uvarint(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint64_t value;
    if (!pb_decode_varint(stream, &value))
        return false;
    
    switch (field->data_size)
    {
        case 4: *(uint32_t*)dest = (uint32_t)value; break;
        case 8: *(uint64_t*)dest = value; break;
        default: PB_RETURN_ERROR(stream, "invalid data_size");
    }
    
    return true;
}

static bool pb_dec_svarint(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    int64_t value;
    if (!pb_decode_svarint(stream, &value))
        return false;
    
    switch (field->data_size)
    {
        case 4: *(int32_t*)dest = (int32_t)value; break;
        case 8: *(int64_t*)dest = value; break;
        default: PB_RETURN_ERROR(stream, "invalid data_size");
    }
    
    return true;
}

static bool pb_dec_fixed32(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    UNUSED(field);
    return pb_decode_fixed32(stream, dest);
}

static bool pb_dec_fixed64(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    UNUSED(field);
    return pb_decode_fixed64(stream, dest);
}

static bool pb_dec_bytes(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t size;
    size_t alloc_size;
    pb_bytes_array_t *bdest;
    
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    alloc_size = PB_BYTES_ARRAY_T_ALLOCSIZE(size);
    if (size > alloc_size)
        PB_RETURN_ERROR(stream, "size too large");
    
    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
        PB_RETURN_ERROR(stream, "no malloc support");
    }
    else
    {
        if (alloc_size > field->data_size)
            PB_RETURN_ERROR(stream, "bytes overflow");
        bdest = (pb_bytes_array_t*)dest;
    }
    
    bdest->size = size;

    return pb_read(stream, bdest->bytes, size);
}

static bool pb_dec_string(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t size;
    size_t alloc_size;
    bool status;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    /* Space for null terminator */
    alloc_size = size + 1;
    
    if (alloc_size < size)
        PB_RETURN_ERROR(stream, "size too large");
    
    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
        PB_RETURN_ERROR(stream, "no malloc support");
    }
    else
    {
        if (alloc_size > field->data_size)
            PB_RETURN_ERROR(stream, "string overflow");
    }
    
    status = pb_read(stream, (uint8_t*)dest, size);
    *((uint8_t*)dest + size) = 0;
    return status;
}

static bool pb_dec_submessage(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    bool status;
    pb_istream_t substream;
    const pb_field_t* submsg_fields = (const pb_field_t*)field->ptr;
    
    if (!pb_make_string_substream(stream, &substream))
        return false;
    
    if (field->ptr == NULL)
        PB_RETURN_ERROR(stream, "invalid field descriptor");
    
    /* New array entries need to be initialized, while required and optional
     * submessages have already been initialized in the top-level pb_decode. */
    if (PB_HTYPE(field->type) == PB_HTYPE_REPEATED)
        status = pb_decode(&substream, submsg_fields, dest);
    else
        status = pb_decode_noinit(&substream, submsg_fields, dest);
    
    pb_close_string_substream(stream, &substream);
    return status;
}
