/*
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** ============================================================================
 *  @file       UART.h
 *
 *  @brief      UART driver interface
 *
 *  The UART header file should be included in an application as follows:
 *  @code
 *  #include <ti/drivers/UART.h>
 *  @endcode
 *
 *  # Operation #
 *  The UART driver simplifies reading and writing to any of the UART
 *  peripherals on the board with multiple modes of operation and performance.
 *  These include blocking, non-blocking, and polling as well as text/binary
 *  mode, echo and return characters. The board's UART pins must
 *  be configured before initializing the UART driver. The application
 *  initializes the UART driver by calling UART_init() and is then ready to
 *  open a UART by calling UART_open(), passing in a UART parameters data
 *  structure.
 *
 *  The APIs in this driver serve as an interface to a typical TI-RTOS
 *  application. The specific peripheral implementations are responsible to
 *  create all the SYS/BIOS specific primitives to allow for thread-safe
 *  operation.
 *
 *  ## Opening the driver #
 *
 *  @code
 *  UART_Handle      handle;
 *  UART_Params      params;
 *
 *  UART_Params_init(&params);
 *  params.baudRate  = someNewBaudRate;
 *  params.writeDataMode = UART_DATA_BINARY;
 *  params.readDataMode = UART_DATA_BINARY;
 *  params.readReturnMode = UART_RETURN_FULL;
 *  params.readEcho = UART_ECHO_OFF;
 *  handle = UART_open(someUART_configIndexValue, &params);
 *  if (!handle) {
 *      System_printf("UART did not open");
 *  }
 *  @endcode
 *
 *  ## Writing data #
 *
 *  @code
 *  const unsigned char hello[] = "Hello World\n";
 *
 *  ret = UART_write(handle, hello, sizeof(hello));
 *  System_printf("The UART wrote %d bytes\n", ret);
 *  @endcode
 *
 *  ## Reading data #
 *
 *  @code
 *  unsigned char rxBuffer[20];
 *
 *  ret = UART_read(handle, rxBuffer, sizeof(rxBuffer));
 *  System_printf("The UART read %d bytes\n", ret);
 *  @endcode
 *
 *  # Implementation #
 *
 *  This module serves as the main interface for TI-RTOS
 *  applications. Its purpose is to redirect the module's APIs to specific
 *  peripheral implementations which are specified using a pointer to a
 *  UART_FxnTable.
 *
 *  The UART driver interface module is joined (at link time) to a
 *  NULL-terminated array of UART_Config data structures named *UART_config*.
 *  *UART_config* is implemented in the application with each entry being an
 *  instance of a UART peripheral. Each entry in *UART_config* contains a:
 *  - (UART_FxnTable *) to a set of functions that implement a UART peripheral
 *  - (void *) data object that is associated with the UART_FxnTable
 *  - (void *) hardware attributes that are associated with the UART_FxnTable
 *
 *  ## Stack requirements #
 *  It is STRONGLY discouraged to perform UART_read() or UART_write calls within
 *  the driver's own callback function when in UART_MODE_CALLBACK. Doing so will
 *  incur additional task or system stack size requirements. See the peripheral
 *  implementations' documentation for stack size estimations.
 *  It is expected that the user perform his/her own stack and usage analysis
 *  when choosing to nest these calls.
 *
 *  # Instrumentation #
 *  The UART driver interface produces log statements if instrumentation is
 *  enabled.
 *
 *  Diagnostics Mask | Log details |
 *  ---------------- | ----------- |
 *  Diags_USER1      | basic operations performed |
 *  Diags_USER2      | detailed operations performed |
 *
 *  ============================================================================
 */

#ifndef ti_drivers_UART__include
#define ti_drivers_UART__include

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 *  @defgroup UART_CONTROL UART_control command and status codes
 *  These UART macros are reservations for UART.h
 *  @{
 */

/*!
 * Common UART_control command code reservation offset.
 * UART driver implementations should offset command codes with
 * UART_CMD_RESERVED growing positively
 *
 * Example implementation specific command codes:
 * @code
 * #define UARTXYZ_CMD_COMMAND0     UART_CMD_RESERVED + 0
 * #define UARTXYZ_CMD_COMMAND1     UART_CMD_RESERVED + 1
 * @endcode
 */
#define UART_CMD_RESERVED            32

/*!
 * Common UART_control status code reservation offset.
 * UART driver implementations should offset status codes with
 * UART_STATUS_RESERVED growing negatively.
 *
 * Example implementation specific status codes:
 * @code
 * #define UARTXYZ_STATUS_ERROR0    UART_STATUS_RESERVED - 0
 * #define UARTXYZ_STATUS_ERROR1    UART_STATUS_RESERVED - 1
 * #define UARTXYZ_STATUS_ERROR2    UART_STATUS_RESERVED - 2
 * @endcode
 */
#define UART_STATUS_RESERVED        -32

/**
 *  @defgroup UART_STATUS Status Codes
 *  UART_STATUS_* macros are general status codes returned by UART_control()
 *  @{
 *  @ingroup UART_CONTROL
 */

/*!
 * @brief   Successful status code returned by UART_control().
 *
 * UART_control() returns UART_STATUS_SUCCESS if the control code was executed
 * successfully.
 */
#define UART_STATUS_SUCCESS         0

/*!
 * @brief   Generic error status code returned by UART_control().
 *
 * UART_control() returns UART_STATUS_ERROR if the control code was not executed
 * successfully.
 */
#define UART_STATUS_ERROR          -1

/*!
 * @brief   An error status code returned by UART_control() for undefined
 * command codes.
 *
 * UART_control() returns UART_STATUS_UNDEFINEDCMD if the control code is not
 * recognized by the driver implementation.
 */
#define UART_STATUS_UNDEFINEDCMD   -2
/** @}*/

/**
 *  @defgroup UART_CMD Command Codes
 *  UART_CMD_* macros are general command codes for UART_control(). Not all UART
 *  driver implementations support these command codes.
 *  @{
 *  @ingroup UART_CONTROL
 */

/*!
 * @brief   Command code used by UART_control() to read the next unsigned char.
 *
 * This command is used to read the next unsigned char from the UART's circular
 * buffer without removing it. With this command code, @b arg is a pointer to an
 * integer. @b *arg contains the next @c unsigned @c char read if data is
 * present, else @b *arg is set to UART_ERROR.
 */
#define UART_CMD_PEEK               0

/*!
 * @brief   Command code used by UART_control() to determine if the read buffer
 *          is empty.
 *
 * This command is used to determine if there are any unsigned chars available
 * to read from the UART's circular buffer using UART_read(). With this command
 * code, @b arg is a pointer to a @c bool. @b *arg contains @c true if data is
 * available, else @c false.
 */
#define UART_CMD_ISAVAILABLE        1

/*!
 * @brief   Command code used by UART_control() to determine how many unsigned
 *          chars are in the read buffer.
 *
 * This command is used to determine how many @c unsigned @c chars are available
 * to read from the UART's circular buffer using UART_read(). With this command
 * code, @b arg is a pointer to an @a integer. @b *arg contains the number of
 * @c unsigned @c chars available to read.
 */
#define UART_CMD_GETRXCOUNT         2

/*!
 * @brief   Command code used by UART_control() to enable data receive by the
 *          UART.
 *
 * This command is used to enable the UART in such a way that it stores received
 * unsigned chars into the circular buffer. For drivers that support power
 * management, this typically means that the UART will set a power constraint
 * while receive is enabled. UART_open() will always have this option
 * enabled. With this command code, @b arg is @a don't @a care.
 */
#define UART_CMD_RXENABLE           3

/*!
 * @brief   Command code used by UART_control() to disable data received by the
 *          UART.
 *
 * This command is used to disable the UART in such a way that ignores the data
 * it receives. For drivers that support power management, this typically means
 * that the driver will release any power constraints, to permit the system to
 * enter low power modes. With this command code, @b arg is @a don't @a care.
 *
 * @warning A call to UART_read() does !!NOT!! re-enable receive.
 */
#define UART_CMD_RXDISABLE          4
/** @}*/

/** @}*/

#define UART_ERROR  UART_STATUS_ERROR

/*!
 *  @brief    Wait forever define
 */
#define UART_WAIT_FOREVER (~0)

/*!
 *  @brief      A handle that is returned from a UART_open() call.
 */
typedef struct UART_Config      *UART_Handle;

/*!
 *  @brief      The definition of a callback function used by the UART driver
 *              when used in ::UART_MODE_CALLBACK
 *              The callback can occur in task or HWI context.
 *
 *  @warning    Making UART_read() or UART_write() calls within its own callback
 *              routines are STRONGLY discouraged as it will impact Task and
 *              System stack size requirements! See the documentation for the
 *              specific driver implementations for additional estimated stack
 *              requirements.
 *
 *  @param      UART_Handle             UART_Handle
 *
 *  @param      buf                     Pointer to read/write buffer
 *
 *  @param      count                   Number of elements read/written
 */
typedef void        (*UART_Callback)    (UART_Handle, void *buf, size_t count);

/*!
 *  @brief      UART mode settings
 *
 *  This enum defines the read and write modes for the configured UART.
 */
typedef enum UART_Mode {
    /*!
      *  Uses a semaphore to block while data is being sent.  Context of the call
      *  must be a Task.
      */
    UART_MODE_BLOCKING,

    /*!
      *  Non-blocking and will return immediately. When UART_write() or
      *  UART_read() has finished, the callback function is called from either
      *  the caller's context or from an interrupt context.
      */
    UART_MODE_CALLBACK
} UART_Mode;

/*!
 *  @brief      UART return mode settings
 *
 *  This enumeration defines the return modes for UART_read() and
 *  UART_readPolling(). This mode only functions when in UART_DATA_TEXT mode.
 *
 *  UART_RETURN_FULL unblocks or performs a callback when the read buffer has
 *  been filled.
 *  UART_RETURN_NEWLINE unblocks or performs a callback whenever a newline
 *  character has been received.
 *
 *  UART operation | UART_RETURN_FULL | UART_RETURN_NEWLINE |
 *  -------------- | ---------------- | ------------------- |
 *  UART_read      | Returns when buffer is full | Returns when buffer is full or newline was read |
 *  UART_write     | Sends data as is | Sends data with an additional newline at the end |
 *
 *  @pre        UART driver must be used in UART_DATA_TEXT mode.
 */
typedef enum UART_ReturnMode {
    /*! Unblock/callback when buffer is full. */
    UART_RETURN_FULL,

    /*! Unblock/callback when newline character is received. */
    UART_RETURN_NEWLINE
} UART_ReturnMode;

/*!
 *  @brief      UART data mode settings
 *
 *  This enumeration defines the data mode for read and write.
 *  In UART_DATA_TEXT mode the driver will examine the UART_ReturnMode value.
 *
 */
typedef enum UART_DataMode {
    UART_DATA_BINARY = 0, /*!< Data is not processed */
    UART_DATA_TEXT = 1    /*!< Data is processed according to above */
} UART_DataMode;

/*!
 *  @brief      UART echo settings
 *
 *  This enumeration defines if the driver will echo data when uses in
 *  UART_DATA_TEXT mode. This only applies to data received by the UART.
 *
 *  UART_ECHO_ON will echo back characters it received while in UART_DATA_TEXT
 *  mode.
 *  UART_ECHO_OFF will not echo back characters it received in UART_DATA_TEXT
 *  mode.
 *
 *  @pre        UART driver must be used in UART_DATA_TEXT mode.
 */
typedef enum UART_Echo {
    UART_ECHO_OFF = 0,  /*!< Data is not echoed */
    UART_ECHO_ON = 1    /*!< Data is echoed */
} UART_Echo;

/*!
 *  @brief    UART data length settings
 *
 *  This enumeration defines the UART data lengths.
 */
typedef enum UART_LEN {
    UART_LEN_5 = 0,        /*!< Data length is 5 bits */
    UART_LEN_6 = 1,        /*!< Data length is 6 bits */
    UART_LEN_7 = 2,        /*!< Data length is 7 bits */
    UART_LEN_8 = 3         /*!< Data length is 8 bits */
} UART_LEN;

/*!
 *  @brief    UART stop bit settings
 *
 *  This enumeration defines the UART stop bits.
 */
typedef enum UART_STOP {
    UART_STOP_ONE = 0,  /*!< One stop bit */
    UART_STOP_TWO = 1   /*!< Two stop bits */
} UART_STOP;

/*!
 *  @brief    UART parity type settings
 *
 *  This enumeration defines the UART parity types.
 */
typedef enum UART_PAR {
    UART_PAR_NONE = 0,  /*!< No parity */
    UART_PAR_EVEN = 1,  /*!< Parity bit is even */
    UART_PAR_ODD  = 2,  /*!< Parity bit is odd */
    UART_PAR_ZERO = 3,  /*!< Parity bit is always zero */
    UART_PAR_ONE  = 4   /*!< Parity bit is always one */
} UART_PAR;

/*!
 *  @brief    UART Parameters
 *
 *  UART parameters are used to with the UART_open() call. Default values for
 *  these parameters are set using UART_Params_init().
 *
 *  @sa       UART_Params_init()
 */
typedef struct UART_Params {
    UART_Mode         readMode;         /*!< Mode for all read calls */
    UART_Mode         writeMode;        /*!< Mode for all write calls */
    unsigned int      readTimeout;      /*!< Timeout for read semaphore */
    unsigned int      writeTimeout;     /*!< Timeout for write semaphore */
    UART_Callback     readCallback;     /*!< Pointer to read callback */
    UART_Callback     writeCallback;    /*!< Pointer to write callback */
    UART_ReturnMode   readReturnMode;   /*!< Receive return mode */
    UART_DataMode     readDataMode;     /*!< Type of data being read */
    UART_DataMode     writeDataMode;    /*!< Type of data being written */
    UART_Echo         readEcho;         /*!< Echo received data back */
    uint32_t          baudRate;         /*!< Baud rate for UART */
    UART_LEN          dataLength;       /*!< Data length for UART */
    UART_STOP         stopBits;         /*!< Stop bits for UART */
    UART_PAR          parityType;       /*!< Parity bit type for UART */
    uintptr_t         custom;           /*!< Custom argument used by driver
                                             implementation */
} UART_Params;

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_CloseFxn().
 */
typedef void        (*UART_CloseFxn)          (UART_Handle handle);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_ControlFxn().
 */
typedef int         (*UART_ControlFxn)        (UART_Handle handle,
                                               unsigned int cmd,
                                               void *arg);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_InitFxn().
 */
typedef void        (*UART_InitFxn)           (UART_Handle handle);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_OpenFxn().
 */
typedef UART_Handle (*UART_OpenFxn)           (UART_Handle handle,
                                               UART_Params *params);
/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_ReadFxn().
 */
typedef int     (*UART_ReadFxn)           (UART_Handle handle, void *buffer,
                                           size_t size);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_ReadPollingFxn().
 */
typedef int     (*UART_ReadPollingFxn)    (UART_Handle handle, void *buffer,
                                           size_t size);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_ReadCancelFxn().
 */
typedef void    (*UART_ReadCancelFxn)     (UART_Handle handle);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_WriteFxn().
 */
typedef int     (*UART_WriteFxn)          (UART_Handle handle,
                                           const void *buffer,
                                           size_t size);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_WritePollingFxn().
 */
typedef int     (*UART_WritePollingFxn)   (UART_Handle handle,
                                           const void *buffer,
                                           size_t size);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              UART_WriteCancelFxn().
 */
typedef void    (*UART_WriteCancelFxn)    (UART_Handle handle);

/*!
 *  @brief      The definition of a UART function table that contains the
 *              required set of functions to control a specific UART driver
 *              implementation.
 */
typedef struct UART_FxnTable {
    /*! Function to close the specified peripheral */
    UART_CloseFxn           closeFxn;

    /*! Function to implementation specific control function */
    UART_ControlFxn         controlFxn;

    /*! Function to initialize the given data object */
    UART_InitFxn            initFxn;

    /*! Function to open the specified peripheral */
    UART_OpenFxn            openFxn;

    /*! Function to read from the specified peripheral */
    UART_ReadFxn            readFxn;

    /*! Function to read via polling from the specified peripheral */
    UART_ReadPollingFxn     readPollingFxn;

    /*! Function to cancel a read from the specified peripheral */
    UART_ReadCancelFxn      readCancelFxn;

    /*! Function to write from the specified peripheral */
    UART_WriteFxn           writeFxn;

    /*! Function to write via polling from the specified peripheral */
    UART_WritePollingFxn    writePollingFxn;

    /*! Function to cancel a write from the specified peripheral */
    UART_WriteCancelFxn     writeCancelFxn;
} UART_FxnTable;

/*!
 *  @brief  UART Global configuration
 *
 *  The UART_Config structure contains a set of pointers used to characterize
 *  the UART driver implementation.
 *
 *  This structure needs to be defined before calling UART_init() and it must
 *  not be changed thereafter.
  *
 *  @sa     UART_init()
 */
typedef struct UART_Config {
    /*! Pointer to a table of driver-specific implementations of UART APIs */
    UART_FxnTable const    *fxnTablePtr;

    /*! Pointer to a driver specific data object */
    void                   *object;

    /*! Pointer to a driver specific hardware attributes structure */
    void          const    *hwAttrs;
} UART_Config;

/*!
 *  @brief  Function to close a UART peripheral specified by the UART handle
 *
 *  @pre    UART_open() has been called.
 *
 *  @param  handle      A UART_Handle returned from UART_open()
 *
 *  @sa     UART_open()
 */
extern void UART_close(UART_Handle handle);

/*!
 *  @brief  Function performs implementation specific features on a given
 *          UART_Handle.
 *
 *  Commands for UART_control can originate from UART.h or from implementation
 *  specific UART*.h (_UARTCC26XX.h_, _UARTTiva.h_, etc.. ) files.
 *  While commands from UART.h are API portable across driver implementations,
 *  not all implementations may support all these commands.
 *  Conversely, commands from driver implementation specific UART*.h files add
 *  unique driver capabilities but are not API portable across all UART driver
 *  implementations.
 *
 *  Commands supported by UART.h follow a UART_CMD_\<cmd\> naming
 *  convention.<br>
 *  Commands supported by UART*.h follow a UART*_CMD_\<cmd\> naming
 *  convention.<br>
 *  Each control command defines @b arg differently. The types of @b arg are
 *  documented with each command.
 *
 *  See @ref UART_CMD "UART_control command codes" for command codes.
 *
 *  See @ref UART_STATUS "UART_control return status codes" for status codes.
 *
 *  @pre    UART_open() has to be called.
 *
 *  @param  handle      A UART handle returned from UART_open()
 *
 *  @param  cmd         UART.h or UART*.h commands.
 *
 *  @param  arg         An optional R/W (read/write) command argument
 *                      accompanied with cmd
 *
 *  @return Implementation specific return codes. Negative values indicate
 *          unsuccessful operations.
 *
 *  @sa     UART_open()
 */
extern int UART_control(UART_Handle handle, unsigned int cmd, void *arg);

/*!
 *  @brief  Function to initialize the UART module
 *
 *  @pre    The UART_config structure must exist and be persistent before this
 *          function can be called. This function must also be called before
 *          any other UART driver APIs.
 */
extern void UART_init(void);

/*!
 *  @brief  Function to initialize a given UART peripheral
 *
 *  Function to initialize a given UART peripheral specified by the
 *  particular index value.
 *
 *  @pre    UART_init() has been called
 *
 *  @param  index         Logical peripheral number for the UART indexed into
 *                        the UART_config table
 *
 *  @param  params        Pointer to a parameter block. If NULL, default
 *                        parameter values will be used. All the fields in
 *                        this structure are RO (read-only).
 *
 *  @return A UART_Handle upon success. NULL if an error occurs, or if the
 *          indexed UART peripheral is already opened.
 *
 *  @sa     UART_init()
 *  @sa     UART_close()
 */
extern UART_Handle UART_open(unsigned int index, UART_Params *params);

/*!
 *  @brief  Function to initialize the UART_Params struct to its defaults
 *
 *  @param  params      An pointer to UART_Params structure for
 *                      initialization
 *
 *  Defaults values are:
 *      readMode = UART_MODE_BLOCKING;
 *      writeMode = UART_MODE_BLOCKING;
 *      readTimeout = UART_WAIT_FOREVER;
 *      writeTimeout = UART_WAIT_FOREVER;
 *      readCallback = NULL;
 *      writeCallback = NULL;
 *      readReturnMode = UART_RETURN_NEWLINE;
 *      readDataMode = UART_DATA_TEXT;
 *      writeDataMode = UART_DATA_TEXT;
 *      readEcho = UART_ECHO_ON;
 *      baudRate = 115200;
 *      dataLength = UART_LEN_8;
 *      stopBits = UART_STOP_ONE;
 *      parityType = UART_PAR_NONE;
 */
extern void UART_Params_init(UART_Params *params);

/*!
 *  @brief  Function that writes data to a UART with interrupts enabled. Usage
 *          of this API is mutually exclusive with usage of
 *          UART_writePolling(). In other words, for an opened UART peripheral,
 *          either UART_write() or UART_writePolling() may be used, but not
 *          both.
 *
 *  In UART_MODE_BLOCKING, UART_write() will block task execution until all
 *  the data in buffer has been written.
 *
 *  In UART_MODE_CALLBACK, UART_write() does not block task execution and calls
 *  a callback function specified by writeCallback. The callback function can
 *  occur in the caller's task context or in a HWI or SWI context, depending on the device.
 *
 *  @warning It is STRONGLY discouraged to call UART_write from its own callback
 *           function (UART_MODE_CALLBACK).
 *
 *  @sa UART_writePolling()
 *
 *  @param  handle      A UART_Handle
 *
 *  @param  buffer      A WO (write-only) pointer to buffer containing data to
 *                      be written to the UART.
 *
 *  @param  size        The number of bytes in the buffer that should be written
 *                      to the UART.
 *
 *  @return Returns the number of bytes that have been written to the UART.
 *          If an error occurs, UART_ERROR is returned. In UART_MODE_CALLBACK,
 *          the return value is always 0.
 */
extern int UART_write(UART_Handle handle, const void *buffer, size_t size);

/*!
 *  @brief  Function that writes data to a UART, polling the peripheral to
 *          wait until new data can be written. Usage of this API is mutually
 *          exclusive with usage of UART_write().
 *
 *  This function initiates an operation to write data to a UART controller.
 *
 *  UART_writePolling() will not return until all the data was written to the
 *  UART (or to its FIFO if applicable).
 *
 *  @sa UART_write()
 *
 *  @param  handle      A UART_Handle
 *
 *  @param  buffer      A pointer to the buffer containing the data to
 *                      be written to the UART.
 *
 *  @param  size        The number of bytes in the buffer that should be written
 *                      to the UART.
 *
 *  @return Returns the number of bytes that have been written to the UART.
 *          If an error occurs, UART_ERROR is returned.
 */
extern int UART_writePolling(UART_Handle handle, const void *buffer,
                             size_t size);

/*!
 *  @brief  Function that cancels a UART_write() function call.
 *
 *  This function cancels a UART_write() operation to a UART peripheral.
 *  This function is only applicable to UART_MODE_CALLBACK.
 *
 *  @param  handle      A UART_Handle
 */
extern void UART_writeCancel(UART_Handle handle);

/*!
 *  @brief  Function that reads data from a UART with interrupt enabled. This
 *          API must be used mutually exclusive with UART_readPolling().
 *
 *  This function initiates an operation to read data from a UART controller.
 *
 *  In UART_MODE_BLOCKING, UART_read() will block task execution until all
 *  the data in buffer has been read.
 *
 *  In UART_MODE_CALLBACK, UART_read does not block task execution an calls a
 *  callback function specified by readCallback. The callback function can
 *  occur in the caller's or in HWI or SWI context, depending on the device.
 *
 *  @warning It is STRONGLY discouraged to call UART_read from its own callback
 *           function (UART_MODE_CALLBACK).
 *
 *  @sa UART_readPolling()
 *
 *  @param  handle      A UART_Handle
 *
 *  @param  buffer      A RO (read-only) pointer to an empty buffer in which
 *                      received data should be written to.
 *
 *  @param  size        The number of bytes to be written into buffer
 *
 *  @return Returns the number of bytes that have been read from the UART,
 *          UART_ERROR on an error.
 */
extern int UART_read(UART_Handle handle, void *buffer, size_t size);

/*!
 *  @brief  Function that reads data from a UART without interrupts. This API
 *          must be used mutually exclusive with UART_read().
 *
 *  This function initiates an operation to read data from a UART peripheral.
 *
 *  UART_readPolling will not return until size data was read to the UART.
 *
 *  @sa UART_read()
 *
 *  @param  handle      A UART_Handle
 *
 *  @param  buffer      A RO (read-only) pointer to an empty buffer in which
 *                      received data should be written to.
 *
 *  @param  size        The number of bytes to be written into buffer
 *
 *  @return Returns the number of bytes that have been read from the UART,
 *          UART_ERROR on an error.
 */
extern int UART_readPolling(UART_Handle handle, void *buffer, size_t size);

/*!
 *  @brief  Function that cancels a UART_read() function call.
 *
 *  This function cancels a UART_read() operation for a UART peripheral. This
 *  function is only applicable to UART_MODE_CALLBACK.
 *
 *  @param  handle      A UART_Handle
 */
extern void UART_readCancel(UART_Handle handle);

#ifdef __cplusplus
}
#endif

#endif /* ti_drivers_UART__include */
