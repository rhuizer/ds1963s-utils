#ifndef __ONEWIRE_H
#define __ONEWIRE_H

#ifdef __cplusplus
extern "C" {
#endif

int onewire_acquire(const char *port);
int onewire_ibutton_sha_find(int portnum, unsigned char *devAN);

#ifdef __cplusplus
};
#endif

#endif
