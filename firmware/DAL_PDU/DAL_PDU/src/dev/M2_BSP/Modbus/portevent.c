#include "port.h"

#include "../../M4_MiddleWare/ThirdParty/freemodbus/modbus/include/mbport.h"

#define MB_EVENT_CAPACITY 4U

static volatile eMBEventType s_events[MB_EVENT_CAPACITY];
static volatile uint8_t s_head;
static volatile uint8_t s_tail;
static volatile uint8_t s_count;

BOOL xMBPortEventInit(void)
{
    ENTER_CRITICAL_SECTION();
    s_head = 0U;
    s_tail = 0U;
    s_count = 0U;
    EXIT_CRITICAL_SECTION();
    return TRUE;
}

BOOL xMBPortEventPost(eMBEventType event)
{
    BOOL posted = FALSE;
    ENTER_CRITICAL_SECTION();
    if (s_count < MB_EVENT_CAPACITY)
    {
        s_events[s_head] = event;
        s_head = (uint8_t)((s_head + 1U) % MB_EVENT_CAPACITY);
        s_count++;
        posted = TRUE;
    }
    EXIT_CRITICAL_SECTION();
    return posted;
}

BOOL xMBPortEventGet(eMBEventType *event)
{
    BOOL found = FALSE;
    if (event == 0) return FALSE;
    ENTER_CRITICAL_SECTION();
    if (s_count != 0U)
    {
        *event = s_events[s_tail];
        s_tail = (uint8_t)((s_tail + 1U) % MB_EVENT_CAPACITY);
        s_count--;
        found = TRUE;
    }
    EXIT_CRITICAL_SECTION();
    return found;
}
