//! \file
//! \brief checksum external routines


//! checksum state
struct checksum_t;
typedef struct checksum_t checksum_t;

//! checksum constructor
checksum_t* checksum_init(void);

//! update checksum
void checksum_update(checksum_t* cs,uint8_t* data,size_t size);
//! finalize checksum, relust pointer shall be at least CHECKSUM_SIZE. cs structure deallocated after call.
void checksum_finalize(checksum_t* cs,uint8_t* result);

