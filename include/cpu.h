#ifndef CPU_H_INCLUDED
#define CPU_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#define ROM_SIZE 0x7FFF
#define VRAM_SIZE 8000
#define WRAM_SIZE 8000

/**
 * CPU registers
 */

// Flags struct for the AF register
typedef union {
  struct {
    uint8_t : 4;  // Upper 4 bits ignored
    uint8_t z : 1;
    uint8_t n : 1;
    uint8_t h : 1;
    uint8_t c : 1;
  };
  uint8_t reg;
} flags_reg_t;

typedef union {
  struct {
    uint8_t a;
    flags_reg_t f;
  };
  uint16_t reg;
} af_reg_t;

typedef union {
  struct {
    uint8_t b;
    uint8_t c;
  };
  uint16_t reg;
} bc_reg_t;

typedef union {
  struct {
    uint8_t d;
    uint8_t e;
  };
  uint16_t reg;
} de_reg_t;

typedef union {
  struct {
    uint8_t h;
    uint8_t l;
  };
  uint16_t reg;
} hl_reg_t;

typedef struct {
  af_reg_t af;
  bc_reg_t bc;
  de_reg_t de;
  hl_reg_t hl;
  uint16_t sp;  // Stack pointer
  uint16_t pc;  // Program counter
} cpu_regs_t;

/**
 * CPU
 */
typedef struct {
  cpu_regs_t regs;
  uint8_t vram[VRAM_SIZE];
  uint8_t wram[WRAM_SIZE];
  uint8_t rom[ROM_SIZE];
} cpu_t;

/**
 * Helper functions
 */
bool read_file_into_rom(char* file_path, uint8_t* rom);

void init_cpu(cpu_t* cpu);

#endif