#include <stdint.h>

/**
 * CPU registers
 */

// Flags struct for the AF register
typedef union {
    struct {
        uint8_t : 4; // Upper 4 bits ignored
        uint8_t Z: 1;
        uint8_t N: 1;
        uint8_t H: 1;
        uint8_t C: 1;
    };
    uint8_t reg;
} flags_reg_t;

typedef union {
    struct {
        uint8_t A;
        flags_reg_t F;
    };
    uint16_t reg;
} af_reg_t;

typedef union {
    struct {
        uint8_t B;
        uint8_t C;
    };
    uint16_t reg;
} bc_reg_t;

typedef union {
    struct {
        uint8_t D;
        uint8_t E;
    };
    uint16_t reg;
} de_reg_t;

typedef union {
    struct {
        uint8_t H;
        uint8_t L;
    };
    uint16_t reg;
} hl_reg_t;


typedef struct {
    af_reg_t AF;
    bc_reg_t BC;
    de_reg_t DE;
    hl_reg_t HL;
    uint16_t SP;
    uint16_t PC;
} cpu_regs_t;

/**
 * CPU
 */
typedef struct {
    cpu_regs_t regs;
} cpu_t;