#ifndef DAL_PDU_MODBUS_PORT_H_
#define DAL_PDU_MODBUS_PORT_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <assert.h>

#define INLINE inline
#define PR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C
#define ENTER_CRITICAL_SECTION() BSP_Modbus_EnterCritical()
#define EXIT_CRITICAL_SECTION() BSP_Modbus_ExitCritical()

typedef char BOOL;
typedef unsigned char UCHAR;
typedef char CHAR;
typedef unsigned short USHORT;
typedef short SHORT;
typedef unsigned long ULONG;
typedef long LONG;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

void BSP_Modbus_EnterCritical(void);
void BSP_Modbus_ExitCritical(void);

#endif /* DAL_PDU_MODBUS_PORT_H_ */
