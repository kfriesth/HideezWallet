/*
 * NVRAM
 *
 * a) Permanent storage
 *
 *    Used for encryption keys, mac address list, etc. Preserved on firmware update.
 *    Storage space is hard-formatted, use nvram_erase_permanent and nvram_write_permanent
 *    function to write items to permanent space
 *
 * b) Dynamic storage
 *
 *    Used to store small records (up to 520 bytes) identified by type and key.
 *    Type range is 1..127, key range is 1..65535.
 *    Type is analog of database table name, key is a primary record key.
 *    Record can contain 32-bit numeric value, ASCIIZ string, or byte array.
 *    Dynamic storage API allows read/write/delete record by type and key,
 *    lookup for ASCIIZ string, enumerate records of some type.
 *    Parameter DSTORAGERESERVE should be set as compromise between "waste"
 *    space and frequency of cleanup operations (one cleanup operation per
 *    DSTORAGERESERVE / average_record_size write operations)
 *
 * c) Journal buffer (not implemented)
 *
 *    Used for journal data, written to circular buffer (older entries are deleted
 *    when new ones added). Each record have own 16-bit ID and data (length of data
 *    can be 0..JMAXRECORD). Write process is atomic. Space for new record is
 *    prepared beforehand. If records are written too quickly and there is no time
 *    to prepare space, record is not written and error will be returned.
 *
 * Global parameters
 *
 *   PSTORAGEADDR = address of permanent storage structure
 *   DSTORAGESIZE = total size of permanent storage structure
 *   DSTORAGEADDR = address of dynamic storage area
 *   DSTORAGESIZE = total size of dynamic storage area
 *   DSTORAGERESERVE = minimal empty space
 *   JBUFFERADDR = address of journal buffer
 *   JBUFFERSIZE = total size of journal buffer
 *   JMAXRECORD = maximum size of record data (not including header)
 *   TEMPBLKADDR = address of one NVRAM block for temporary storage (used when updating
 *                 permanent records)
 */

#ifndef _NVRAM_H_
#define _NVRAM_H_

#include "types.h"

#define NVRAM_MAX_RETRY  5
#define NVRAM_TIMEOUT    1000

#define NVRAM_PAGESIZE   1024

#define DSTORAGEADDR     0x29800 //0x25800//39000
#define DSTORAGESIZE     0x12800 //0x04000
#define DSTORAGERESERVE  0x00800

#define NVRAM_BLOCKSTART(p)             ((p) & ~(NVRAM_PAGESIZE - 1))

#define NVS_MAGIC                       0xca759302

#define NVS_TYPEBITS                    7
#define NVS_KEYBITS                     16
#define NVS_VLENBITS                    9
#define NVS_MAXRECORDSIZE               ((1 << NVS_VLENBITS) - 4)

#define NVS_MAXTYPE                     ((1 << NVS_TYPEBITS) - 1)
#define NVS_MAXKEY                      ((1 << NVS_KEYBITS) - 1)
#define NVS_TYPEOFFSET                  (NVS_KEYBITS+NVS_VLENBITS)
#define NVS_KEYOFFSET                   (NVS_VLENBITS)
#define NVS_TYPEMASK                    (0xffffffff << NVS_TYPEOFFSET)
#define NVS_KEYMASK                     ((0xffffffff << NVS_KEYOFFSET) & ~NVS_TYPEMASK)
#define NVS_VLENMASK                    (~(0xffffffff << NVS_KEYOFFSET))
#define NVS_EMPTYRECORD                 (0xffffffff)

#define NVS_RECORDHEADER(type,key,vlen) (((type) << NVS_TYPEOFFSET) | ((key) << NVS_KEYOFFSET) | (vlen))
#define NVS_RECORDTYPE(rh)              (((rh) & NVS_TYPEMASK) >> NVS_TYPEOFFSET)
#define NVS_RECORDKEY(rh)               (((rh) & NVS_KEYMASK) >> NVS_KEYOFFSET)
#define NVS_RECORDVLEN(rh)              ((rh) & NVS_VLENMASK)
#define NVS_STARTOFSTORAGE              (u32 *)(NVPTR(DSTORAGEADDR))
#define NVS_FIRSTRECORD                 (u32 *)(NVPTR(DSTORAGEADDR+4))
#define NVS_ENDOFSTORAGE                (u32 *)(NVPTR(DSTORAGEADDR+DSTORAGESIZE))
#define NVS_ISDELETED(rh)               ((rh & (NVS_TYPEMASK|NVS_KEYMASK)) == 0)
#define NVS_CONTENTWORDS(rh)            (NVS_RECORDVLEN(rh) >> 2)


#define SYSTEM_TABLE_DEV_MODULES  1

#define FIRST_USER_TABLE          33

#ifndef LINUX
  // for real device, simple address-to-pointer conversions
  typedef u32 nvram_addr_t;
  #define NVPTR(a)     (u8 *)(a)
  #define NVADDR(p)    (u32)(p)
#endif

extern u32 *nvs_empty_pointer; // for debugging
extern int nvs_used_space;

enum {
	NVRAM_OK = 0,
	NVRAM_BUSY,
	NVRAM_FAILURE,
	NVRAM_INEVENT
};

int nvram_event(int success);
u32 *nvs_getrecord(u8 type, u32 key);

/* --------------------- COMMON API ---------------------- */

/* 
 * nvram_init: initialize NVRAM control structures at startup
 */
void nvram_init(void);

/* --------------- PERMANENT STORAGE API ----------------- */

/* 
 * nvram_write_permanent: write data block to NVRAM
 * this function uses shared buffer. It is ok to have source data placed in
 * shared buffer but contents of buffer will be destroyed on return.
 * return NVRAM_OK on success or error code
 */
int  nvram_write_permanent(void *addr, const void *data, int len);

/* 
 * nvram_erase_permanent: erase NVRAM area
 * return NVRAM_OK on success or error code
 */
int  nvram_erase_permanent(void *addr, int len);

/* ---------------- DYNAMIC STORAGE API ------------------ */

/* 
 * nvs_write_record: store record data with given type and key.
 * if key==0, new unique key will be selected.
 * len is the length of data in bytes, it will not be stored anywhere,
 * so you should know exact size of written data or store it along with them.
 * All write functions use shared buffer, source data SHOULD NOT reside
 * in shared buffer.
 * return key of new record or 0 on error
 */
int nvs_write_record(u8 type, int key, const void *data, int len);

/* 
 * nvs_write_string: store ASCIIZ-string with given type and key.
 * if key==0, new unique key will be selected.
 * return key of new record or 0 on error
 */
int nvs_write_string(u8 type, int key, const char *str);

/* 
 * nvs_write_value: store 32-bit value with given type and key.
 * if key==0, new unique key will be selected.
 * return key of new record or 0 on error
 */
int nvs_write_value(u8 type, int key, int value);

/* 
 * nvs_read_record: find stored record data with given type and key,
 * return pointer to data or NULL if not found
 * if len is not NULL, it is filled with record length
 */
const void *nvs_read_record(u8 type, int key, int *len);

/* 
 * nvs_read_string: find stored ASCIIZ-string with given type and key,
 * return pointer to data or NULL if not found
 */
const char *nvs_read_string(u8 type, int key);

/* 
 * nvs_read_value: find stored 32-bit value with given type and key,
 * return value or deflt if not found
 */
int nvs_read_value(u8 type, int key, int deflt);

/* 
 * nvs_lookup_data: search record store for some data (data position
 * in record specified by offset), return record key or 0 if not found
 */
int nvs_lookup_data(u8 type, int offset, const u8 *data, int len);

/*
 * nvs_enum_records: enumerate records of given type (all records if type=0)
 * returns key of next record or 0 if no more records found.
 * data should be NULL at first call, every time it is filled with data pointer
 * (only for records writen with nvs_write_record or nvs_write_string)
 * example:
 *           void *data=NULL;
 *           u32 key;
 *           int len;
 *           while ((key = nvs_enum_records(5, &data, &len)) != 0) {
 *             process_record(key, data, len);
 *           }
 */
int nvs_enum_records(u8 type, void **data, int *len);

/* 
 * nvs_delete_record: delete record with given type and key.
 * pass key=0 to delete all records of given type
 * return number of deleted records, or 0 on error
 */
int nvs_delete_record(u8 type, int key);

/* 
 * nvs_available: return record count of given type (all records if type=0)
 */
int nvs_count(u8 type);

/* 
 * nvs_available: return available storage space (including deleted records)
 */
int nvs_available(void);

/*
 * nvs_cleanup: squeeze dynamic storage, purge deleted records
 */
int nvs_squeeze(void);

/*
 * nvs_wipe: wipe the whole storage
 */
int nvs_wipe(void);

/*
 * Non-volatile logging, intended for use on beta-testing devices with release build.
 * Records are written into free space after the end of firmware up to dynamic storage.
 * Size of firmware is read from offset 0x18. Storage space is used block-by-block.
 * Each block has a 8-byte header: 32-bit magic, then 32-bit sequence counter.
 * For every new written block the counter is incremented by 1, it is used during
 * initialization to find last record position. When log space is full, it is
 * overwritten by circle - next block is erased.
 * All records are aligned to 4-byte boundary and have length multiple of 4 bytes.
 * Record cannot span across flash blocks - when there is no space for current
 * record, it is written into next block, leaving some space at the tail unused.
 * New records are buffered in RAM and written asynchronously - when buffer is full
 * and periodically every NLCSYNCPERIOD.
 * A special case is recording of hardfault event - direct flash interface is used.
 * Maximum record size is 32 bytes (2 bytes header + up to 30 bytes data).
 *
 * NLOGD(type, data, dlen) - write new record of given type with optional data
 * NLOGV(type, value) - write new record of given type with 16-bit value
 *
 * Format of the record: [ e e e e e L L L ] [ t t t t t t B B ] [ data ... ]
 *
 * e: event ID (0-31)
 * L: length of additional data in 32-bit words (0-7)
 * t: time difference from previous record (0..4600 ms, exponential format)
 * B: number of bytes not used in the last 32-bit word (data length = 2 + L * 4 - B)
 */

#define NL_STARTUP        0
#define NL_SHUTDOWN       1
#define NL_SD_CONNECT     2
#define NL_SD_DISCONNECT  3
#define NL_SD_PKTIN       4
#define NL_SD_PKTOUT      5
#define NL_TS_CONNECT     6
#define NL_TS_DISCONNECT  7
#define NL_TS_PKTIN       8
#define NL_TS_PKTOUT      9
#define NL_BATTERY       10
#define NL_HARDFAULT     31

#ifdef FUNCTION_NLOGGER

void NLOGD(u8 type, const void *data, u8 dlen);
void NLOGV(u8 type, u16 value);
void nlog_exception(int type, unsigned long *args);

#else

#define NLOGD(type, data, dlen)
#define NLOGV(type, value)
#define nlog_exception(type, args)

#endif

#endif

