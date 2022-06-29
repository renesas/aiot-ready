#include "hal_data.h"

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

#define AMIC_BUF_SIZE   (8000)

static uint32_t amic_buf[2][AMIC_BUF_SIZE];
static volatile uint8_t amic_idx;

static volatile bool amic_done;

void hal_entry(void)
{
    fsp_err_t err;

    /* Initialize ELC peripheral */
    err = R_ELC_Open(&g_elc_ctrl, &g_elc_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Enabled configured ELC links */
    err = R_ELC_Enable(&g_elc_ctrl);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Initialize the ADC peripheral */
    err = R_ADC_Open(&g_adc0_ctrl, &g_adc0_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Enable ADC scanning on microphone channels */
    err = R_ADC_ScanCfg(&g_adc0_ctrl, &g_adc0_channel_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Enable ADC scanning */
    err = R_ADC_ScanStart(&g_adc0_ctrl);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Initialize the DMA peripheral */
    err = R_DMAC_Open(&g_transfer0_ctrl, &g_transfer0_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Set the DMA to capture from ADC registers into amic_buf */
    err = R_DMAC_Reset(&g_transfer0_ctrl, (void *) R_ADC0->ADDR, amic_buf[amic_idx], AMIC_BUF_SIZE);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Initialize timer used to limit the sampling rate */
    err = R_GPT_Open(&g_timer0_ctrl, &g_timer0_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Start the timer */
    err = R_GPT_Start(&g_timer0_ctrl);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    while (1)
    {
        /* Wait for interrupt & check for event */
        while (false == amic_done)
            __WFI();

        amic_done = false;

        /** Data in amic_buf[amic_idx ^ 1] can be used at this point */

        /* Toggle green LED to indicate buffer received */
        bsp_io_level_t level;
        R_IOPORT_PinRead(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_13, &level);
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_13, !level);
    }
}

void g_transfer0_cb(dmac_callback_args_t * p_args)
{
    /* Change index of the active write buffer */
    amic_idx ^= 1;

    /* Start subsequent ADC capture */
    R_DMAC_Reset(&g_transfer0_ctrl, (void *) R_ADC0->ADDR, amic_buf[amic_idx], AMIC_BUF_SIZE);

    amic_done = true;

    /* Suppress compiler warning for unused p_args */
    FSP_PARAMETER_NOT_USED(p_args);
}

void R_BSP_WarmStart(bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */

        /* Configure pins. */
        R_IOPORT_Open (&g_ioport_ctrl, g_ioport.p_cfg);
    }
}
