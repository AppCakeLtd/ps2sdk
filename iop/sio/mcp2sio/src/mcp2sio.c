// This include will allow us to avoid reincluding other headers
#include <sysmem.h>
#include "irx_imports.h"

#define MODNAME "mcp2sio"
IRX_ID(MODNAME, 1, 1);
#define WELCOME_STR "mcp2sio v1.1\n"

extern struct irx_export_table _exp_mcp2sio;


// This contain our prototype for our export, and we need it to call it inside our _start
#include "mcp2sio.h"
#include "mcp2sio-internal.h"
#include "sio2man_hook.h"

#define EF_SIO2_INTR_REVERSE  0x00000100
#define EF_SIO2_INTR_COMPLETE 0x00000200
#define PORT_NR                2
#define SECTOR_SIZE            512

static int event_flag          = -1;
static sio2_transfer_data_t global_td;
static int fastDiv = 1;
static u32 sio2man_save_crtl;

struct dma_command
{
    uint8_t *buffer;
    u16 sector_count;
    volatile u16 sectors_transferred; // written by isr, read by thread
    u16 sectors_reversed;
    u16 portNr;
#ifdef CONFIG_USE_CRC16
    u16 crc[MAX_SECTORS];
#endif
    uint8_t response;
    volatile uint8_t abort; // written by thread, read by isr
};
struct dma_command cmd;

void *malloc(int size){
	int OldState;
	void *result;

	CpuSuspendIntr(&OldState);
	result = AllocSysMemory(ALLOC_FIRST, size, NULL);
	CpuResumeIntr(OldState);

	return result;
}

void free(void *buffer){
	int OldState;

	CpuSuspendIntr(&OldState);
	FreeSysMemory(buffer);
	CpuResumeIntr(OldState);
}

static uint8_t sendCmd_Tx1_Rx1(uint8_t data, int portNr)
{
    inl_sio2_ctrl_set(0x0bc); // no interrupt

    inl_sio2_regN_set(0,
                      TR_CTRL_PORT_NR(portNr) |
                          TR_CTRL_PAUSE(0) |
                          TR_CTRL_TX_MODE_PIO_DMA(0) |
                          TR_CTRL_RX_MODE_PIO_DMA(0) |
                          TR_CTRL_NORMAL_TR(1) |
                          TR_CTRL_SPECIAL_TR(0) |
                          TR_CTRL_BAUD_DIV(fastDiv) |
                          TR_CTRL_WAIT_ACK_FOREVER(0) |
                          TR_CTRL_TX_DATA_SZ(1) |
                          TR_CTRL_RX_DATA_SZ(1));
    inl_sio2_regN_set(1, 0);

    // Put byte in queue
    inl_sio2_data_out(data);

    // Start queue exec
    inl_sio2_ctrl_set(inl_sio2_ctrl_get() | 1);

    // Wait for completion
    while ((inl_sio2_stat6c_get() & (1 << 12)) == 0)
        ;

    // Get byte from queue
    return inl_sio2_data_in();
}

static uint8_t wait_equal(uint8_t value, int count, int portNr)
{
    uint32_t i;
    uint8_t response = 0;

    for (i = 0; i < (uint32_t)count; i++) {
        response = sendCmd_Tx1_Rx1(0xff, portNr);
        if (response == value)
            break;
    }

    return (response != value) ? SPISD_RESULT_ERROR : SPISD_RESULT_OK;
}

static void sendCmd_Rx_DMA_start(uint8_t *rdBufA, int portNr)
{
    const uint32_t trSettingsDMA =
        TR_CTRL_PAUSE(0) |
        TR_CTRL_TX_MODE_PIO_DMA(0) |
        TR_CTRL_RX_MODE_PIO_DMA(1) |
        TR_CTRL_NORMAL_TR(1) |
        TR_CTRL_SPECIAL_TR(0) |
        TR_CTRL_TX_DATA_SZ(0) |
        TR_CTRL_RX_DATA_SZ(0x100) |
        TR_CTRL_BAUD_DIV(1) |
        TR_CTRL_WAIT_ACK_FOREVER(0);

    const uint32_t trSettingsPIO =
        TR_CTRL_PAUSE(0) |
        TR_CTRL_TX_MODE_PIO_DMA(0) |
        TR_CTRL_RX_MODE_PIO_DMA(0) |
        TR_CTRL_NORMAL_TR(1) |
        TR_CTRL_SPECIAL_TR(0) |
        TR_CTRL_TX_DATA_SZ(0) |
        TR_CTRL_RX_DATA_SZ(2) |
        TR_CTRL_BAUD_DIV(1) |
        TR_CTRL_WAIT_ACK_FOREVER(0);

    inl_sio2_ctrl_set(0x0bc); // no interrupt

    inl_sio2_regN_set(0, trSettingsDMA | TR_CTRL_PORT_NR(portNr));
    inl_sio2_regN_set(1, trSettingsDMA | TR_CTRL_PORT_NR(portNr));
    inl_sio2_regN_set(2, trSettingsPIO | TR_CTRL_PORT_NR(portNr));
    inl_sio2_regN_set(3, 0);

    dmac_request(IOP_DMAC_SIO2out, rdBufA, 0x100 >> 2, 2, DMAC_TO_MEM);
    dmac_transfer(IOP_DMAC_SIO2out);

    // Start queue exec
    inl_sio2_ctrl_set(inl_sio2_ctrl_get() | 1);
}

static void sendCmd_Tx_PIO(const uint8_t *wrBufA, int sndSz, int portNr)
{
    inl_sio2_ctrl_set(0x0bc); // no interrupt

    inl_sio2_regN_set(0,
                      TR_CTRL_PORT_NR(portNr) |
                          TR_CTRL_PAUSE(1) |
                          TR_CTRL_TX_MODE_PIO_DMA(0) |
                          TR_CTRL_RX_MODE_PIO_DMA(0) |
                          TR_CTRL_NORMAL_TR(1) |
                          TR_CTRL_SPECIAL_TR(0) |
                          TR_CTRL_BAUD_DIV(fastDiv) |
                          TR_CTRL_WAIT_ACK_FOREVER(0) |
                          TR_CTRL_TX_DATA_SZ(sndSz) |
                          TR_CTRL_RX_DATA_SZ(0));
    inl_sio2_regN_set(1, 0);

    // PIO: IOP -> SIO2
    // Fill the queue
    while (sndSz--)
        inl_sio2_data_out(*wrBufA++);

    // Start queue exec
    inl_sio2_ctrl_set(inl_sio2_ctrl_get() | 1);

    // Wait for completion
    while ((inl_sio2_stat6c_get() & (1 << 12)) == 0)
        ;
}

// static void sendCmd_Rx_PIO(uint8_t *rdBufA, int rcvSz, int portNr)
// {
//     inl_sio2_ctrl_set(0x0bc); // no interrupt

//     inl_sio2_regN_set(0,
//                       TR_CTRL_PORT_NR(portNr) |
//                           TR_CTRL_PAUSE(0) |
//                           TR_CTRL_TX_MODE_PIO_DMA(0) |
//                           TR_CTRL_RX_MODE_PIO_DMA(0) |
//                           TR_CTRL_NORMAL_TR(1) |
//                           TR_CTRL_SPECIAL_TR(0) |
//                           TR_CTRL_BAUD_DIV(fastDiv) |
//                           TR_CTRL_WAIT_ACK_FOREVER(0) |
//                           TR_CTRL_TX_DATA_SZ(0) |
//                           TR_CTRL_RX_DATA_SZ(rcvSz));
//     inl_sio2_regN_set(1, 0);

//     // Start queue exec
//     inl_sio2_ctrl_set(inl_sio2_ctrl_get() | 1);

//     // Wait for completion
//     while ((inl_sio2_stat6c_get() & (1 << 12)) == 0)
//         ;

//     // PIO: IOP <- SIO2
//     // Empty the queue
//     while (rcvSz--)
//         *rdBufA++ = reverseByte_LUT8(inl_sio2_data_in());
// }

int sio2_intr_handler(void *arg)
{
    int ef     = *(int *)arg;
    int eflags = EF_SIO2_INTR_REVERSE;

    // Clear interrupt?
    inl_sio2_stat_set(inl_sio2_stat_get());

    // Wait for completion
    while ((inl_sio2_stat6c_get() & (1 << 12)) == 0)
        ;

        // Finish sector read, read 2 crc bytes
#ifdef CONFIG_USE_CRC16
    cmd.crc[cmd.sectors_transferred] = reverseByte_LUT8(inl_sio2_data_in()) << 8;
    cmd.crc[cmd.sectors_transferred] |= reverseByte_LUT8(inl_sio2_data_in());
#else
    inl_sio2_data_in();
    inl_sio2_data_in();
#endif
    cmd.sectors_transferred++;

    if ((cmd.abort == 0) && (cmd.sectors_transferred < cmd.sector_count)) {
        // Start next DMA transfer ASAP
        cmd.response = wait_equal(0xFE, 200, cmd.portNr);
        if (cmd.response == SPISD_RESULT_OK)
            sendCmd_Rx_DMA_start(&cmd.buffer[cmd.sectors_transferred * SECTOR_SIZE], cmd.portNr);
    }

    if ((cmd.abort == 1) || (cmd.sectors_transferred >= cmd.sector_count) || (cmd.response != SPISD_RESULT_OK)) {
        // Done or error, notify user task
        eflags |= EF_SIO2_INTR_COMPLETE;
    }

    iSetEventFlag(ef, eflags);

    return 1;
}

static void _init_td(sio2_transfer_data_t *td, int portNr)
{
    static const uint8_t TxByte = 0xff;
    static uint8_t RxByte;
    int i;

    /*
     * Clock divider for 48MHz clock:
     * 1 = 48  MHz - Damaged data
     * 2 = 24  MHz - Fastest usable speed
     * 3 = 16  MHz
     * 4 = 12  MHz
     * 5 =  9.6MHz
     * 6 =  6  MHz
     * ...
     * 0x78 = 400KHz - Initialization speed
     */
    const int slowDivVal   = 0x78;
    const int fastDivVal   = 0x2;
    const int interBytePer = 0; // 2;

    for (i = 0; i < 4; i++) {
        td->port_ctrl1[i] = 0;
        td->port_ctrl2[i] = 0;
    }

    td->port_ctrl1[portNr] =
        PCTRL0_ATT_LOW_PER(0x5) |
        PCTRL0_ATT_MIN_HIGH_PER(0x5) |
        PCTRL0_BAUD0_DIV(slowDivVal) |
        PCTRL0_BAUD1_DIV(fastDivVal);

    td->port_ctrl2[portNr] =
        PCTRL1_ACK_TIMEOUT_PER(0x12C) |
        PCTRL1_INTER_BYTE_PER(interBytePer) |
        PCTRL1_UNK24(0) |
        PCTRL1_IF_MODE_SPI_DIFF(0);

    // create dummy transfer to unlock old rom0:SIO2MAN
    // - Tx 1 byte PIO
    // - Rx 1 byte PIO
    td->regdata[0] =
        TR_CTRL_PORT_NR(portNr) |
        TR_CTRL_PAUSE(0) |
        TR_CTRL_TX_MODE_PIO_DMA(0) |
        TR_CTRL_RX_MODE_PIO_DMA(0) |
        TR_CTRL_NORMAL_TR(1) |
        TR_CTRL_SPECIAL_TR(0) |
        TR_CTRL_TX_DATA_SZ(1) |
        TR_CTRL_RX_DATA_SZ(1) |
        TR_CTRL_BAUD_DIV(fastDiv) |
        TR_CTRL_WAIT_ACK_FOREVER(0);
    td->regdata[1] = 0;

    // Tx 1 byte PIO
    td->in_size      = 1;
    td->in           = (void *)&TxByte;
    td->in_dma.addr  = NULL;
    td->in_dma.size  = 0;
    td->in_dma.count = 0;

    // Rx 1 byte PIO
    td->out_size      = 1;
    td->out           = &RxByte;
    td->out_dma.addr  = NULL;
    td->out_dma.size  = 0;
    td->out_dma.count = 0;
}

static void _init_ports(sio2_transfer_data_t *td)
{
    // int i;
    // Do we need to lock all ports here?
    // Try locking the port we're interested in
    inl_sio2_portN_ctrl1_set(PORT_NR, td->port_ctrl1[PORT_NR]);
    inl_sio2_portN_ctrl2_set(PORT_NR, td->port_ctrl2[PORT_NR]);

    // for (i = 0; i < 4; i++) {
    //     inl_sio2_portN_ctrl1_set(i, td->port_ctrl1[i]);
    //     inl_sio2_portN_ctrl2_set(i, td->port_ctrl2[i]);
    // }
}

static void sio2_lock()
{
    int state;

    // M_DEBUG("%s()\n", __FUNCTION__);

    // Lock sio2man driver so we can use it exclusively
    sio2man_hook_sio2_lock();

    // Save ctrl state
    sio2man_save_crtl = inl_sio2_ctrl_get();

    // We're in control, setup the ports for our use
    _init_ports(&global_td);

    // Enable DMA interrupts
    CpuSuspendIntr(&state);
    RegisterIntrHandler(IOP_IRQ_DMA_SIO2_OUT, 1, sio2_intr_handler, &event_flag);
    EnableIntr(IOP_IRQ_DMA_SIO2_OUT);
    CpuResumeIntr(state);
}

static void sio2_unlock()
{
    int state;
    int res;

    // Disable DMA interrupts
    CpuSuspendIntr(&state);
    DisableIntr(IOP_IRQ_DMA_SIO2_OUT, &res);
    CpuResumeIntr(state);

    // Restore ctrl state, and reset STATE + FIFOS
    inl_sio2_ctrl_set(sio2man_save_crtl | 0xc);

    // Unlock sio2man driver
    sio2man_hook_sio2_unlock();

    // M_DEBUG("%s()\n", __FUNCTION__);
}

int sendCommandWithBuffer(uint8_t command, uint8_t *data, int size) {
    sio2_lock();

    switch (command)
    {
    case MCP_PING:
        uint8_t ping[6] = { 0x8B, 0x20, 0x00, 0x00, 0x00, 0x00 };
        sendCmd_Tx_PIO(ping, 6, PORT_NR);
        break;
    case MCP_SET_GAMEID:
        uint8_t gameid_header[3] = { 0x8B, 0x21, 0x00 };
        uint8_t *final_command = (uint8_t *)malloc((size + 3) * sizeof(uint8_t));
        memcpy(final_command, gameid_header, 3);
        memcpy(final_command + 3, data, size);
        
        sendCmd_Tx_PIO(final_command, (size + 3), PORT_NR);

        free(final_command);
    
    default:
        break;
    }

    sio2_unlock();

    return -1;
}

int module_start(int argc, char *argv[])
{
    if (RegisterLibraryEntries(&_exp_mcp2sio) != 0)
        return MODULE_NO_RESIDENT_END;

    iop_event_t event;
    int rv;

    DPRINTF("Initialising TD on Port %i\n", PORT_NR);
    _init_td(&global_td, PORT_NR);

    // Create event flag
    event.attr   = 2;
    event.option = 0;
    event.bits   = 0;
    rv = event_flag = CreateEventFlag(&event);
    if (rv < 0) {
        DPRINTF("ERROR: CreateEventFlag returned %d\n", rv);
        goto error1;
    }
    DPRINTF("EventFlag Created\n");

    rv = sio2man_hook_init();
    if (rv < 0) {
        DPRINTF("ERROR: sio2man_hook_init returned %d\n", rv);
        goto error2;
    }
    DPRINTF("sio2man hooked\n");

    dmac_ch_set_dpcr(IOP_DMAC_SIO2in, 3);
    dmac_ch_set_dpcr(IOP_DMAC_SIO2out, 3);
    dmac_enable(IOP_DMAC_SIO2in);
    dmac_enable(IOP_DMAC_SIO2out);

    sendCommandWithBuffer(MCP_PING, NULL, 0);

    DPRINTF("Init Done\n");

    return MODULE_RESIDENT_END;

error2:
    DeleteEventFlag(event_flag);
error1:
    return MODULE_NO_RESIDENT_END;
}

int module_stop(int argc, char *argv[])
{
    int i;

    DPRINTF("Stopping module\n");
    for (i = 0; i < argc; i++)
        DPRINTF(" - argv[%d] = %s\n", i, argv[i]);

    return MODULE_NO_RESIDENT_END;
}

// This is a bit like a "main" for IRX files.
int _start(int argc, char *argv[])
{
    DPRINTF(WELCOME_STR);

    if (argc >= 0)
        return module_start(argc, argv);
    else
        return module_stop(argc, argv);
}
