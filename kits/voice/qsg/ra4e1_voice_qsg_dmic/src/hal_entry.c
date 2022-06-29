#include "hal_data.h"

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

#define DMIC_BUF_SIZE   (8000)

static uint32_t dmic_buf[2][DMIC_BUF_SIZE];
static volatile uint8_t dmic_idx;

static volatile bool dmic_done;
static volatile bool dmic_err;

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

    /* Initialize timer used to generate I2S BLCK signal */
    err = R_GPT_Open(&g_timer_bclk_ctrl, &g_timer_bclk_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Initialize timer used to generate I2S WS signal */
    err = R_GPT_Open(&g_timer_ws_ctrl, &g_timer_ws_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Set initial counter value before the cycle start for SPI to register falling edge */
    err = R_GPT_CounterSet(&g_timer_ws_ctrl, g_timer_ws_cfg.period_counts - 1);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Enable the I2S WS timer (counting will only start after I2S BLCK is enabled) */
    err = R_GPT_Start(&g_timer_ws_ctrl);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Initialize SPI perpiheral used to receive I2S data */
    err = R_SPI_Open(&g_spi0_ctrl, &g_spi0_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Start the I2S BCLK clock */
    err = R_GPT_Start(&g_timer_bclk_ctrl);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Set up the initial I2S read */
    err = R_SPI_Read(&g_spi0_ctrl, dmic_buf[dmic_idx], DMIC_BUF_SIZE, SPI_BIT_WIDTH_32_BITS);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    while (1)
    {
        /* Wait for interrupt & check for event */
        while ((false == dmic_done) && (false == dmic_err))
            __WFI();

        if (true == dmic_err)
        {
            dmic_err = false;

            /* Restart SPI peripheral to clear the underrun error state */
            R_SPI_Close(&g_spi0_ctrl);
            R_SPI_Open(&g_spi0_ctrl, &g_spi0_cfg);
            do
            {
                /* Repeat this request if it fails */
                err = R_SPI_Read(&g_spi0_ctrl, dmic_buf, DMIC_BUF_SIZE, SPI_BIT_WIDTH_32_BITS);
            }
            while (FSP_SUCCESS != err);
        }

        else // (true == dmic_done)
        {
            dmic_done = false;

            /* Trim and align data down to 16-bit */
            for (int i = 0; i < DMIC_BUF_SIZE; i++)
            {
                dmic_buf[dmic_idx ^ 1][i] = (dmic_buf[dmic_idx ^ 1][i] >> 15) & 0xFFFF;
            }

            /** Data in dmic_buf[dmic_idx ^ 1] can be used at this point */

            /* Toggle blue LED to indicate buffer received */
            bsp_io_level_t level;
            R_IOPORT_PinRead(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_12, &level);
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_12, !level);
        }
    }
}

void g_spi0_cb(spi_callback_args_t * p_args)
{
    if (SPI_EVENT_TRANSFER_COMPLETE == p_args->event)
    {
        /* Change index of the active write buffer */
        dmic_idx ^= 1;

        /* Start subsequent I2S read */
        R_SPI_Read(&g_spi0_ctrl, dmic_buf[dmic_idx], DMIC_BUF_SIZE, SPI_BIT_WIDTH_32_BITS);

        dmic_done = true;
    }

    else if (SPI_EVENT_ERR_MODE_UNDERRUN == p_args->event)
    {
        /* SPI peripheral wasn't ready when data was sent */
        dmic_err = true;
    }

    else
    {}
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
