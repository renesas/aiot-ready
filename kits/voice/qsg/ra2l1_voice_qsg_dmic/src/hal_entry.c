#include "hal_data.h"

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

#define DMIC_BUF_SIZE   (2000)

static uint32_t dmic_buf[2][DMIC_BUF_SIZE];
static volatile uint8_t dmic_idx;

static volatile bool dmic_done;
static volatile bool dmic_err;

void hal_entry(void)
{
    fsp_err_t err;

    /* Open timer used for I2S BCLK output */
    err = R_GPT_Open(&g_timer_sck_ctrl, &g_timer_sck_cfg);
    if(FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    err = R_POEG_Open(&g_poeg_ws_ctrl, &g_poeg_ws_cfg);
    if (FSP_SUCCESS != err)
    {
       __BKPT(0);
    }

    /* Open timer used for I2S WS output */
    err = R_GPT_Open(&g_timer_ws_ctrl, &g_timer_ws_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Delay slave select output by 1 clock wrt. I2S WS */
    err = R_GPT_CounterSet(&g_timer_ws_ctrl, g_timer_ws_cfg.period_counts - 13);
    if(FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    err = R_GPT_Open(&g_timer_ssl_ctrl, &g_timer_ssl_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    err = R_GPT_CounterSet(&g_timer_ssl_ctrl, g_timer_ssl_cfg.period_counts - 14);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Start WS and SS timers */
    err = R_GPT_Start(&g_timer_ws_ctrl);
    if(FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    err = R_GPT_Start(&g_timer_ssl_ctrl);
    if(FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Initialize SPI peripheral */
    err = R_SPI_Open(&g_spi_i2s_ctrl, &g_spi_i2s_cfg);
    if(FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    err = R_GPT_Start(&g_timer_sck_ctrl);
    if(FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Set up the initial I2S read */
    err = R_SPI_Read(&g_spi_i2s_ctrl, dmic_buf[dmic_idx], DMIC_BUF_SIZE, SPI_BIT_WIDTH_32_BITS);
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
            R_SPI_Close(&g_spi_i2s_ctrl);
            R_SPI_Open(&g_spi_i2s_ctrl, &g_spi_i2s_cfg);
            do
            {
                /* Repeat this request if it fails */
                err = R_SPI_Read(&g_spi_i2s_ctrl, dmic_buf, DMIC_BUF_SIZE, SPI_BIT_WIDTH_32_BITS);
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

void g_spi_cb(spi_callback_args_t * p_args)
{
    if (SPI_EVENT_TRANSFER_COMPLETE == p_args->event)
    {
        /* Change index of the active write buffer */
        dmic_idx ^= 1;

        /* Start subsequent I2S read */
        R_SPI_Read(&g_spi_i2s_ctrl, dmic_buf[dmic_idx], DMIC_BUF_SIZE, SPI_BIT_WIDTH_32_BITS);

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
