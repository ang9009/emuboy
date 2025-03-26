#ifndef CPU_H_INCLUDED
#define CPU_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#define ROM_BANK_SIZE 0x8000
#define VRAM_SIZE 0x2000
#define WRAM_SIZE 0x2000
#define OAM_SIZE 0xA0

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

#define REGISTER_STRUCT(x, y) \
  typedef union {             \
    struct {                  \
      uint8_t x;              \
      uint8_t y;              \
    };                        \
    uint16_t reg;             \
  } x##y##_reg_t;

#define REGISTERS       \
  REGISTER_STRUCT(a, f) \
  REGISTER_STRUCT(b, c) \
  REGISTER_STRUCT(d, e) \
  REGISTER_STRUCT(h, l)

REGISTERS
#undef REGISTER_STRUCT
#undef REGISTERS

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
  // Memory regions
  // ! Need new helper for ROM memory access (read-only)
  // ! Also can set up the registers using X-Macro
  uint8_t rom_bank_0[ROM_BANK_SIZE];
  uint8_t rom_bank_N[ROM_BANK_SIZE];
  uint8_t vram[VRAM_SIZE];
  uint8_t wram[WRAM_SIZE];
  uint8_t oam[WRAM_SIZE];
  // ! Missing remaining regions
} cpu_t;

/**
 * Helper functions
 */
bool read_file_into_rom(char* file_path, uint8_t* rom);

void init_cpu(cpu_t* cpu);

#endif