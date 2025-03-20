#include <stdint.h>

typedef struct {
    uint8_t A; // Accumulator register
    uint8_t B;
    uint8_t C;
    uint8_t D;
    uint8_t E;
    uint8_t H;
    uint8_t L;
} registers_t;

typedef struct {
    uint8_t Z;
    uint8_t N;
    uint8_t H;
    uint8_t C;
} flags_t;

typedef struct {
    registers_t registers;
    uint16_t SP;
    uint16_t PC;
} cpu_t;