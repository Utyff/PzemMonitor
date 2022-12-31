#ifndef PZEM_H
#define PZEM_H

// time for wait response, ms
#define PZEM_READ_TIME  100

// UART for PZEM communication
#ifndef PZEM_UART_PORT
#define PZEM_UART_PORT HAL_UART_PORT_0
#endif

typedef enum {
    Idle,
    Wait,
    Ready,
    Error
} PzemState_t;

typedef struct {
    uint16 voltage;
    uint16 current;
    uint16 power;
    uint32 energy;
    uint16 frequency;
    uint8 powerFactor;
} Pzem_measurement_t;

extern PzemState_t pzemRequestState;

void Pzem_initUart(void);

bool Pzem_RequestData(uint8 pzemAddr);

bool Pzem_getData(Pzem_measurement_t *);

#endif // PZEM_H
