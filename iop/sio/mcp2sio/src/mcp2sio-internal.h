/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2009 jimmikaelkael
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#ifndef __MCP2SIO_INTERNAL_H__
#define __MCP2SIO_INTERNAL_H__

#include "stdint.h"
#define MODNAME "mcp2sio"

#ifdef SIO_DEBUG
	#include <sior.h>
	#define DEBUG
	#define DPRINTF(format, args...) \
		sio_printf(MODNAME ": " format, ##args)
#else
	#ifdef DEBUG
		#define DPRINTF(format, args...) \
			printf(MODNAME ": " format, ##args)
    #else
		#define DPRINTF(format, args...)
    #endif
#endif

typedef enum spisd_result_s {
    SPISD_RESULT_OK = 0,
    SPISD_RESULT_ERROR,
    SPISD_RESULT_NO_CARD,
    SPISD_RESULT_TIMEOUT,
} spisd_result_t;

#define SIO2_REL_BASE 0x1F808000
#define SIO2_SEG      0xA0000000 // Uncached
#define SIO2_BASE     (SIO2_REL_BASE + SIO2_SEG)

#define SIO2_REG_BASE        (SIO2_BASE + 0x200)
#define SIO2_REG_PORT0_CTRL1 (SIO2_BASE + 0x240)
#define SIO2_REG_PORT0_CTRL2 (SIO2_BASE + 0x244)
#define SIO2_REG_DATA_OUT    (SIO2_BASE + 0x260)
#define SIO2_REG_DATA_IN     (SIO2_BASE + 0x264)
#define SIO2_REG_CTRL        (SIO2_BASE + 0x268)
#define SIO2_REG_STAT6C      (SIO2_BASE + 0x26C)
#define SIO2_REG_STAT70      (SIO2_BASE + 0x270)
#define SIO2_REG_STAT74      (SIO2_BASE + 0x274)
#define SIO2_REG_UNKN78      (SIO2_BASE + 0x278)
#define SIO2_REG_UNKN7C      (SIO2_BASE + 0x27C)
#define SIO2_REG_STAT        (SIO2_BASE + 0x280)

#define SIO2_A_QUEUE       (SIO2_BASE + 0x200)
#define SIO2_A_PORT0_CTRL1 (SIO2_BASE + 0x240)
#define SIO2_A_PORT0_CTRL2 (SIO2_BASE + 0x244)
#define SIO2_A_DATA_OUT    (SIO2_BASE + 0x260)
#define SIO2_A_DATA_IN     (SIO2_BASE + 0x264)
#define SIO2_A_CTRL        (SIO2_BASE + 0x268)
#define SIO2_A_STAT6C      (SIO2_BASE + 0x26C)
#define SIO2_A_CONN_STAT70 (SIO2_BASE + 0x270)
#define SIO2_A_FIFO_STAT74 (SIO2_BASE + 0x274)
#define SIO2_A_TX_FIFO_PT  (SIO2_BASE + 0x278)
#define SIO2_A_RX_FIFO_PT  (SIO2_BASE + 0x27C)
#define SIO2_A_INTR_STAT   (SIO2_BASE + 0x280)

#define PCTRL0_ATT_LOW_PER(x)      ((((u32)(x)) << 0) & 0xFF)
#define PCTRL0_ATT_MIN_HIGH_PER(x) ((((u32)(x)) << 8) & 0xFF00)
#define PCTRL0_BAUD0_DIV(x)        ((((u32)(x)) << 16) & 0xFF0000)
#define PCTRL0_BAUD1_DIV(x)        ((((u32)(x)) << 24) & 0xFF000000)

#define PCTRL1_ACK_TIMEOUT_PER(x)  ((((u32)(x)) << 0) & 0xFFFF)
#define PCTRL1_INTER_BYTE_PER(x)   ((((u32)(x)) << 16) & 0xFF0000)
#define PCTRL1_UNK24(x)            ((((u32)(x))&1) << 24)
#define PCTRL1_IF_MODE_SPI_DIFF(x) ((((u32)(x))&1) << 25)

#define TR_CTRL_PORT_NR(x)          ((((u32)(x))&0x3) << 0)
#define TR_CTRL_PAUSE(x)            ((((u32)(x))&1) << 2)
#define TR_CTRL_UNK03(x)            ((((u32)(x))&1) << 3)
// Each of the folowing is 0 for PIO transfer in the given direction. to select DMA, set to 1.
#define TR_CTRL_TX_MODE_PIO_DMA(x)  ((((u32)(x))&1) << 4)
#define TR_CTRL_RX_MODE_PIO_DMA(x)  ((((u32)(x))&1) << 5)
/*
normal    special
0            0                no transfer done(???)
1            0                normal transfer - usually used
0            1                "special" transfer (not known to have been ever used by anything, no known difference from normal).
1            1                no transfer takes place
*/
#define TR_CTRL_NORMAL_TR(x)        (((u32)((x))&1) << 6)
#define TR_CTRL_SPECIAL_TR(x)       (((u32)((x))&1) << 7)
// In bytes 0 - 0x100:
#define TR_CTRL_TX_DATA_SZ(x)       ((((u32)(x))&0x1FF) << 8)
#define TR_CTRL_UNK17(x)            ((((u32)(x))&1) << 17)
#define TR_CTRL_RX_DATA_SZ(x)       ((((u32)(x))&0x1FF) << 18)
#define TR_CTRL_UNK27(x)            ((((u32)(x))&1) << 27)
// 28 and 29 can't be set
#define TR_CTRL_UNK28(x)            ((((u32)(x))&1) << 28)
#define TR_CTRL_UNK29(x)            ((((u32)(x))&1) << 29)
// selects between baud rate divisors 0 and 1
#define TR_CTRL_BAUD_DIV(x)         ((((u32)(x))&1) << 30)
#define TR_CTRL_WAIT_ACK_FOREVER(x) ((((u32)(x))&1) << 31)

/* 04 */ static inline void inl_sio2_ctrl_set(u32 val) { _sw(val, SIO2_REG_CTRL); }
/* 05 */ static inline u32 inl_sio2_ctrl_get() { return _lw(SIO2_REG_CTRL); }
/* 06 */ static inline u32 inl_sio2_stat6c_get() { return _lw(SIO2_REG_STAT6C); }
/* 07 */ static inline void inl_sio2_portN_ctrl1_set(int N, u32 val) { _sw(val, SIO2_REG_PORT0_CTRL1 + (N * 8)); }
/* 08 */ static inline u32 inl_sio2_portN_ctrl1_get(int N) { return _lw(SIO2_REG_PORT0_CTRL1 + (N * 8)); }
/* 09 */ static inline void inl_sio2_portN_ctrl2_set(int N, u32 val) { _sw(val, SIO2_REG_PORT0_CTRL2 + (N * 8)); }
/* 10 */ static inline u32 inl_sio2_portN_ctrl2_get(int N) { return _lw(SIO2_REG_PORT0_CTRL2 + (N * 8)); }
/* 11 */ static inline u32 inl_sio2_stat70_get() { return _lw(SIO2_REG_STAT70); }
/* 12 */ static inline void inl_sio2_regN_set(int N, u32 val) { _sw(val, SIO2_REG_BASE + (N * 4)); }
/* 13 */ static inline u32 inl_sio2_regN_get(int N) { return _lw(SIO2_REG_BASE + (N * 4)); }
/* 14 */ static inline u32 inl_sio2_stat74_get() { return _lw(SIO2_REG_STAT74); }
/* 15 */ static inline void inl_sio2_unkn78_set(u32 val) { _sw(val, SIO2_REG_UNKN78); }
/* 16 */ static inline u32 inl_sio2_unkn78_get() { return _lw(SIO2_REG_UNKN78); }
/* 17 */ static inline void inl_sio2_unkn7c_set(u32 val) { _sw(val, SIO2_REG_UNKN7C); }
/* 18 */ static inline u32 inl_sio2_unkn7c_get() { return _lw(SIO2_REG_UNKN7C); }
/* 19 */ static inline void inl_sio2_data_out(u8 val) { _sb(val, SIO2_REG_DATA_OUT); }
/* 20 */ static inline u8 inl_sio2_data_in() { return _lb(SIO2_REG_DATA_IN); }
/* 21 */ static inline void inl_sio2_stat_set(u32 val) { _sw(val, SIO2_REG_STAT); }
/* 22 */ static inline u32 inl_sio2_stat_get() { return _lw(SIO2_REG_STAT); }

#endif // __MCP2SIO_INTERNAL_H__