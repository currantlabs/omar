/* Copyright (c) 2015 Currant Inc. All Rights Reserved.
 *
 * adi_spi.c - contains Analog Devices SPI related routines
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/spi_master.h"


#include "adi_spi.h"
#include "utils.h"
#include "hw_setup.h"

static spi_device_handle_t m_spi_master;



/*
 * This example uses only one instance of the SPI master.
 * Please make sure that only one instance of the SPI master is enabled in config file.
 */

#define APP_TIMER_PRESCALER      0                      /**< Value of the RTC1 PRESCALER register. */

#if defined    (ADE7953_INTERRUPT_SUPPORT)
//we're wiring the ADE7953 IRQ (Pin 10 on the P7 of the eval board) to P0.24
//active low, so we will set it up as HITOLO
#define ADE7953_IRQ 24
#endif      // (ADE7953_INTERRUPT_SUPPORT)

//all spi transactions have 8, 16, 24, or 32 bit payloads
#define SPI_1BYTE_TRANSACTION_LEN (4)
#define SPI_2BYTE_TRANSACTION_LEN (5)
#define SPI_3BYTE_TRANSACTION_LEN (6)
#define SPI_4BYTE_TRANSACTION_LEN (7)

//read/write byte ()
#define SPI_READ  (0x80)
#define SPI_WRITE (0x00)

#define RO       (3)
#define RW_1BYTE (4)
#define RW_2BYTE (5)
#define RW_3BYTE (6)

//CONFIG Register (0x102) bits
#define INTENA     (0)
#define INTENB     (1)
#define HPFEN      (2)
#define PFMODE     (3)
#define REVP_CF    (4)
#define REVP_PULSE (5)
#define ZXLPF      (6)
#define SWRST      (7)

#define CRC_ENABLE (0)
#define CFG_RSVD0  (1)
#define CFG_RSVD1  (2)
#define ZX_I       (3)
#define ZX_EDGE1   (4)
#define ZX_EDGE2   (5)
#define CFG_RSVD2  (6)
#define COMM_LOCK  (7)

//ALT_OUTPUT Register (0x110)
#define ZX_ALT0    (0)
#define ZX_ALT1    (1)
#define ZX_ALT2    (2)
#define ZX_ALT3    (3)
#define ZXI_ALT0   (4)
#define ZXI_ALT1   (5)
#define ZXI_ALT2   (6)
#define ZXI_ALT3   (7)

#define REVP_ALT0  (0)
#define REVP_ALT1  (1)
#define REVP_ALT2  (2)
#define REVP_ALT3  (3)

//IRQENA and IRQSTATA Registers (0x22c, 0x22d)
#define AEHFA       (0)
#define VAREHFA     (1)
#define VAEHFA      (2)
#define AEOFA       (3)
#define VAREOFA     (4)
#define VAEOFA      (5)
#define AP_NOLOADA  (6)
#define VAR_NOLOADA (7)

#define VA_NOLOADA  (0)
#define APSIGN_A    (1)
#define VARSIGN_A   (2)
#define ZXTO_IA     (3)
#define ZXIA        (4)
#define OIA         (5)
#define ZXTO        (6)
#define ZXV         (7)

#define OV          (0)
#define WSMP        (1)
#define CYCEND      (2)
#define SAG         (3)
#define RESET       (4)
#define CRC         (5)

//IRQENB and IRQSTATB Registers (0x22f, 0x230)
#define AEHFB       (0)
#define VAREHFB     (1)
#define VAEHFB      (2)
#define AEOFB       (3)
#define VAREOFB     (4)
#define VAEOFB      (5)
#define AP_NOLOADB  (6)
#define VAR_NOLOADB (7)

#define VA_NOLOADB  (0)
#define APSIGN_B    (1)
#define VARSIGN_B   (2)
#define ZXTO_IB     (3)
#define ZXIB        (4)
#define OIB         (5)

//LCYCMODE Register bit offsets
#define ALWATT      (0)
#define BLWATT      (1)
#define ALVAR       (2)
#define BLVAR       (3)
#define ALVA        (4)
#define BLVA        (5)
#define RSTREAD     (6)

//see page 18 of the AD7953 data sheet
#define REGISTER_0X120_KEY    (0xAD)
#define REGISTER_0X120_CONFIG (0x30)

//8-bit registers
static uint8_t m_unlock_pkt[RW_1BYTE] =     {0x00, 0xfe};
static uint8_t m_optimum_pkt[RW_1BYTE] =    {0x01, 0x20};

static uint8_t m_lcycmode_pkt[RW_1BYTE] =   {0x00, 0x04};
static uint8_t m_pga_v_pkt[RW_1BYTE] =      {0x00, 0x07};
static uint8_t m_pga_ia_pkt[RW_1BYTE] =     {0x00, 0x08};
static uint8_t m_pga_ib_pkt[RW_1BYTE] =     {0x00, 0x09};

static uint8_t m_last_op_pkt[RO] =          {0x00, 0xfd};
static uint8_t m_last_rwdata8_pkt[RO] =     {0x00, 0xff};

//16-bit registers
static uint8_t m_linecyc_pkt[RW_2BYTE] =    {0x01, 0x01};
static uint8_t m_config_pkt[RW_2BYTE] =     {0x01, 0x02};
static uint8_t m_alt_output_pkt[RW_2BYTE] = {0x01, 0x10};
static uint8_t m_last_add_pkt[RO] =         {0x01, 0xfe};
static uint8_t m_last_rwdata16_pkt[RO] =    {0x01, 0xff};

//24-bit registers
static uint8_t m_last_rwdata24_pkt[RO] =    {0x02, 0xff};
static uint8_t m_awatt_pkt[RO] =            {0x02, 0x12};
static uint8_t m_bwatt_pkt[RO] =            {0x02, 0x13};
static uint8_t m_avar_pkt[RO] =             {0x02, 0x14};
static uint8_t m_bvar_pkt[RO] =             {0x02, 0x15};
static uint8_t m_ava_pkt[RO] =              {0x02, 0x10};
static uint8_t m_bva_pkt[RO] =              {0x02, 0x11};
static uint8_t m_ia_pkt[RO] =               {0x02, 0x16};
static uint8_t m_ib_pkt[RO] =               {0x02, 0x17};
static uint8_t m_v_pkt[RO] =                {0x02, 0x18};

static uint8_t m_irmsa_pkt[RO] =            {0x02, 0x1a};
static uint8_t m_irmsb_pkt[RO] =            {0x02, 0x1b};
static uint8_t m_vrms_pkt[RO] =             {0x02, 0x1c};

static uint8_t m_aenergya_pkt[RO] =         {0x02, 0x1e};
static uint8_t m_aenergyb_pkt[RO] =         {0x02, 0x1f};
static uint8_t m_renergya_pkt[RO] =         {0x02, 0x20};
static uint8_t m_renergyb_pkt[RO] =         {0x02, 0x21};
static uint8_t m_apenergya_pkt[RO] =        {0x02, 0x22};
static uint8_t m_apenergyb_pkt[RO] =        {0x02, 0x23};

static uint8_t m_aigain_pkt[RW_3BYTE] =     {0x02, 0x80};
static uint8_t m_avgain_pkt[RW_3BYTE] =     {0x02, 0x81};
static uint8_t m_awgain_pkt[RW_3BYTE] =     {0x02, 0x82};
static uint8_t m_avargain_pkt[RW_3BYTE] =   {0x02, 0x83};
static uint8_t m_avagain_pkt[RW_3BYTE] =    {0x02, 0x84};
static uint8_t m_airmsos_pkt[RW_3BYTE] =    {0x02, 0x86};
static uint8_t m_vrmsos_pkt[RW_3BYTE] =     {0x02, 0x88};
static uint8_t m_awattos_pkt[RW_3BYTE] =    {0x02, 0x89};
static uint8_t m_avaros_pkt[RW_3BYTE] =     {0x02, 0x8a};
static uint8_t m_avaos_pkt[RW_3BYTE] =      {0x02, 0x8b};

static uint8_t m_bigain_pkt[RW_3BYTE] =     {0x02, 0x8c};
static uint8_t m_bvgain_pkt[RW_3BYTE] =     {0x02, 0x8d};
static uint8_t m_bwgain_pkt[RW_3BYTE] =     {0x02, 0x8e};
static uint8_t m_bvargain_pkt[RW_3BYTE] =   {0x02, 0x8f};
static uint8_t m_bvagain_pkt[RW_3BYTE] =    {0x02, 0x90};
static uint8_t m_birmsos_pkt[RW_3BYTE] =    {0x02, 0x92};
static uint8_t m_bwattos_pkt[RW_3BYTE] =    {0x02, 0x95};
static uint8_t m_bvaros_pkt[RW_3BYTE] =     {0x02, 0x96};
static uint8_t m_bvaos_pkt[RW_3BYTE] =      {0x02, 0x97};

static uint8_t m_vpeak_pkt[RO] =            {0x02, 0x26};
static uint8_t m_rstvpeak_pkt[RO] =         {0x02, 0x27};

static uint8_t m_irqena_pkt[RW_3BYTE] =     {0x02, 0x2c};

static uint8_t m_irqstata_pkt[RO] =         {0x02, 0x2d};
static uint8_t m_rstirqstata_pkt[RO] =      {0x02, 0x2e};

static uint8_t m_irqenb_pkt[RW_3BYTE] =     {0x02, 0x2f};
static uint8_t m_irqstatb_pkt[RO] =         {0x02, 0x30};
static uint8_t m_rstirqstatb_pkt[RO] =      {0x02, 0x31};

//32-bit registers
//NOTE: we can save 1 byte of the SPI transaction by reading the 24-bit
//values and sign-extending them into 32-bit values.  TODO as an optimization.
static uint8_t m_last_rwdata32_pkt[RO] =    {0x03, 0xff};
static uint8_t m_ap_noload_pkt[RO] =        {0x03, 0x03};

//receive buffer
static uint8_t m_rx_data[SPI_4BYTE_TRANSACTION_LEN] = {0}; /* make this the largest transaction size */

static int test(void);

typedef struct {
    SpiCmdNameT name;
    char *name_str;
    uint8_t *pkt;
    uint8_t pkt_size;
} SpiCmdT;

//NOTE: the order here must match the order of commands defined in SpiCmdNameT or we'll assert.
static SpiCmdT m_spi_commands[] = {
    {UNLOCK, "UNLOCK", m_unlock_pkt, SPI_1BYTE_TRANSACTION_LEN},
    {OPTIMUM_SETTING, "OPTIMUM_SETTING", m_optimum_pkt, SPI_1BYTE_TRANSACTION_LEN},

    {LCYCMODE, "LCYCMODE", m_lcycmode_pkt, SPI_1BYTE_TRANSACTION_LEN},
    {PGA_V, "PGA_V", m_pga_v_pkt, SPI_1BYTE_TRANSACTION_LEN},
    {PGA_IA, "PGA_IA", m_pga_ia_pkt, SPI_1BYTE_TRANSACTION_LEN},
    {PGA_IB, "PGA_IB", m_pga_ib_pkt, SPI_1BYTE_TRANSACTION_LEN},

    {LAST_OP, "LAST_OP", m_last_op_pkt, SPI_1BYTE_TRANSACTION_LEN},

    {LAST_RWDATA_8, "LAST_RWDATA_8", m_last_rwdata8_pkt, SPI_1BYTE_TRANSACTION_LEN},
    {LAST_RWDATA_16, "LAST_RWDATA_16", m_last_rwdata16_pkt, SPI_2BYTE_TRANSACTION_LEN},
    {LAST_RWDATA_24, "LAST_RWDATA_24", m_last_rwdata24_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {LAST_RWDATA_32, "LAST_RWDATA_32", m_last_rwdata32_pkt, SPI_4BYTE_TRANSACTION_LEN},
    {LAST_ADD, "LAST_ADD", m_last_add_pkt, SPI_2BYTE_TRANSACTION_LEN},

    {LINECYC, "LINECYC", m_linecyc_pkt, SPI_2BYTE_TRANSACTION_LEN},
    {CONFIG, "CONFIG", m_config_pkt, SPI_2BYTE_TRANSACTION_LEN},
    {ALT_OUTPUT, "ALT_OUTPUT", m_alt_output_pkt, SPI_2BYTE_TRANSACTION_LEN},
    {IRQENA, "IRQENA", m_irqena_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {IRQSTATA, "IRQSTATA", m_irqstata_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {RSTIRQSTATA, "RSTIRQSTATA", m_rstirqstata_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {IRQENB, "IRQENB", m_irqenb_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {IRQSTATB, "IRQSTATB", m_irqstatb_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {RSTIRQSTATB, "RSTIRQSTATB", m_rstirqstatb_pkt, SPI_3BYTE_TRANSACTION_LEN},

    //instantaneous registers for waveform sampling
    {AWATT, "AWATT", m_awatt_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BWATT, "BWATT", m_bwatt_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AVAR, "AVAR", m_avar_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BVAR, "BVAR", m_bvar_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AVA, "AVA", m_ava_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BVA, "BVA", m_bva_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {IA, "IA", m_ia_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {IB, "IB", m_ib_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {V, "V", m_v_pkt, SPI_3BYTE_TRANSACTION_LEN},

    {IRMSA, "IRMSA", m_irmsa_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {IRMSB, "IRMSB", m_irmsb_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {VRMS, "VRMS", m_vrms_pkt, SPI_3BYTE_TRANSACTION_LEN},

    {AENERGYA, "AENERGYA", m_aenergya_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AENERGYB, "AENERGYB", m_aenergyb_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {RENERGYA, "RENERGYA", m_renergya_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {RENERGYB, "RENERGYB", m_renergyb_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {APENERGYA, "APENERGYA", m_apenergya_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {APENERGYB, "APENERGYB", m_apenergyb_pkt, SPI_3BYTE_TRANSACTION_LEN},

    {AIGAIN, "AIGAIN", m_aigain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AVGAIN, "AVGAIN", m_avgain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AWGAIN, "AWGAIN", m_awgain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AVARGAIN, "AVARGAIN", m_avargain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AVAGAIN, "AVAGAIN", m_avagain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AIRMSOS, "AIRMSOS", m_airmsos_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {VRMSOS, "VRMSOS", m_vrmsos_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AWATTOS, "AWATTOS", m_awattos_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AVAROS, "AVAROS", m_avaros_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {AVAOS, "AVAOS", m_avaos_pkt, SPI_3BYTE_TRANSACTION_LEN},

    {BIGAIN, "BIGAIN", m_bigain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BVGAIN, "BVGAIN", m_bvgain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BWGAIN, "BWGAIN", m_bwgain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BVARGAIN, "BVARGAIN", m_bvargain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BVAGAIN, "BVAGAIN", m_bvagain_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BIRMSOS, "BIRMSOS", m_birmsos_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BWATTOS, "BWATTOS", m_bwattos_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BVAROS, "BVAROS", m_bvaros_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {BVAOS, "BVAOS", m_bvaos_pkt, SPI_3BYTE_TRANSACTION_LEN},

    {VPEAK, "VPEAK", m_vpeak_pkt, SPI_3BYTE_TRANSACTION_LEN},
    {RSTVPEAK, "RSTVPEAK", m_rstvpeak_pkt, SPI_3BYTE_TRANSACTION_LEN},

    {AP_NOLOAD, "AP_NOLOAD", m_ap_noload_pkt, SPI_4BYTE_TRANSACTION_LEN},
};

SpiCmdT m_last_read_cmd;

#if defined    (WAVEFORM_SAMPLING_SUPPORT)
static bool m_waveform_sampling_configured = false;



//we're wiring the ADE7953 ZX (Pin 1 on the P7 of the eval board) to the Nordic.
//(needs to be re-mapped depending on HW version)
//The data ready signal goes high for a period of 280ns before automatically
//returning low, so we will set it up as LOTOHI.
static int ZX_IRQ;
#endif      // (WAVEFORM_SAMPLING_SUPPORT)

static volatile bool m_transfer_completed = true; /**< A flag to inform about completed transfer. */

/**@brief Function for SPI master event callback.
 *
 * Upon receiving an SPI transaction complete event, checks if received data are valid.
 *
 * @param[in] spi_master_evt    SPI master driver event.
 */
static void spi_master_event_handler(spi_transaction_t *cur_trans)
{
    //uint32_t err_code = NRF_SUCCESS;

    //printf("%s(): 01\n", __func__);

    if (cur_trans != NULL) {

        //printf("%s(): 02 - \n", __func__);

        // Inform application that transfer is completed.
        m_transfer_completed = true;

    }
}

/*
 * power_up_register_sequence - this sequence is apparently required for
 * "optimum performance".  See page 18 of the AD7953 spec.
 */
void adi_power_up_register_sequence(void)
{
    uint8_t buff;

    //unlock register 0x120
    buff = REGISTER_0X120_KEY;
    spi_write_reg(UNLOCK, &buff);

    //configure optimum settings
    buff = REGISTER_0X120_CONFIG;
    spi_write_reg(OPTIMUM_SETTING, &buff);
}


/*
 * enable_hpf - enables or disables the HPF (all channels)
 */
void enable_hpf(bool enable)
{
    uint8_t buff[2];
    spi_read_reg(CONFIG, buff);

    if (enable) {
        buff[0] |= 1 << HPFEN;
    } else {
        buff[0] &= ~(1 << HPFEN);
    }

    spi_write_reg(CONFIG, buff);
}

/*
 * performs a software reset by writing to bit 7 of the CONFIG register
 */
void adi_sw_reset(void)
{
    uint8_t buff[2];

    adi_dump_reg(CONFIG);

    spi_read_reg(CONFIG, buff);
    buff[1] |= 1 << SWRST;
    spi_write_reg(CONFIG, buff);
}

#if defined    (ADE7953_INTERRUPT_SUPPORT)

/*
 * enable_waveform_sampling_interrupt - enables or disables an interrupt
 * when new waveform sampling data is acquired
 */
void enable_waveform_sampling_interrupt(bool enable)
{
    uint8_t buff[3];
    spi_read_reg(IRQENA, buff);

    if (enable) {
        buff[2] |= (1 << WSMP);  //set to 1 to enable an interrupt when new waveform data is acquired
    } else {
        buff[2] &= ~(1 << WSMP);
    }

    spi_write_reg(IRQENA, buff);
}

/*
 * enable_energy_interrupts - enables or disables an interrupt
 * when active, reactive, or apparrent energy registers are either half full or have overflowed.
 */
void adi_enable_energy_interrupts(bool enable)
{
    uint8_t buff[3];
    spi_read_reg(IRQENA, buff);

    if (enable) {
        buff[2] |= (1 << AEHFA | 1 << VAREHFA | 1 << VAEHFA | 1 << AEOFA | 1 << VAREOFA | 1 << VAEOFA);
    } else {
        buff[2] &= ~(1 << AEHFA | 1 << VAREHFA | 1 << VAEHFA | 1 << AEOFA | 1 << VAREOFA | 1 << VAEOFA);
    }

    spi_write_reg(IRQENA, buff);

    spi_read_reg(IRQENB, buff);

    if (enable) {
        buff[2] |= (1 << AEHFB | 1 << VAREHFB | 1 << VAEHFB | 1 << AEOFB | 1 << VAREOFB | 1 << VAEOFB);
    } else {
        buff[2] &= ~(1 << AEHFB | 1 << VAREHFB | 1 << VAEHFB | 1 << AEOFB | 1 << VAREOFB | 1 << VAEOFB);
    }

    spi_write_reg(IRQENB, buff);

}

#endif      // (ADE7953_INTERRUPT_SUPPORT)


/*
 * enable_lca_mode - enable or disable active energy line cycle accumulation mode on
 *                   current channels A and B.  Also enables read with reset for all
 *                   registers.  This is useful for determining the Wh/LSB constant.
 */
void enable_active_lca_mode(bool enable, uint16_t cycles)
{
    if (enable) {
        uint8_t buff[2];
        buff[0] = cycles >> 8;
        buff[1] = cycles & 0xff;
        spi_write_reg(LINECYC, buff);

        //enable Line Cycle accumulation mode, and reset on read
        uint8_t mode = ((1 << RSTREAD) | (1 << ALWATT) | (1 << BLWATT));
        spi_write_reg(LCYCMODE, &mode);

    } else {
        //disable Line Cycle accumulation mode
        uint8_t mode = 0;
        spi_write_reg(LCYCMODE, &mode);
    }
}

/*
 * set gain register to 'gain'
 */
void set_gain(SpiCmdNameT reg, uint32_t gain)
{
    uint8_t buff[3];
    buff[0] = (gain >> 16) & 0xff;
    buff[1] = (gain >> 8) & 0xff;
    buff[2] = gain & 0xff;

    spi_write_reg(reg, buff);
    ESP_LOGI(__func__, "set %s to 0x%08x\r\n", get_reg_name(reg), gain);
}

#if defined    (WAVEFORM_SAMPLING_SUPPORT)
/*
 * writes to ALT_OUTPUT register to set the unlatched waveform sampling signal
 * to output on Pin 1 (ZX pin)
 */
void waveform_sampling_pin_config(bool enable)
{

    if (enable) {
        uint8_t buff[2];
        spi_read_reg(ALT_OUTPUT, buff);
        buff[1] &= 0xF0;  //lower 4-bits are ZX_ALT
        buff[1] |= 0x09;  //Unlatched waveform sampling signal is output on Pin 1
        spi_write_reg(ALT_OUTPUT, buff);
        m_waveform_sampling_configured = true;
    } else {
        irq_pin_config();
        m_waveform_sampling_configured = false;
    }
}

#endif      // (WAVEFORM_SAMPLING_SUPPORT)


#if defined    (ADE7953_INTERRUPT_SUPPORT)

/*
 * writes to ALT_OUTPUT register to set IRQ (for Channel B)
 * to output on Pin 1 (ZX pin)
 */
void irq_pin_config(void)
{
    uint8_t buff[2];
    spi_read_reg(ALT_OUTPUT, buff);

    buff[1] &= 0xF0;  //lower 4-bits are ZX_ALT
    buff[1] |= 0x0A;  //IRQ (for Channel B) is output on Pin 1
    spi_write_reg(ALT_OUTPUT, buff);
}

static void handle_7953_irq(void * p_event_data, uint16_t event_size)
{
    uint8_t buff[3];
    spi_read_reg(RSTIRQSTATA, buff);
    hexdump_bytes(buff, 3);

    if (buff[2] & ((1 << AEHFA) | (1 << AEOFA))) {
        poll_outlet(OUTLET_B, true);  //force an advertisement with new energy
    }

    spi_read_reg(RSTIRQSTATB, buff);
    if (buff[2] & ((1 << AEHFB) | (1 << AEOFB))) {
        poll_outlet(OUTLET_A, true);  //force an advertisement with new energy
    }
}

void irq_a_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    app_sched_event_put(NULL, 0, handle_7953_irq);
}

void irq_b_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    if (m_waveform_sampling_configured) {
        static uint8_t count;
        if ((count++ % dataready_divisor) == 0) dataready_signal_set = true;
    } else {
        app_sched_event_put(NULL, 0, handle_7953_irq);
    }
}

#endif      // (ADE7953_INTERRUPT_SUPPORT)


void adi_spi_init(void)
{
    //First, hardware reset the ADI7953:
    adi_hw_reset();

    // Next, configure the SPI bus:
    adi_spi_setup();


}

#ifdef REPEAT_SUCCESS_OF_COMMIT_80c69d9
//copy the kind of call I see succeeding in the esp_idf LCD samle:
/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}
#endif //REPEAT_SUCCESS_OF_COMMIT_80c69d9

void lcd_get_id(void)
{

	// This is the code that gave me a decent-looking
	// spi signal capture on the Saleae: git commit 80c69d9:
#ifdef REPEAT_SUCCESS_OF_COMMIT_80c69d9
	
    //get_id cmd
    lcd_cmd(m_spi_master, 0x04);

#endif //REPEAT_SUCCESS_OF_COMMIT_80c69d9


	// Now I'm trying the same approach to implement
	// the guts of the proposition adi_spi.c routine:
	//
	// test()
	//
	// Here we go:

    esp_err_t ret;
    spi_transaction_t t;
	uint8_t cmd[] = {0x01, 0x02, 0x80};
	uint8_t miso[5] = {0};

    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=40;                     //Command is 24 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.rx_buffer=miso;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_polling_transmit(m_spi_master, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.

    printf("CONFIG REGISTER: {0x%02x, 0x%02x}\n", miso[3], miso[4]);

	if (miso[3] == 0x80 && miso[4] == 0x04) {
		printf("[OK]\n");
	} else {
		printf("[FAILED]\n");
	}
}

//called after a hardware reset, to reinitialize chip to a known state
int adi_spi_reinit(void)
{
    vTaskDelay(100 / portTICK_RATE_MS);
    
#if defined    (ADE7953_INTERRUPT_SUPPORT)
    irq_pin_config();
#endif      // (ADE7953_INTERRUPT_SUPPORT)

    //reset ADE7953 interrupts
    uint8_t buff[3];
    spi_read_reg(RSTIRQSTATA, buff);


#ifdef NOWAY
    spi_read_reg(RSTIRQSTATB, buff);

    //initialize 7953
    adi_power_up_register_sequence();
#if defined    (ADE7953_INTERRUPT_SUPPORT)
    adi_enable_energy_interrupts(true);
#endif      // (ADE7953_INTERRUPT_SUPPORT)

#if defined    (ADE7953_CALIBRATION_SUPPORT)
    //re-set calibration constants
    calibration_init();
#endif      // (ADE7953_CALIBRATION_SUPPORT)

#endif // NOWAY


    return 0;
}

int set_pgagain(uint8_t gain)
{
    switch(gain) {
    case 1:
        gain = 0;
        break;
    case 2:
        gain = 1;
        break;
    case 4:
        gain = 2;
        break;
    case 8:
        gain = 3;
        break;
    case 16:
        gain = 4;
        break;
    case 22:
        gain = 5;
        break;
    default:
        ESP_LOGI(__func__, "invalid PGA_GAIN!");
        return -1;
    }
    spi_write_reg(PGA_IA, &gain);  //set default gain
    spi_write_reg(PGA_IB, &gain);  //set default gain
    ESP_LOGI(__func__, "set gain to %d", gain);


    return 0;
}

static esp_err_t local_spi_device_polling_transmit(spi_device_handle_t handle, spi_transaction_t* trans_desc)
{
    esp_err_t ret;
    ret = spi_device_polling_start(handle, trans_desc, portMAX_DELAY);
    if (ret != ESP_OK) return ret;
    ret = spi_device_polling_end(handle, portMAX_DELAY);
    if (ret != ESP_OK) return ret;
    printf("%s(): 01\n", __func__);

    return ESP_OK;
}
	

static void spi_send_recv(SpiCmdNameT send_cmd, uint8_t *data)
{

    SpiCmdT cmd = m_spi_commands[send_cmd];

    uint16_t len = cmd.pkt_size;
    uint8_t *p_tx_data = cmd.pkt;

    esp_err_t ret;
    spi_transaction_t t;

    bool spi_read = false;

    memset(&t, 0, sizeof(t));       //Zero out the transaction

    printf("%s(): 01\n", __func__);

    m_transfer_completed = false;

    if (data != NULL) {
        printf("%s(): 02\n", __func__);

        //this is a write command - copy data to write into buffer
        p_tx_data[2] = SPI_WRITE;
        memcpy(p_tx_data + 3, data, len - 3);
    } else {
        printf("%s(): 03\n", __func__);


        //this is a read command - data tx buffer doesn't matter
        m_last_read_cmd = cmd;
        p_tx_data[2] = SPI_READ;

        spi_read = true;
        printf("%s(): m_rx_data before read:\n", __func__);
        hexdump_bytes(m_rx_data, SPI_4BYTE_TRANSACTION_LEN);
        t.rx_buffer=m_rx_data;


    }

    printf("%s(): 04\n", __func__);


    t.length=len;                                       //Command length
    t.tx_buffer=p_tx_data;                              //The data is the cmd itself
    t.user=(void*)0;                                    //D/C needs to be set to 0
    printf("%s(): 05\n", __func__);


    ret=local_spi_device_polling_transmit(m_spi_master, &t);  //Transmit!

    printf("%s(): 06, spi_device_polling_transmit() retval is 0x%02x\n", __func__, ret);

    if (spi_read) {
        printf("%s(): m_rx_data after read:\n", __func__);
        hexdump_bytes(m_rx_data, SPI_4BYTE_TRANSACTION_LEN);
    }


    assert(ret==ESP_OK);                                //Should have had no issues.
    

#ifdef NOWAY

    SpiCmdT cmd = m_spi_commands[send_cmd];

    uint16_t len = cmd.pkt_size;
    uint8_t *p_tx_data = cmd.pkt;

    memset(m_rx_data, 0, sizeof(m_rx_data));
    m_transfer_completed = false;

    if (data != NULL) {
        //this is a write command - copy data to write into buffer
        p_tx_data[2] = SPI_WRITE;
        memcpy(p_tx_data + 3, data, len - 3);
    } else {
        //this is a read command - data tx buffer doesn't matter
        m_last_read_cmd = cmd;
        p_tx_data[2] = SPI_READ;
    }

    uint32_t err_code;
    bool failed = false;
    uint32_t t;
    utils_timer_cnt_get(&t);
    //printf("t=%u\r\n", (unsigned)t);

    do {
        err_code = nrf_drv_spi_transfer(&m_spi_master, p_tx_data, len, m_rx_data, len);
        //utils_timer_cnt_diff(t);
        if (err_code != NRF_SUCCESS) {
            failed = true;
        }
    } while (err_code != NRF_SUCCESS);
    if (failed) {
        ESP_LOGI(__func__, "adi_spi nrf_drv_spi_transfer failed: ");
        utils_timer_cnt_diff(t);
    }
    //APP_ERROR_CHECK(err_code);
#endif // NOWAY

}

/*
 * spi_read_reg - read data from specified register into provided buff.
 * Returns number of bytes read.
 *
 * Busy spins until data is read.
 *
 * NOTE: buff must be large enough for register being read;
 * i.e., 4 bytes, for 32-bit registers.
 */
uint32_t spi_read_reg(SpiCmdNameT reg, uint8_t *buff)
{

    printf("%s(): 01\n", __func__);

    //TESTING!!
    if (!m_transfer_completed) {
		printf("%s(): 02\n", __func__);
        return -1;
    }
    printf("%s(): 03\n", __func__);
    spi_send_recv(reg, NULL);
    while (!m_transfer_completed) {};

    printf("%s(): 04\n", __func__);
    memcpy(buff, m_rx_data + 3, m_last_read_cmd.pkt_size - 3);
    printf("%s(): 05\n", __func__);
    return m_last_read_cmd.pkt_size - 3;
}

/*
 * spi_write_reg - writes data into the specified register.
 *
 * Busy spins until data is written.
 */
void spi_write_reg(SpiCmdNameT reg, uint8_t *buff)
{
    spi_send_recv(reg, buff);
    while (!m_transfer_completed) {};
}


char *get_reg_name(SpiCmdNameT reg)
{
    SpiCmdT cmd = m_spi_commands[reg];
    return cmd.name_str;
}


static int test(void)
{
    uint8_t buff[4];

    //test reading config
    ESP_LOGI(__func__, "CONFIG: ");
    spi_read_reg(CONFIG, buff);
    printf("%s(): 01\n", __func__);
    if (buff[0] == 0x80 && buff[1] == 0x04) {
        ESP_LOGI(__func__, "[OK]");
    } else {
        ESP_LOGI(__func__, "[FAILED]");
        return -1;
    }

    printf("%s(): 02\n", __func__);

    spi_read_reg(AP_NOLOAD, buff);

    //test writing AP_NOLOAD
#define TEST_BYTE_0 (0x00)
#define TEST_BYTE_1 (0x00)
#define TEST_BYTE_2 (0xd5)
#define TEST_BYTE_3 (0xa3)
    buff[0] = TEST_BYTE_0;
    buff[1] = TEST_BYTE_1;
    buff[2] = TEST_BYTE_2;
    buff[3] = TEST_BYTE_3;

    printf("%s(): 03\n", __func__);

    spi_write_reg(AP_NOLOAD, buff);
    memset(buff, 0, sizeof(buff));

    printf("%s(): 04\n", __func__);

    ESP_LOGI(__func__, "AP_NOLOAD: ");
    spi_read_reg(AP_NOLOAD, buff);

    printf("%s(): 05\n", __func__);

    if (buff[0] == TEST_BYTE_0 && buff[1] == TEST_BYTE_1 && buff[2] == TEST_BYTE_2 && buff[3] == TEST_BYTE_3) {
        ESP_LOGI(__func__, "[OK]");
    } else {
        ESP_LOGI(__func__, "[FAILED]");
        return -1;
    }


#if defined    (METER_SUPPORT)
    float v = meter_get_voltage();
    ESP_LOGI(__func__, "voltage (rms): %f V", v);
#else
    ESP_LOGI(__func__, "voltage (rms): %f V", 666.66);
#endif      // (METER_SUPPORT)
    
    return 0;
}


void factory_7953(void)
{
    if (test()) {
        ESP_LOGI(__func__, "FACTORY_TEST_STATUS__FAILED");
    } else {
        ESP_LOGI(__func__, "FACTORY_TEST_STATUS__PASSED");
    }   
}


void adi_hw_reset(void)
{
    gpio_set_level(ADI_RESET, false);
    vTaskDelay(10/portTICK_PERIOD_MS);
    gpio_set_level(ADI_RESET, true);

}

void adi_spi_setup(void)
{
    esp_err_t ret;
    spi_device_handle_t spi;
    spi_bus_config_t buscfg={
        .miso_io_num=OMAR_SPIM0_MISO_PIN,
        .mosi_io_num=OMAR_SPIM0_MOSI_PIN,
        .sclk_io_num=OMAR_SPIM0_SCK_PIN,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=0  // defaults to 4094 if this is 0
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=4*1000*1000,           //Clock out at 4 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=OMAR_SPIM0_SS_PIN,        //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .post_cb=spi_master_event_handler,      //Called after a spi xmission completes (called in interrupt context)
    };

    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);

    //printf("%s(): 03\n", __func__);

    //Attach the ADI7953 to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    //printf("%s(): 04\n", __func__);

    m_spi_master = spi;
    //Initialize the AD7953:
    //ad7953_init(spi); // (vjc) add this later

}



/** @} */


