#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


typedef enum {
    UNLOCK,
    OPTIMUM_SETTING,
    LCYCMODE,
    PGA_V,
    PGA_IA,
    PGA_IB,
    LAST_OP,
    LAST_RWDATA_8,
    LAST_RWDATA_16,
    LAST_RWDATA_24,
    LAST_RWDATA_32,
    LAST_ADD,

    LINECYC,
    CONFIG,
    ALT_OUTPUT,
    IRQENA,
    IRQSTATA,
    RSTIRQSTATA,
    IRQENB,
    IRQSTATB,
    RSTIRQSTATB,

    AWATT,
    BWATT,
    AVAR,
    BVAR,
    AVA,
    BVA,
    IA,
    IB,
    V,

    IRMSA,
    IRMSB,
    VRMS,

    AENERGYA,
    AENERGYB,
    RENERGYA,
    RENERGYB,
    APENERGYA,
    APENERGYB,

    AIGAIN,
    AVGAIN,
    AWGAIN,
    AVARGAIN,
    AVAGAIN,
    AIRMSOS,
    VRMSOS,
    AWATTOS,
    AVAROS,
    AVAOS,

    BIGAIN,
    BVGAIN,
    BWGAIN,
    BVARGAIN,
    BVAGAIN,
    BIRMSOS,
    BWATTOS,
    BVAROS,
    BVAOS,


    VPEAK,
    RSTVPEAK,

    AP_NOLOAD,
} SpiCmdNameT;

// Configure the SPI connection to the AD7953
void adi_spi_init(void);
void factory_7953(void);

uint32_t spi_read_reg(SpiCmdNameT reg, uint8_t *buff);
void spi_write_reg(SpiCmdNameT reg, uint8_t *buff);
char *get_reg_name(SpiCmdNameT reg);
void adi_hw_reset(void);
void adi_spi_setup(void);
int adi_spi_reinit(void);


#if defined (__OMAR_AD7953_SPI_SUPPORT_READY__)


void enable_hpf(bool enable);
void adi_sw_reset(void);
void waveform_sampling_pin_config(bool enable);
void irq_pin_config(void);

void enable_waveform_sampling_interrupt(bool enable);
void enable_active_lca_mode(bool enable, uint16_t cycles);
void set_gain(SpiCmdNameT reg, uint32_t gain);
int set_pgagain(uint8_t gain);
void adi_power_up_register_sequence(void);
void adi_enable_energy_interrupts(bool enable);


#endif      // (__OMAR_AD7953_SPI_SUPPORT_READY__)

