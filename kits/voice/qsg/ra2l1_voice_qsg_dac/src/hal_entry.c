#include "hal_data.h"

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

extern uint8_t audio_samples[130032];

static volatile bool dac_done;

void hal_entry(void)
{
    fsp_err_t err;

    /* Initialize the DAC peripheral */
    err = R_DAC_Open(&g_dac0_ctrl, &g_dac0_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Enable DAC output */
    err = R_DAC_Start(&g_dac0_ctrl);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Initialize the DTC peripheral */
    err = R_DTC_Open(&g_transfer_dac_ctrl, &g_transfer_dac_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Initialize the timer used to control the sampling rate */
    err = R_GPT_Open(&g_timer_dac_ctrl, &g_timer_dac_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Start the timer */
    err = R_GPT_Start(&g_timer_dac_ctrl);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    while (1)
    {
        /* Start playback by setting DMA to transfer audio samples to the DAC */
        err = R_DTC_Reset(&g_transfer_dac_ctrl, audio_samples, (void *) R_DAC->DADR,
                           sizeof(audio_samples) / sizeof(uint16_t));
        if (FSP_SUCCESS != err)
        {
            __BKPT(0);
        }

        /* Wait for interrupt & check for event */
        while (false == dac_done)
            __WFI();

        dac_done = false;

        /* Wait before starting the playback again */
        R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
    }
}

void g_transfer_dac_cb(timer_callback_args_t * p_args)
{
    /* Use this callback to end the playback or restart the DMA
     * with more samples to play longer tracks */

    /* Signal that last sample has been sent to DAC */
    dac_done = true;

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
