#ifndef CPU_H_INCLUDED
#define CPU_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#define ROM_BANK_SIZE 0x8000
#define VRAM_SIZE 0x2000
#define WRAM_SIZE 0x2000
#define OAM_SIZE 0xA0
#define IO_REGS_SIZE 0x80
#define HRAM_SIZE 0x7F

/**
 * CPU registers
 */
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

typedef struct {
  uint8_t rom_bank_0[ROM_BANK_SIZE];
  uint8_t rom_bank_N[ROM_BANK_SIZE];
  uint8_t* cart;  // The entire cartridge
  uint8_t vram[VRAM_SIZE];
  uint8_t wram[WRAM_SIZE];
  uint8_t*
      eram;  // External ram from cartridsssge for savestates. Set to NULL if not available,
  uint8_t oam[WRAM_SIZE];
  uint8_t io_regs[IO_REGS_SIZE];
  uint8_t hram[HRAM_SIZE];
  uint8_t ie;  // Interrupt enable register
} cpu_mem_t;

/**
 * CPU
 */
typedef struct {
  cpu_regs_t regs;  // Registers
  cpu_mem_t mem;    // Memory regions
} cpu_t;

/**
 * Helper functions
 */

//  Reads a cartridge file into ROM
void read_file_into_rom(char* file_path, cpu_t* cpu);

// Initializes the CPU
void init_cpu(cpu_t* cpu, char* cart_file);

// Frees memory related to the CPU
void cleanup_cpu(cpu_t* cpu);

// Performs 1 iteration of the fetch-decode-execute cycle
void perform_cycle(cpu_t* cpu, bool debug);

#endif