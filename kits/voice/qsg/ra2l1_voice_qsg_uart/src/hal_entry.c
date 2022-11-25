#include "hal_data.h"
#include "stdio.h"

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

static volatile bool uart_done;
static volatile char uart_rec;

void hal_entry(void)
{
    fsp_err_t err;

    /* Initialize SCI peripheral in UART mode */
    err = R_SCI_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Perform UART write */
    err = R_SCI_UART_Write(&g_uart0_ctrl, (void *) "Hello from Renesas VOICE kit\r\n", 30);
    if (FSP_SUCCESS != err)
    {
        __BKPT(0);
    }

    /* Wait for interrupt & check for completion */
    while (false == uart_done)
        __WFI();

    uart_done = false;

    while (1)
    {
        /* Wait for interrupt & check for received data */
        while ('\0' == uart_rec)
            __WFI();

        char text_buf[32] = {0};
        snprintf(text_buf, 32, "Received character: '%c'\r\n", uart_rec);

        uart_rec = '\0';

        /* Perform UART write */
        err = R_SCI_UART_Write(&g_uart0_ctrl, (void *) text_buf, strlen(text_buf));
        if (FSP_SUCCESS != err)
        {
            __BKPT(0);
        }

        /* Wait for interrupt & check for completion */
        while (false == uart_done)
            __WFI();

        uart_done = false;
    }
}

void g_uart0_cb(uart_callback_args_t * p_args)
{
    if (UART_EVENT_TX_COMPLETE == p_args->event)
    {
        uart_done = true;
    }

    else if (UART_EVENT_RX_CHAR == p_args->event)
    {
        uart_rec = (char) p_args->data;
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
