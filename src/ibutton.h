#ifndef __IBUTTON_H
#define __IBUTTON_H

#define IBUTTON_SERIAL_SIZE	32

#define WRITE_CYCLE_DATA_8	0
#define WRITE_CYCLE_DATA_9	1
#define WRITE_CYCLE_DATA_10	2
#define WRITE_CYCLE_DATA_11	3
#define WRITE_CYCLE_DATA_12	4
#define WRITE_CYCLE_DATA_13	5
#define WRITE_CYCLE_DATA_14	6
#define WRITE_CYCLE_DATA_15	7
#define WRITE_CYCLE_SECRET_0	8
#define WRITE_CYCLE_SECRET_1	9
#define WRITE_CYCLE_SECRET_2	10
#define WRITE_CYCLE_SECRET_3	11
#define WRITE_CYCLE_SECRET_4	12
#define WRITE_CYCLE_SECRET_5	13
#define WRITE_CYCLE_SECRET_6	14
#define WRITE_CYCLE_SECRET_7	15

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

uint32_t ibutton_write_cycle_get(int portnum, int write_cycle_type);

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif	/* !__IBUTTON_H */
