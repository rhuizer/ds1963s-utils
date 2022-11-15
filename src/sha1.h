#ifndef SHA1_INCLUDE
#define SHA1_INCLUDE

#include <stdint.h>

#define SHA1_SIGNATURE_SIZE 20

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

extern void SHA1_Init(SHA1_CTX *);
extern void SHA1_Update(SHA1_CTX *, const unsigned char *, unsigned int);
extern void SHA1_Final(unsigned char[SHA1_SIGNATURE_SIZE], SHA1_CTX *);

#endif /* SHA1_INCLUDE_ */
