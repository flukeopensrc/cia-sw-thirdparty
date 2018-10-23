#include <ctype.h>

#include "usbtmc488.c"

/***************************************************************************************
 * BEGIN STANDALONE TEST APPLICATION
****************************************************************************************/
const char testVersionString[64] = "3.5";
const char *pTestVersion  = &testVersionString[0];

#define CFG_USBTMC_MAX_STRINGLEN          64
#define CFG_USBTMC_VENDOR_ID              0x0F7E
#define CFG_USBTMC_PRODUCT_ID             0x1234
#define CFG_USBTMC_MFG_DESCR              "Fluke"
#define CFG_USBTMC_CFG_DESCR              "Standard"
#define CFG_USBTMC_IFC_DESCR              "USBTMC-USB488"
#define CFG_USBTMC_MAXPOWER               (uint8_t)50
#define CFG_USBTMC_SELFPOWERED            true
#define CFG_USBTMC_HARDWARE_ID            (uint32_t)0
#define CFG_VERSION_STRING_LEN            16
#define CFG_MODEL_ID_STRING               "USBTMC Gadget"
#define CFG_SERIAL_NUMBER_STRING          "999"

#undef  DBG_PRINT
#define DBG_PRINT(ctx, fmt, args...)     usbtmc488_tsprintf("%s: " fmt, __func__, ##args)
#define DBG_ERROR(ctx, fmt, args...)     usbtmc488_tsprintf("%s: ERROR: " fmt, __func__, ##args)
#define DBG_WARNING(ctx, fmt, args...)   usbtmc488_tsprintf("%s: WARNING: " fmt, __func__, ##args)
#define DBG_VERBOSE(ctx, fmt, args...)   usbtmc488_tsprintf("%s: " fmt, __func__, ##args)

#undef  _NELEM
#define _NELEM(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifndef WIN32
typedef __u8    uint8_t;
typedef __u16   uint16_t;
typedef __u32   uint32_t;

#define strcat_s(s, l, t)               strcat(s, t)
#define strcpy_s(d, l, s)               strcpy(d, s)
#define vsprintf_s(b, l, fmt, args...)  vsprintf(b, fmt, ##args)
#define sprintf_s(b, l, fmt, args...)   sprintf(b, fmt, ##args)
#endif /* WIN32 */

#define MAX_OPC_TIMEOUT 4

/* ESR bit definitions */
#define _ESR_OPC        (0x01 << 0) /* Operation Complete      0x01 */
#define _ESR_RQC        (0x01 << 1) /* Request Control         0x02 */
#define _ESR_QYE        (0x01 << 2) /* Query Error             0x04 */
#define _ESR_DDE        (0x01 << 3) /* Device Dependent Error  0x08 */
#define _ESR_EXE        (0x01 << 4) /* Execution Error         0x10 */
#define _ESR_CME        (0x01 << 5) /* Command Error           0x20 */
#define _ESR_URQ        (0x01 << 6) /* User Request            0x40 */
#define _ESR_PON        (0x01 << 7) /* Power On                0x80 */

/* ESE bit definitions */
#define _ESE_OPC        _ESR_OPC   /* Operation Complete */
#define _ESE_RQC        _ESR_RQC   /* Request Control */
#define _ESE_QYE        _ESR_QYE   /* Query Error */
#define _ESE_DDE        _ESR_DDE   /* Device Dependent Error */
#define _ESE_EXE        _ESR_EXE   /* Execution Error */
#define _ESE_CME        _ESR_CME   /* Command Error */
#define _ESE_URQ        _ESR_URQ   /* User Request */
#define _ESE_PON        _ESR_PON   /* Power On */

/* STB bit definitions */
#define _STB_MAV        (0x01 << 4) /* Message Available     0x10 */
#define _STB_ESB        (0x01 << 5) /* Event Summary Bit     0x20 */
#define _STB_RQS        (0x01 << 6) /* Request for Service   0x40 */

/* SRE bit definitions */
#define _SRE_MAV        _STB_MAV   /* Message Available */
#define _SRE_ESB        _STB_ESB   /* Event Summary Bit */
#define _SRE_RQS        _STB_RQS   /* Request for Service */

#define PTEST_TYPE      "USB-TMC"
#define F_WAS_CURVE     (0x01 << 0)
#define F_SHUTDOWN      (0x01 << 1)

typedef struct test_s {
    char            name[64];
    uint8_t         dese;
    uint8_t         esr;
    uint8_t         ese;
    uint8_t         sre;
    uint8_t         stb;
    int             n_events;
    int             n_events_alloc;
    char          **events;
    uint32_t        flags;
    uint32_t        rec_length;
    uint8_t        *buffer;
    uint32_t        buffer_offset;
    uint32_t        buffer_length;
    uint32_t        buffer_size;
    char           *write_buffer;
    struct test_s  *next;
} test_t;

test_t *g_test = NULL;

/****************************************************************************
 * DbgEsrToString
 */
const char *
DbgEsrToString(uint8_t esr)
{
    static char esr_str[64];

    *esr_str = '\0';
    if (esr & _ESR_OPC) { strcat_s(esr_str, sizeof(esr_str) - strlen(esr_str), "OPC|"); esr &= ~_ESR_OPC; }
    if (esr & _ESR_RQC) { strcat_s(esr_str, sizeof(esr_str) - strlen(esr_str), "RQC|"); esr &= ~_ESR_RQC; }
    if (esr & _ESR_QYE) { strcat_s(esr_str, sizeof(esr_str) - strlen(esr_str), "QYE|"); esr &= ~_ESR_QYE; }
    if (esr & _ESR_DDE) { strcat_s(esr_str, sizeof(esr_str) - strlen(esr_str), "DDE|"); esr &= ~_ESR_DDE; }
    if (esr & _ESR_EXE) { strcat_s(esr_str, sizeof(esr_str) - strlen(esr_str), "EXE|"); esr &= ~_ESR_EXE; }
    if (esr & _ESR_CME) { strcat_s(esr_str, sizeof(esr_str) - strlen(esr_str), "CME|"); esr &= ~_ESR_CME; }
    if (esr & _ESR_URQ) { strcat_s(esr_str, sizeof(esr_str) - strlen(esr_str), "URQ|"); esr &= ~_ESR_URQ; }
    if (esr & _ESR_PON) { strcat_s(esr_str, sizeof(esr_str) - strlen(esr_str), "PON|"); esr &= ~_ESR_PON; }
    if (esr != 0x00)    { sprintf_s(&(esr_str[strlen(esr_str)]), sizeof(esr_str) - strlen(esr_str), "0x%02X|", esr); }
    if (*esr_str != '\0') esr_str[strlen(esr_str) - 1] = '\0';

    return esr_str;
}

/****************************************************************************
 * DbgStbToString
 */
const char *
DbgStbToString(uint8_t stb)
{
    static char stb_str[64];

    *stb_str = '\0';
    if (stb & _STB_MAV) { strcat_s(stb_str, sizeof(stb_str) - strlen(stb_str), "MAV|"); stb &= ~_STB_MAV; }
    if (stb & _STB_ESB) { strcat_s(stb_str, sizeof(stb_str) - strlen(stb_str), "ESB|"); stb &= ~_STB_ESB; }
    if (stb & _STB_RQS) { strcat_s(stb_str, sizeof(stb_str) - strlen(stb_str), "RQS|"); stb &= ~_STB_RQS; }
    if (stb != 0x00)    { sprintf_s(&(stb_str[strlen(stb_str)]), sizeof(stb_str) - strlen(stb_str), "0x%02X|", stb); }
    if (*stb_str != '\0') stb_str[strlen(stb_str) - 1] = '\0';

    return stb_str;
}

/****************************************************************************
 * ptest_update_stb
 */
static uint8_t
ptest_update_stb(
    test_t     *pTest
    )
{
    uint8_t stb = 0x0;

    if (pTest->esr & pTest->ese)
    {
        stb |= _STB_ESB;
    }

    if (pTest->stb & _STB_MAV)
    {
        if (pTest->buffer_length != 0)
        {
            stb |= _STB_MAV;
        }
        else
        {
            pTest->stb &= ~_STB_MAV;
        }
    }

    if (stb & pTest->sre)
{
        stb |= _STB_RQS;
    }

    return stb;
}

/****************************************************************************
 * ptest_srq_event
 */
static void
ptest_srq_event(
    test_t     *pTest
    )
{
    DBG_VERBOSE(DBG_CTX, "<<<<< SrqEvent (%s) >>>>>\n", DbgStbToString(ptest_update_stb(pTest)));

//DWG_SRQ_EVENT

#ifndef DWG_SRQ_EVENT
    USBTMC488_MESSAGE       deviceMsg;
//DWG    USBTMC488_MSG_BUFFER    deviceMsgBuf;
    USBTMC488_STATUS        deviceStatus;

    memset(&deviceMsg, 0, sizeof(deviceMsg));
//DWG    memset(&deviceMsgBuf, 0, sizeof(deviceMsgBuf));
//DWG    deviceMsg.msg_buffer = &deviceMsgBuf;
//DWG    deviceMsgBuf.buffer = NULL;
//DWG    deviceMsgBuf.length = 0;

#ifndef DWG_UPDATE_STB
    deviceMsg.type = USBTMC488_MSG_UPDATE_STATUS_BYTE;
    deviceMsg.value = pTest->stb;
    if ((deviceStatus = usbtmc488_message(&deviceMsg)) != USBTMC488_SUCCESS)
    {
        DBG_ERROR(DBG_CTX, "%s sending msg type %s to gadget\n",
            usbtmc488_status_to_string(deviceStatus),
            usbtmc488_msg_type_to_string(deviceMsg.type));
    }
#endif // DWG_UPDATE_STB

    deviceMsg.type = USBTMC488_MSG_REQUEST_SERVICE;
    deviceMsg.value = pTest->stb;
    if ((deviceStatus = usbtmc488_message(&deviceMsg)) != USBTMC488_SUCCESS)
        {
        DBG_ERROR(DBG_CTX, "%s sending msg type %s to gadget\n",
                usbtmc488_status_to_string(deviceStatus),
                usbtmc488_msg_type_to_string(deviceMsg.type));
        }
#endif // DWG_SRQ_EVENT
}

/****************************************************************************
 * ptest_output_flush
 */
static void
ptest_output_flush(
    test_t     *pTest
    )
{
    pTest->buffer_offset = pTest->buffer_length = 0;
    pTest->stb &= ~_STB_MAV;

//DWG_OUTPUT_FLUSH

    ptest_srq_event(pTest);
}

/****************************************************************************
 * ptest_device_clear
 */
static void
ptest_device_clear(
    test_t     *pTest
    )
{
    int n_events, x;

    n_events = pTest->n_events;
    pTest->n_events = 0;

    for (x = 0; x < n_events; x++)
    {
        if (pTest->events[x] != NULL)
        {
            free(pTest->events[x]);
            pTest->events[x] = NULL;
        }
    }

    pTest->stb &= ~(_STB_MAV | _STB_ESB | _STB_RQS);
    ptest_output_flush(pTest);
}

/****************************************************************************
 * ptest_queue_event
 */
static int
ptest_queue_event(
    test_t     *pTest,
    uint8_t     esrMask,
    const char *fmt,
    ...
    )
{
    va_list     vaList;
    char        event_buffer[256];
    char       *ebuf;

    va_start(vaList, fmt);
    vsprintf_s(event_buffer, sizeof(event_buffer), fmt, vaList);
    va_end(vaList);

//DWG    DBG_PRINT(DBG_CTX, "%s - ESR:%s '%s'\n", pTest->name, DbgEsrToString(esrMask), event_buffer);

    if ((pTest->n_events + 1) >= pTest->n_events_alloc)
    {
        char **events;
        int n_events_alloc;

        n_events_alloc = pTest->n_events_alloc + 16;

        events = (char **) realloc(pTest->events, n_events_alloc * sizeof(pTest->events[0]));

        if (events == NULL)
        {
            DBG_ERROR(DBG_CTX, "%s - Unable to grow event list from %d to %d\n", pTest->name, pTest->n_events_alloc, n_events_alloc);

            int x;

            for (x = 0; x < pTest->n_events; x++)
            {
                if (pTest->events[x] != NULL)
        {
                    free(pTest->events[x]);
                    pTest->events[x] = NULL;
                }
            }
            pTest->n_events = 0;
        }
        else
        {
            int x;

            for (x = pTest->n_events; x < n_events_alloc; x++)
            {
                events[x] = NULL;
            }

            pTest->n_events_alloc = n_events_alloc;
            pTest->events = events;
        }
    }

    if ((ebuf = (char *) malloc(strlen(event_buffer) + 1)) == NULL)
    {
        DBG_ERROR(DBG_CTX, "%s - Unable to allocate storage for event buffer\n", pTest->name);
    }
    strcpy_s(ebuf, strlen(event_buffer) + 1, event_buffer);
    pTest->events[pTest->n_events++] = ebuf;

    if (pTest->dese & esrMask)
    {
        pTest->esr |= esrMask;
    }

    return pTest->n_events;
}

/****************************************************************************
 * ptest_append_response
 */
static int
ptest_append_response(
    test_t     *pTest,
    const char *fmt,
    ...
    )
{
    int         nBytes = 0;
    int         n;
    va_list     vaList;

    va_start(vaList, fmt);
    if (pTest->buffer_length != 0)
    {
        n = sprintf_s((char *) &(pTest->buffer[pTest->buffer_length]), pTest->buffer_size - pTest->buffer_length, ";");
        pTest->buffer_length += n; nBytes += n;
    }
    n = vsprintf_s((char *) &(pTest->buffer[pTest->buffer_length]), pTest->buffer_size - pTest->buffer_length, fmt, vaList);
    pTest->buffer_length += n; nBytes += n;
    va_end(vaList);

    return nBytes;
}

/****************************************************************************
 * ptest_close
 */
static void
ptest_close(
    test_t     *pTest
    )
{
    if (pTest != NULL)
    {
        if (g_test == pTest)
        {
            g_test = pTest->next;
            pTest->next = NULL;
        }
        else
        {
            test_t *pPrev;

            for (pPrev = g_test; (pPrev != NULL) && (pPrev->next != pTest); pPrev = pPrev->next);

            if (pPrev != NULL)
            {
                pPrev->next = pTest->next;
                pTest->next = NULL;
        }
        else
        {
                DBG_WARNING(DBG_CTX, "Unable to find test device in list! Name:'%s'\n", pTest->name);
            }
        }

        if (pTest->buffer != NULL)
        {
            pTest->buffer_offset = pTest->buffer_length = 0;
            pTest->buffer_size = 0;

            free(pTest->buffer);
            pTest->buffer = NULL;
        }

        if (pTest->n_events != 0)
        {
            int n_events, x;

            n_events = pTest->n_events;
            pTest->n_events = 0;

            for (x = 0; x < n_events; x++)
            {
                if (pTest->events[x] != NULL)
                {
                    free(pTest->events[x]);
                    pTest->events[x] = NULL;
                }
            }
        }

        if (pTest->n_events_alloc != 0)
        {
            pTest->n_events_alloc = 0;

            free(pTest->events);
            pTest->events = NULL;
        }

        free(pTest);
        pTest = NULL;
        }

    return;
}

/****************************************************************************
 * ptest_open
 */
static test_t *
ptest_open(
    const char     *name
    )
{
    test_t *pTest = NULL;

    pTest = (test_t *) malloc(sizeof(*pTest));
    if (pTest == NULL)
    {
        DBG_ERROR(DBG_CTX, "Unable to allocate a new test device '%s'.\n", name);
        goto e_ptest_open;
    }
    memset(pTest, 0, sizeof(*pTest));
    strcpy_s(pTest->name, sizeof(pTest->name), name);
    pTest->dese = 0xFF;
    pTest->ese = 0x00;
    pTest->sre = 0x00;
    pTest->rec_length = 5000;
    pTest->write_buffer = NULL;

    pTest->buffer_size = pTest->rec_length + 32;
    pTest->buffer = (uint8_t *) malloc(pTest->buffer_size);
    if (pTest->buffer == NULL)
    {
        DBG_ERROR(DBG_CTX, "Unable to allocate device buffer of %u bytes\n", pTest->buffer_size);
        pTest->buffer_size = 0;
        goto e_ptest_open;
    }

    ptest_queue_event(pTest, _ESR_PON, "401,\"Power on\"");

    if (g_test != NULL)
    {
        pTest->next = g_test;
        g_test = pTest;
    }
    else
        {
        g_test = pTest;
        }

    return pTest;

e_ptest_open:
    if (pTest != NULL)
    {
        if (pTest->buffer != NULL)
        {
            free(pTest->buffer);
            pTest->buffer = NULL;
        }
        pTest->buffer_size = 0;

        free(pTest);
        pTest = NULL;
        }

    return NULL;
}

/****************************************************************************
 * ptest_write
         */
static int
ptest_write(
    test_t         *pTest,
    const void     *msg,
    uint32_t        msg_len,
    int             end_asserted
    )
{
    int     status = -1;
    char   *buffer = NULL;

    if (pTest == NULL)
    {
        DBG_ERROR(DBG_CTX, "Invalid pTest context.\n");
        goto e_ptest_write;
    }

    if (pTest->write_buffer)
    {
        size_t write_buffer_len;

        write_buffer_len = strlen(pTest->write_buffer);

        buffer = (char *) malloc(write_buffer_len + msg_len + 1);
        if (buffer == NULL)
        {
            DBG_ERROR(DBG_CTX, "Unable to re-allocate msg buffer.\n");
            goto e_ptest_write;
        }
        memcpy(buffer, pTest->write_buffer, write_buffer_len);
        memcpy(buffer + write_buffer_len, msg, msg_len);
        buffer[write_buffer_len + msg_len] = '\0';

        free(pTest->write_buffer);
        pTest->write_buffer = NULL;
    }
    else
    {
        buffer = (char *) malloc(msg_len + 1);
        if (buffer == NULL)
        {
            DBG_ERROR(DBG_CTX, "Unable to allocate msg buffer.\n");
            goto e_ptest_write;
        }
        memcpy(buffer, msg, msg_len);
        buffer[msg_len] = '\0';
    }

    if (end_asserted)
    {
        char *bp;

        pTest->flags = 0;
        pTest->buffer_offset = pTest->buffer_length = 0;

        for (bp = buffer; *bp != '\0';)
        {
            char *ep;
            char *p;

            if ((ep = strchr(bp, ';')) != NULL)
            {
                *ep = '\0';
                ep++;
            }
            else
            {
                ep = &(bp[strlen(bp)]);
            }

            /* Strip leading spaces */
            while ((*bp != '\0') && isspace(*bp)) bp++;

            /* Strip trailing spaces */
            for (p = &(bp[strlen(bp) - 1]); (isspace(*p) && (p >= bp)); p--); p[1] = '\0';

            /* Skip leading ':' */
            while ((*bp != '\0') && (*bp == ':')) bp++;

#define _CMD_MATCH_N(s) (strncasecmp(bp, s, strlen(s)) == 0)
#define _CMD_MATCH(s) (strcasecmp(bp, s) == 0)

            if (_CMD_MATCH("id?") || _CMD_MATCH("*idn?"))
            {
                uint32_t    x;
                char        buf[32];
                char       *cname = NULL;
#ifndef WIN32
                char        hname[128] = { 0 };
#else /* WIN32 */
                errno_t     err;
                size_t      clen;
#endif /* WIN32 */

                // TEKTRONIX,TDS 220,0,CF:91.1CT FV:v2.03 TDS2CM:CMV:v1.04
                // TEKTRONIX,TLA7N3,B000000,SCPI:94.0 FVER:4.3.14
                // TEKTRONIX,TDS5054,B020490,CF:91.1CT FV:1.2.1
                // USBTESTDEVICE,MODEL 0,SERIAL 0,CF:91.1CT FV:v1.00

                /* Escape the ',' since that is the field separator */
                strcpy_s(buf, sizeof(buf), pTest->name);
                for (x = 0; x < strlen(buf); x++)
                {
                    if (buf[x] == ',') buf[x] = '.';
                }
#ifndef WIN32
                cname = hname;
                if (gethostname(hname, sizeof(hname)) != 0)
                {
                    strcpy_s(hname, sizeof(hname), "UNKNOWN");
                }
                for (x = 0; x < strlen(hname); x++)
                {
                    if ((hname[x] >= 'a') && (hname[x] <= 'z'))
                    {
                        hname[x] = 'A' + (hname[x] - 'a');
                    }
                }
#else /* WIN32 */
                err = _dupenv_s(&cname, &clen, "COMPUTERNAME");
                if (err)
                {
                    cname = "UNKNOWN";
                }
#endif /* WIN32 */
                ptest_append_response(pTest, "Fluke," PTEST_TYPE " (%s),SN-%s,SCPI:94.0 FV:v1.00", buf, cname);
#ifdef WIN32
                if (!err)
                {
                    free(cname);
                    cname = NULL;
                }
#endif /* WIN32 */
            }
            else if (_CMD_MATCH("{exit}"))
        {
                pTest->flags |= F_SHUTDOWN;

                ptest_append_response(pTest, "<<<< Shutting down server >>>>");

#if TPI_DBG_MALLOC
                gShutdownFlag = TRUE;
#endif /* TPI_DBG_MALLOC */
            }
            else if (_CMD_MATCH("*opc?"))
            {
                ptest_append_response(pTest, "%d", 1);
            }
            else if (_CMD_MATCH("*opc"))
            {
                ptest_queue_event(pTest, _ESR_OPC, "402,\"Operation complete\"");
                if (pTest->dese & _ESE_OPC)
                {
#ifndef WIN32
                    alarm(random() % MAX_OPC_TIMEOUT);
#else /* WIN32 */
                    if ((pTest->sre & _SRE_ESB) &&
                        (pTest->ese & pTest->esr))
                    {
                        ptest_srq_event(pTest);
                    }
#endif /* WIN32 */
                }
            }
            else if (_CMD_MATCH("*cls"))
            {
                ptest_device_clear(pTest);
            }
            else if (0
//DWG - Generate OPC
// AUTOSET (EXECute|UNDo)
// ACQuire:STATE ({NR1}|ON|RUN)
// FACtory
// RECAll:SETUp "xxx"
// *RST
// SAVe:IMAGe "xxx"
// SAVe:SETUp "xxx"
                || _CMD_MATCH_N("autoset ")
                || _CMD_MATCH_N("acquire:state ") // ACQuire:STATE ({NR1}|ON|RUN)
                || _CMD_MATCH_N("recall:setup ") // RECAll:SETUp (FACtory|{NR1}|"{file_path}")
                || _CMD_MATCH_N("*rcl ") // *RCL {NR1}
                || _CMD_MATCH_N("save:setup ") // SAVe:SETUp ("{file_path}"|{NR1})
                || _CMD_MATCH_N("*sav ") // *SAV {NR1}
                || _CMD_MATCH("*wai")
                || 0)
            {
                usleep(300);
            }
            else if (0
                || _CMD_MATCH_N("ACQUIRE:")
                || _CMD_MATCH_N("DATA:ENCDG ")
                || _CMD_MATCH_N("DATA:START ")
                || _CMD_MATCH_N("DATA:STOP ")
                || _CMD_MATCH_N("SOURCE CH")
                || _CMD_MATCH_N("REPET ")
                || _CMD_MATCH_N("STATE ")
                || _CMD_MATCH_N("MODE ")
                || _CMD_MATCH_N("START ")
                || _CMD_MATCH_N("STOP ")
                || _CMD_MATCH_N("WIDTH ")
                || _CMD_MATCH_N("DATA INIT")
                || _CMD_MATCH_N("CURVE #")
                || _CMD_MATCH_N("HEADER ") || _CMD_MATCH_N("HDR ") // HEADer|:HDR (OFF|ON|{NR1})
                || _CMD_MATCH_N("VERBOSE ") // VERBOSE (OFF|ON|{NR1})
                || 0)
            {
//DWG
            }
            else if (_CMD_MATCH("factory") || _CMD_MATCH("*rst"))
            {
                usleep(500 * 1000);
                ptest_device_clear(pTest);
            }
            else if (_CMD_MATCH("set?") || _CMD_MATCH("*lrn?"))
            {
                ptest_append_response(pTest, "LOTS_OF_SETTINGS");
            }
            else if (_CMD_MATCH_N("pause ")) // PAUSe {NR3}
            {
                double secs;

                secs = strtod(&(bp[6]), NULL);
                if ((secs >= 0.0) && (secs < 1800.0))
                {
                    DBG_PRINT(DBG_CTX, "******** Pause for %.2f seconds ********\n", secs);
                    usleep(secs * 1000 * 1000);
                }
            }
            else if (_CMD_MATCH("busy?"))
            {
                ptest_append_response(pTest, "0");
            }
            else if (_CMD_MATCH_N("dese "))
            {
                pTest->dese = (uint8_t) strtoul(&(bp[5]), NULL, 0);
            }
            else if (_CMD_MATCH_N("*ese "))
            {
                pTest->ese = (uint8_t) strtoul(&(bp[5]), NULL, 0);
            }
            else if (_CMD_MATCH_N("*sre "))
            {
                pTest->sre = (uint8_t) strtoul(&(bp[5]), NULL, 0) & ~_SRE_RQS;
            }
            else if (_CMD_MATCH("*esr?"))
            {
                ptest_append_response(pTest, "%d", pTest->esr);
                pTest->esr = 0x00;
            }
            else if (_CMD_MATCH("dese?"))
            {
                ptest_append_response(pTest, "%d", pTest->dese);
            }
            else if (_CMD_MATCH("*ese?"))
            {
                ptest_append_response(pTest, "%d", pTest->ese);
            }
            else if (_CMD_MATCH("*stb?") || _CMD_MATCH("stb?"))
            {
                ptest_append_response(pTest, "%d", ptest_update_stb(pTest));
            }
            else if (_CMD_MATCH("*sre?"))
            {
                ptest_append_response(pTest, "%d", pTest->sre);
            }
            else if (_CMD_MATCH("evqty?"))
            {
                ptest_append_response(pTest, "%d", (pTest->esr != 0x0) ? 0 : pTest->n_events);
            }
            else if (_CMD_MATCH("allev?") || _CMD_MATCH("evmsg?"))
            {
                if (pTest->n_events == 0)
                {
                    ptest_append_response(pTest, "0,\"No events to report - queue empty\"");
                }
                else
                {
                    if (pTest->esr != 0x0)
                    {
                        ptest_append_response(pTest, "1,\"No events to report - new events pending *ESR?\"");
                    }
                    else
                    {
                        if (_CMD_MATCH("evmsg?"))
                        {
                            char   *event;
                            int     x;

                            x = 0;
                            if ((event = pTest->events[x]) != NULL)
                            {
                                pTest->events[x] = NULL;

                                ptest_append_response(pTest, event);

                                free(event);
                                event = NULL;
                            }

                            pTest->n_events -= 1;
                            for (x = 0; x < pTest->n_events; x++)
                            {
                                pTest->events[x] = pTest->events[x + 1];
                            }
                        }
                        else
                        {
                            int n_events, x;

                            n_events = pTest->n_events;
                            pTest->n_events = 0;

                            for (x = 0; x < n_events; x++)
                            {
                                char   *event;

                                if ((event = pTest->events[x]) != NULL)
                                {
                                    pTest->events[x] = NULL;

                                    ptest_append_response(pTest, event);

                                    free(event);
                                    event = NULL;
                                }
                            }
                        }
                    }
                }
            }
            else if (_CMD_MATCH_N("horizontal:recordlength?"))
            {
                ptest_append_response(pTest, "%d", pTest->rec_length);
            }
            else if (_CMD_MATCH_N("horizontal:recordlength "))
            {
                pTest->rec_length = (uint32_t) strtoul(&(bp[24]), NULL, 0);

                if ((pTest->rec_length + 32) > pTest->buffer_size)
                {
                    pTest->buffer_size = pTest->rec_length + 32;
                    pTest->buffer = (uint8_t *) realloc(pTest->buffer, pTest->buffer_size);
                    if (pTest->buffer == NULL)
                    {
                        DBG_ERROR(DBG_CTX, "%s - Unable to allocate device buffer of %u bytes\n", pTest->name, pTest->buffer_size);
                        pTest->buffer_size = 0;
                    }
                }
            }
            else if (_CMD_MATCH("curve?"))
            {
                int      n_digits;
                uint32_t rl;
                char     num_buf[16];
                uint8_t *pv;

                n_digits = sprintf_s(num_buf, sizeof(num_buf), "%d", pTest->rec_length);
                ptest_append_response(pTest, "#%d%d", n_digits, pTest->rec_length);
                pv = &(pTest->buffer[pTest->buffer_length]);
                for (rl = 0; rl < pTest->rec_length; rl++)
                {
                    *(pv++) = 0x80;
                }
                pTest->buffer_length += pTest->rec_length;

                pTest->flags |= F_WAS_CURVE;
            }
            else if (_CMD_MATCH_N("srq "))
            {
                int     x;
                int     nEvents;
                int     delay;

                nEvents = (int) strtoul(&(bp[4]), &p, 0);
                delay = (int) strtoul(p, NULL, 0);

                DBG_PRINT(DBG_CTX, "Generating %d SRQ events with a delay of %dms\n", nEvents, delay);
                for (x = 0; x < nEvents; x++)
                {
                    if (delay > 0)
                    {
                        usleep(delay);
                    }
                    if (pTest->dese & _ESE_OPC)
                    {
                        ptest_queue_event(pTest, _ESR_OPC, "402,\"Operation complete\"");

                        if ((pTest->sre & _SRE_ESB) &&
                            (pTest->ese & pTest->esr))
                        {
                            ptest_srq_event(pTest);
                        }
                    }
                }
            }
            else if (_CMD_MATCH_N("ldata "))
            {
                //DWG ldata <size> <value> <inc>?
                uint32_t size;
                uint8_t val;
                int     inc;

                size = strtoul(&(bp[6]), &p, 0);
                val = (uint8_t) (strtoul(p, &p, 0) & 0x0FF);
                inc = (int) strtol(p, &p, 0);

                DBG_PRINT(DBG_CTX, "Generating 0x%08X bytes of data start:0x%02X increment:%d\n", size, val, inc);
                if ((size + 10) > pTest->buffer_size)
                {
                    uint8_t *tbuf;

                    tbuf = pTest->buffer;

                    pTest->buffer = (uint8_t *) malloc(size + 10);
                    if (pTest->buffer != NULL)
                    {
                        pTest->buffer_size = size + 10;
                        free(tbuf);
                    }
                    else
            {
                        DBG_WARNING(DBG_CTX, "Unable to allocate buffer of %u bytes\n", size + 10);
                        pTest->buffer = tbuf;
            }
        }

                if (size < pTest->buffer_size)
        {
                    uint32_t x;
                    uint8_t *pv;

                    ptest_append_response(pTest, "#%08X", size);
                    pv = &(pTest->buffer[pTest->buffer_length]);
                    for (x = 0; x < size; x++)
            {
                        *(pv++) = (val & 0x0FF);
                        val += inc;
            }
                    pTest->buffer_length += size;
        }
                else
        {
                    ptest_append_response(pTest, "Unable to allocate buffer of 0x%08X bytes", size + 10);
                }
        }
            else
        {
                /* Unknown command */

                /* Find the end of the header */
                for (p = bp; ((*p != '\0') && !isspace(*p) && (*p != ':')); p++); p[0] = '\0';

                ptest_queue_event(pTest, _ESR_CME, "113,\"Undefined header; Command not found; %s\"", bp);
                if (pTest->dese & _ESE_CME)
                {
                    if ((pTest->sre & _SRE_ESB) &&
                        (pTest->ese & pTest->esr))
                    {
                        ptest_srq_event(pTest);
                    }
                }
            }
#undef _CMD_MATCH_N
#undef _CMD_MATCH

            bp = ep;
        }

        if (pTest->buffer_length != 0)
        {
            if (pTest->stb & _STB_MAV)
            {
                ptest_queue_event(pTest, _ESR_QYE, "410,\"Query INTERRUPTED\"");

                ptest_output_flush(pTest);
        }

            pTest->buffer[pTest->buffer_length++] = '\n';

            // Assert the events that there is data available.
            pTest->stb |= _STB_MAV;
            if ((pTest->sre & _SRE_ESB) &&
                (pTest->ese & pTest->esr))
        {
                ptest_srq_event(pTest);
            }
        }
    }
    else
        {
        pTest->write_buffer = buffer;
        buffer = NULL;
        }

    status = 0;

e_ptest_write:
    if (buffer != NULL)
        {
        free(buffer);
        buffer = NULL;
        }

    return status;
}

/****************************************************************************
 * usbTmcEventHandler
 */
static void
usbTmcEventHandler(const USBTMC488_MESSAGE *msgIn)
        {
    test_t                     *pTest = g_test;
    static USBTMC488_MESSAGE    deviceMsg;
    static USBTMC488_MESSAGE    freeMsg;
    static USBTMC488_MSG_BUFFER deviceMsgBuf;
    USBTMC488_STATUS            deviceStatus = USBTMC488_SUCCESS;

    switch (msgIn->type)
    {
    case USBTMC488_MSG_DEVICE_CLEAR:        // Handle device clear message
        DBG_PRINT(DBG_CTX, "******** %s ********\n", usbtmc488_msg_type_to_string(msgIn->type));

        ptest_device_clear(pTest);

        // Notify the gadget driver that the device clear process is complete.
        memset(&deviceMsg, 0, sizeof(deviceMsg));
        deviceMsg.type = USBTMC488_MSG_DEVICE_CLEAR_DONE;
        deviceMsg.value = 0;
        deviceMsg.msg_buffer = NULL;
        if ((deviceStatus = usbtmc488_message(&deviceMsg)) != USBTMC488_SUCCESS)
        {
            DBG_ERROR(DBG_CTX, "%s sending msg type %s to gadget\n",
                usbtmc488_status_to_string(deviceStatus),
                usbtmc488_msg_type_to_string(deviceMsg.type));
        }
        break;

    case USBTMC488_MSG_DEVICE_TRIGGER:  // Handle group execute trigger message
        DBG_PRINT(DBG_CTX, "******** %s ********\n", usbtmc488_msg_type_to_string(msgIn->type));
        break;

    case USBTMC488_MSG_DATA_REQUESTED:
    {
        uint32_t    bytesAvail;

        DBG_PRINT(DBG_CTX, "******** %s ********\n", usbtmc488_msg_type_to_string(msgIn->type));

        bytesAvail = pTest->buffer_length - pTest->buffer_offset;
        if (bytesAvail == 0)
        {
            if (!(pTest->esr & _ESR_QYE))
            {
                pTest->esr |= _ESR_QYE;
                if ((pTest->sre & _SRE_ESB) &&
                    (pTest->ese & pTest->esr))
        {
                    ptest_srq_event(pTest);
        }
            }
        }
        else
        {
            uint32_t    nBytes = 0;
            uint32_t    bufSize;

            bufSize = msgIn->msg_buffer->length;

            memset(&deviceMsg, 0, sizeof(deviceMsg));

            if (bufSize >= bytesAvail)
        {
                nBytes += bytesAvail;

                deviceMsg.type = USBTMC488_MSG_DATA_WITH_EOM;
                deviceMsgBuf.buffer = &(pTest->buffer[pTest->buffer_offset]);
                deviceMsgBuf.length = nBytes;

                pTest->buffer_offset = pTest->buffer_length = 0;
                pTest->stb &= ~_STB_MAV;
        }
            else
        {
                nBytes += bufSize;

                deviceMsg.type = USBTMC488_MSG_DATA_WITHOUT_EOM;
                deviceMsgBuf.buffer = &(pTest->buffer[pTest->buffer_offset]);
                deviceMsgBuf.length = nBytes;

                pTest->buffer_offset += nBytes;
            }

            deviceMsg.value = 0;
            deviceMsg.msg_buffer = &deviceMsgBuf;
            if ((deviceStatus = usbtmc488_message(&deviceMsg)) != USBTMC488_SUCCESS)
            {
                DBG_ERROR(DBG_CTX, "%s sending msg type %s to gadget\n",
                    usbtmc488_status_to_string(deviceStatus),
                    usbtmc488_msg_type_to_string(deviceMsg.type));
            }
        }
        }
        break;

    case USBTMC488_MSG_DATA_WITHOUT_EOM: // Handle data message
    case USBTMC488_MSG_DATA_WITH_EOM:
        DBG_PRINT(DBG_CTX, "******** %s ********\n", usbtmc488_msg_type_to_string(msgIn->type));

        if (ptest_write(pTest, msgIn->msg_buffer->buffer, msgIn->msg_buffer->length, msgIn->type == USBTMC488_MSG_DATA_WITH_EOM))
        {
            DBG_PRINT(DBG_CTX, "%s: pTest write failed!\n", __FUNCTION__);
            break;
        }

//DWG_XXX
        DBG_PRINT(DBG_CTX, "STB:0x%02X ESE:0x%02X DESE:0x%02X SRE:0x%02X\n", pTest->stb, pTest->ese, pTest->dese, pTest->sre);
        if ((msgIn->type == USBTMC488_MSG_DATA_WITH_EOM) &&
             (pTest->stb & _STB_RQS) &&
             (pTest->ese & _ESE_CME) && (pTest->dese & _ESE_CME) && (pTest->sre & _ESE_CME))
        {
            deviceMsg.type = USBTMC488_MSG_REQUEST_SERVICE;
            if ((deviceStatus = usbtmc488_message(&deviceMsg)) != USBTMC488_SUCCESS)
            {
                DBG_ERROR(DBG_CTX, "%s sending msg type %s to gadget\n",
                    usbtmc488_status_to_string(deviceStatus),
                    usbtmc488_msg_type_to_string(deviceMsg.type));
            }
        }

        break;

    default:
//DWG USBTMC488_MSG_UPDATE_STATUS_BYTE = 5,
//DWG USBTMC488_MSG_REQUEST_SERVICE    = 6,
//DWG USBTMC488_MSG_GOTO_LOCAL         = 7,
//DWG USBTMC488_MSG_GOTO_REMOTE        = 8,
//DWG USBTMC488_MSG_CANCEL_IO          = 9,
//DWG USBTMC488_MSG_DEVICE_CLEAR_DONE  = 10,
//DWG USBTMC488_MSG_FREE_REQUEST_MSG   = 11,
        // Handle invalid message
        DBG_WARNING(DBG_CTX, "******** %s ******** - Unhandled message type!\n", usbtmc488_msg_type_to_string(msgIn->type));
        break;
    }

    // Notify the gadget driver that we're done with the request
    // message.
    memset(&freeMsg, 0, sizeof(freeMsg));
    freeMsg.type = USBTMC488_MSG_FREE_REQUEST_MSG;
    if ((deviceStatus = usbtmc488_message(&freeMsg)) != USBTMC488_SUCCESS)
    {
        DBG_ERROR(DBG_CTX, "%s sending msg type %s to gadget\n",
            usbtmc488_status_to_string(deviceStatus),
            usbtmc488_msg_type_to_string(freeMsg.type));
    }
}

/****************************************************************************
 * gadget_callback
 */
static void
gadget_callback(const USBTMC488_MESSAGE *msg)
{
    usbTmcEventHandler(msg);
}

/****************************************************************************
 * onAlarm
 */
void onAlarm(
    int one
    )
{
    test_t     *pTest;

    for (pTest = g_test; pTest != NULL; pTest = pTest->next)
{
        if ((pTest->sre & _SRE_ESB) &&
            (pTest->ese & pTest->esr))
        {
            ptest_srq_event(pTest);
        }
    }
}

/****************************************************************************
 * main
 */
int
main(
    int     argc,
    char   *argv[]
    )
{
    int     status = 0;

    USBTMC488_DEVICE_INFO gadgetDeviceInfo;
    char                  majorVer[CFG_VERSION_STRING_LEN + 1] = "01";
    char                  minorVer[CFG_VERSION_STRING_LEN + 1] = "00";
    uint8_t               minorBcd, majorBcd;
    USBTMC488_STATUS      deviceStatus;
    test_t               *pTest = NULL;

    printf("usbtmc488_test version %s\n", pTestVersion);

    gadgetDeviceInfo.vendorId = CFG_USBTMC_VENDOR_ID;
    gadgetDeviceInfo.productId = CFG_USBTMC_PRODUCT_ID;

    majorBcd = (majorVer[0] - '0') << 4;
    majorBcd |= majorVer[1] - '0';
    minorBcd = (minorVer[0] - '0') << 4;
    minorBcd |= minorVer[1] - '0';
    gadgetDeviceInfo.productVersion = (minorBcd | (majorBcd << 8));
    strncpy(gadgetDeviceInfo.manufacturerDescr,
            CFG_USBTMC_MFG_DESCR,
            USBTMC488_DEVINFO_STRING_BUFSIZE);
    strncpy(gadgetDeviceInfo.productDescr,
            CFG_MODEL_ID_STRING,
            USBTMC488_DEVINFO_STRING_BUFSIZE);
    strncpy(gadgetDeviceInfo.productSerialNumberDescr,
            CFG_SERIAL_NUMBER_STRING,
            USBTMC488_DEVINFO_STRING_BUFSIZE);
    strncpy(gadgetDeviceInfo.configurationDescr,
            CFG_USBTMC_CFG_DESCR,
            USBTMC488_DEVINFO_STRING_BUFSIZE);
    strncpy(gadgetDeviceInfo.interfaceDescr,
            CFG_USBTMC_IFC_DESCR,
            USBTMC488_DEVINFO_STRING_BUFSIZE);
    gadgetDeviceInfo.maxPower = CFG_USBTMC_MAXPOWER;
    gadgetDeviceInfo.selfPowered = CFG_USBTMC_SELFPOWERED;

    pTest = ptest_open("Port#0");
    if (pTest == NULL)
    {
        status = -1;
        goto e_main;
    }
#ifndef WIN32
    signal(SIGALRM, onAlarm);
#endif /* WIN32 */

    usbtmc488_set_verbosity(USBTMC488_API | USBTMC488_EP_BULKIN | USBTMC488_EP_BULKOUT |
                            USBTMC488_EP_CONTROL | USBTMC488_EP_INTRPT | USBTMC488_EVENTS |
                            USBTMC488_SYNCH | USBTMC488_SYSTEM | USBTMC488_THREADS);

    deviceStatus =
        usbtmc488_enable_interface(
            &gadgetDeviceInfo,
            &gadget_callback);
    if (deviceStatus != USBTMC488_SUCCESS)
    {
        DBG_ERROR(DBG_CTX, "usbtmc488_enable_interface() failed!\n"
            "Status returned from gadget driver: %s\n",
            usbtmc488_status_to_string(deviceStatus));
    }
    DBG_PRINT(DBG_CTX, "STANDALONE TEST APPLICATION IS RUNNING\n");

    while (1)
    {
        usleep(100);
    }

e_main:
    if (pTest != NULL)
    {
#ifndef WIN32
        signal(SIGALRM, SIG_DFL);
#endif /* WIN32 */

        ptest_close(pTest);
        pTest = NULL;
}

    return status;
}
