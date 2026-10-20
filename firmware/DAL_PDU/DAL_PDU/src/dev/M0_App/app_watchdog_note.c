#include "app_watchdog_note.h"

#ifdef __AVR__
#define NOTE_SECTION __attribute__((section(".noinit")))
#else
#define NOTE_SECTION
#endif

#define NOTE_MAGIC 0xC7U
#define NOTE_KEY 0x5AU

typedef struct
{
    uint8_t magic;
    uint8_t source;
    uint8_t check;
} AppWatchdogNote_Record;

static AppWatchdogNote_Record s_note NOTE_SECTION;

void AppWatchdogNote_Set(uint8_t source)
{
    s_note.source = source;
    s_note.check = (uint8_t)(source ^ NOTE_KEY);
    s_note.magic = NOTE_MAGIC; /* Written last, so a reset mid-write leaves the note invalid. */
}

bool AppWatchdogNote_Take(uint8_t *source)
{
    bool valid = s_note.magic == NOTE_MAGIC &&
                 s_note.check == (uint8_t)(s_note.source ^ NOTE_KEY);
    if (valid && source != 0) *source = s_note.source;
    s_note.magic = 0U;
    return valid;
}
