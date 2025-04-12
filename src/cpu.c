
#include "../include/cpu.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/utils.h"

// Consider the opcode's bits as XXYYZZZZ, where XX is the opcode
typedef struct {
  uint8_t opcode;
  uint8_t YY : 2;
  uint8_t ZZZZ : 4;
  uint8_t YYZ : 3;  // Middle 3 bits
  uint8_t ZZZ : 3;  // Last 3 bits
} opcode_t;

// Taken from https://github.com/deltabeard/gameboy-c
static const uint8_t OP_CYCLES[0x100] = {
    //   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    4,  12, 8,  8,  4,  4,  8,  4,  20, 8, 8,  8, 4,  4,  8, 4,   // 0x00
    4,  12, 8,  8,  4,  4,  8,  4,  8,  8, 8,  8, 4,  4,  8, 4,   // 0x10
    8,  12, 8,  8,  4,  4,  8,  4,  8,  8, 8,  8, 4,  4,  8, 4,   // 0x20
    8,  12, 8,  8,  12, 12, 12, 4,  8,  8, 8,  8, 4,  4,  8, 4,   // 0x30
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4, 4,  4, 4,  4,  8, 4,   // 0x40
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4, 4,  4, 4,  4,  8, 4,   // 0x50
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4, 4,  4, 4,  4,  8, 4,   // 0x60
    8,  8,  8,  8,  8,  8,  4,  8,  4,  4, 4,  4, 4,  4,  8, 4,   // 0x70
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4, 4,  4, 4,  4,  8, 4,   // 0x80
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4, 4,  4, 4,  4,  8, 4,   // 0x90
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4, 4,  4, 4,  4,  8, 4,   // 0xA0
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4, 4,  4, 4,  4,  8, 4,   // 0xB0
    8,  12, 12, 12, 12, 16, 8,  32, 8,  8, 12, 8, 12, 12, 8, 32,  // 0xC0
    8,  12, 12, 0,  12, 16, 8,  32, 8,  8, 12, 0, 12, 0,  8, 32,  // 0xD0
    12, 12, 8,  0,  0,  16, 8,  32, 16, 4, 16, 0, 0,  0,  8, 32,  // 0xE0
    12, 12, 8,  4,  0,  16, 8,  32, 12, 8, 16, 4, 0,  0,  8, 32   // 0xF0
};

// Returns the number of t-cycles an instruction takes if it prefixed by 0xCB
static uint8_t get_prefixed_insn_cycles(uint8_t opcode) {
  switch (opcode) {
    case 0x06:
    case 0x16:
    case 0x26:
    case 0x36:
    case 0x86:
    case 0x96:
    case 0xA6:
    case 0xB6:
    case 0xC6:
    case 0xD6:
    case 0xE6:
    case 0xF6:
    case 0x0E:
    case 0x1E:
    case 0x2E:
    case 0x3E:
    case 0x8E:
    case 0x9E:
    case 0xAE:
    case 0xBE:
    case 0xCE:
    case 0xDE:
    case 0xEE:
    case 0xFE:
      return 16;

    case 0x46:
    case 0x56:
    case 0x66:
    case 0x76:
    case 0x4E:
    case 0x5E:
    case 0x6E:
    case 0x7E:
      return 12;

    default:
      return 8;
  }
}

/**
 * Returns the total length of the instruction given the opcode. This does not handle 0xCB 
 * prefixed instructions.
 */
static uint8_t get_unprefixed_insn_length(uint8_t opcode) {
  switch (opcode) {
    // 3-byte instructions
    case 0x01:  // LD BC, d16
    case 0x11:  // LD DE, d16
    case 0x21:  // LD HL, d16
    case 0x31:  // LD SP, d16
    case 0x08:  // LD (u16),SP
    case 0xC2:  // JP NZ,u16
    case 0xC3:  // JP u16
    case 0xC4:  // CALL NZ,u16
    case 0xCA:  // JP Z,u16
    case 0xCC:  // CALL Z,u16
    case 0xCD:  // CALL u16
    case 0xD2:  // JP NC,u16
    case 0xD4:  // CALL NC,u16
    case 0xDA:  // JP C,u16
    case 0xDC:  // CALL C,u16
    case 0xEA:  // LD (u16),A
    case 0xFA:  // LD A,(u16)
      return 3;

    // 2-byte instructions
    case 0x20:
    case 0x30:
    case 0xE0:
    case 0xF0:
    case 0x06:
    case 0x16:
    case 0x26:
    case 0x36:
    case 0xC6:
    case 0xD6:
    case 0xE6:
    case 0xF6:
    case 0x18:
    case 0x28:
    case 0x38:
    case 0xE8:
    case 0xF8:
    case 0x0E:
    case 0x1E:
    case 0x2E:
    case 0x3E:
    case 0xCE:
    case 0xDE:
    case 0xEE:
    case 0xFE:
      return 2;

    // 1-byte instructions
    default:
      return 1;
  }
}

// Provides a pointer to position in RAM. Exits if address is invalid memory location.
static uint8_t* get_ram_ptr(const uint16_t addr, cpu_mem_t* mem) {
  if (addr >= 0x8000 && addr <= 0x9FFF) {
    return &mem->vram[addr - 0x8000];
  } else if (mem->eram != NULL && addr >= 0xA000 && addr <= 0xBFFF) {
    return &mem->eram[addr - 0xA000];
  } else if (addr >= 0xC000 && addr <= 0xDFFF) {
    return &mem->wram[addr - 0xC000];
  } else if (addr >= 0xFE00 && addr <= 0xFE9F) {
    return &mem->oam[addr - 0xFE00];
  } else if (addr >= 0xFF00 && addr <= 0xFF7F) {
    return &mem->io_regs[addr - 0xFF00];
  } else if (addr >= 0xFF80 && addr <= 0xFFFE) {
    return &mem->hram[addr - 0xFF80];
  } else if (addr == 0xFFFF) {
    return &mem->ie;
  }

  PERRORF("Attempted to access invalid memory location 0x%04X", addr);
  exit(EXIT_FAILURE);
}

// Returns a pointer to the flags struct of the given cpu
static flags_reg_t* get_flags_ptr(cpu_t* cpu) {
  return &cpu->regs.af.f;
}

// Reads 8 bit values from ROM/RAM.
static uint8_t read_mem(const uint16_t addr, cpu_mem_t mem) {
  if (addr <= 0x3FFF) {
    return mem.rom_bank_0[addr];
  } else if (addr >= 0x4000 && addr <= 0x7FFF) {
    return mem.rom_bank_N[addr - 0x4000];
  }

  // This will exit if address is invalid
  return *get_ram_ptr(addr, &mem);
}

// Updates the PC by the length of the current instruction, and updates the cycle count
void update_cpu(uint8_t opcode, cpu_t* cpu) {
  if (opcode == 0xCB) {
    uint8_t prefixed_opcode = read_mem(cpu->regs.pc + 1, cpu->mem);
    cpu->regs.pc += 2;
    cpu->cycles += get_prefixed_insn_cycles(prefixed_opcode);
  } else {
    uint8_t pc_inc = get_unprefixed_insn_length(opcode);
    cpu->regs.pc += pc_inc;
    cpu->cycles += OP_CYCLES[opcode];
  }
}

// Reads the cart into memory banks and the cart member
void read_cart_into_mem(char* file_path, cpu_mem_t* cpu_mem) {
  FILE* file = fopen(file_path, "rb");
  if (file == NULL) {
    perror("Could not find file");
    exit(EXIT_FAILURE);
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    perror("Failed to find file size");
    exit(EXIT_FAILURE);
  }

  const long file_size = ftell(file);
  if (file_size == -1) {
    fclose(file);
    perror("Failed to find file size");
    exit(EXIT_FAILURE);
  }
  rewind(file);

  // Clear out rom banks first
  memset(cpu_mem->rom_bank_N, 0, ROM_BANK_SIZE);
  memset(cpu_mem->rom_bank_0, 0, ROM_BANK_SIZE);
  cpu_mem->cart = calloc(1, file_size);

  if (fread(cpu_mem->cart, file_size, 1, file) != 1) {
    fclose(file);
    perror("Failed to read cartridge into memory");
    exit(EXIT_FAILURE);
  }
  fclose(file);

  const size_t bank0_size =
      file_size > ROM_BANK_SIZE ? ROM_BANK_SIZE : file_size;
  memcpy(cpu_mem->rom_bank_0, cpu_mem->cart, bank0_size);

  if (file_size > ROM_BANK_SIZE) {
    size_t remainder = file_size - ROM_BANK_SIZE;
    const size_t bankN_size =
        remainder < ROM_BANK_SIZE ? remainder : ROM_BANK_SIZE;
    memcpy(cpu_mem->rom_bank_N, cpu_mem->cart + bank0_size, bankN_size);
  }
}

// Allocates memory for and sets up the cpu struct at the given pointer
void init_cpu(cpu_t** cpu, char* cart_file) {
  *cpu = calloc(1, sizeof(cpu_t));
  cpu_t* cpu_ptr = *cpu;
  cpu_ptr->halt = false;

  // Memory
  read_cart_into_mem(cart_file, &cpu_ptr->mem);
  const uint8_t eram_type = cpu_ptr->mem.cart[0x0149];
  size_t eram_size = 0;

  switch (eram_type) {
    case 0x0:
      cpu_ptr->mem.eram = NULL;
      break;
    case 0x2:
      eram_size = 8 * 1000;
      break;
    case 0x3:
      eram_size = 32 * 1000;
      break;
    case 0x4:
      eram_size = 128 * 1000;
      break;
    case 0x5:
      eram_size = 64 * 1000;
      break;
    default:
      PERRORF("Unexpected SRAM/ERAM type: 0x%04X", eram_type);
      exit(EXIT_FAILURE);
  }
  if (eram_size != 0) {
    cpu_ptr->mem.eram = malloc(eram_size);  // Placeholder lol
  }

  // Registers
  cpu_ptr->regs.pc = 0x0100;
}

void cleanup_cpu(cpu_t* cpu) {
  if (cpu->mem.eram != NULL) {
    free(cpu->mem.eram);
  }

  free(cpu->mem.cart);
  free(cpu);
}

/**
 * Translates r16 placeholder to register value pointer. Exits if YY 
 * is not recognized
 */
static uint16_t* get_r16_ptr(uint8_t YY, cpu_t* cpu) {
  switch (YY) {
    case 0:
      return &cpu->regs.bc.reg;
    case 1:
      return &cpu->regs.de.reg;
    case 2:
      return &cpu->regs.hl.reg;
    case 3:
      return &cpu->regs.sp;
    default:
      PERRORF("Could not identify YY (%X) for r16 placeholder", YY);
      exit(EXIT_FAILURE);
  }
}

typedef struct {
  uint16_t* r16mem_ptr;  // Pointer to the r16mem register
  int post_op;  // The value the register should be changed by after the operation
} r16mem_ptr_t;

/**
 * Translates r16mem placeholder to register pointer. It is the caller's responsibility
 * to update the register's value once the register has been accessed according to the
 * return value.
 */
static r16mem_ptr_t get_r16mem_ptr(uint8_t YY, cpu_t* cpu) {
  uint16_t* reg_ptr = NULL;
  int post_op = 0;

  switch (YY) {
    case 0:
      reg_ptr = &cpu->regs.bc.reg;
      break;
    case 1:
      reg_ptr = &cpu->regs.de.reg;
      break;
    case 2:
      reg_ptr = &cpu->regs.hl.reg;
      post_op = 1;
      break;
    case 3:
      reg_ptr = &cpu->regs.hl.reg;
      post_op = -1;
      break;
    default:
      PERRORF("Could not identify YY value when getting r16mem: %d", YY);
      exit(EXIT_FAILURE);
  }

  return (r16mem_ptr_t){
      .r16mem_ptr = reg_ptr,
      .post_op = post_op,
  };
}

/**
 * Translates r16mem placeholder to register value. Returns true if YY
 * is not recognized, and false otherwise. This modifies the HL register if necessary 
 * (HL+ or HL-).
 */
static uint16_t get_r16mem_val(uint8_t YY, cpu_t* cpu) {
  r16mem_ptr_t reg_data = get_r16mem_ptr(YY, cpu);
  uint16_t val = *reg_data.r16mem_ptr;
  *reg_data.r16mem_ptr += reg_data.post_op;

  return val;
}

/**
 * Fetches the immediate 16 bits after the opcode address given in big-endian. Returns 0
 * if successful, or -1 if out of bounds. This assumes that the opcode is 8 bits long, and 
 * that the address was originally stored in little-endian.
 */
static uint16_t get_imm16(const uint16_t op_addr, const cpu_mem_t mem) {
  // Upper and lower in big endian
  uint8_t lower = read_mem(op_addr + 1, mem);
  uint8_t upper = read_mem(op_addr + 2, mem);

  return (upper << 8) | lower;
}

/**
 * Returns the associated r8 pointer based on the given placeholder. "bits" should be a 
 * 3 bit value.
 * */

static uint8_t* get_r8_ptr(const uint8_t bits, cpu_t* cpu) {
  switch (bits) {
    case 0:
      return &cpu->regs.bc.b;
    case 1:
      return &cpu->regs.bc.c;
    case 2:
      return &cpu->regs.de.d;
    case 3:
      return &cpu->regs.de.e;
    case 4:
      return &cpu->regs.hl.h;
    case 5:
      return &cpu->regs.hl.l;
    case 6:
      const uint16_t hl = cpu->regs.hl.reg;
      return get_ram_ptr(hl, &cpu->mem);
    case 7:
      return &cpu->regs.af.a;
    default:
      PERRORF("Could not identify r8 placeholder: %d", bits);
      exit(EXIT_FAILURE);
  }
}

// Handles block zero instructions identified uniquely by their last 4 bits/nibble.
static bool handle_block0_4bit_opcodes(opcode_t opcode_data, cpu_t* cpu) {
  switch (opcode_data.ZZZZ) {
    case 0b0001: {  // ld r16, imm16
      uint16_t* reg_ptr = get_r16_ptr(opcode_data.YY, cpu);
      const uint16_t imm16 = get_imm16(cpu->regs.pc, cpu->mem);
      DBG_PRINT("ld r16 (%d) 0x%04X", opcode_data.YY, imm16);

      *reg_ptr = imm16;
      break;
    }
    case 0b0010: {  // ld [r16mem], a
      r16mem_ptr_t reg_data = get_r16mem_ptr(opcode_data.YY, cpu);
      DBG_PRINT("ld [0x%04X], 0x%02X", *reg_data.r16mem_ptr, cpu->regs.af.a);
      *reg_data.r16mem_ptr = cpu->regs.af.a;
      *reg_data.r16mem_ptr += reg_data.post_op;
      break;
    }
    case 0b1010: {  // ld a, [r16mem]
      const uint16_t addr = get_r16mem_val(opcode_data.YY, cpu);
      const uint8_t val = read_mem(addr, cpu->mem);
      DBG_PRINT("ld a, [0x%04X]", addr);
      cpu->regs.af.a = val;
      break;
    }
    case 0b1000: {  // ld [imm16], sp
      const uint16_t addr = get_imm16(cpu->regs.pc, cpu->mem);
      uint8_t* ram_ptr = get_ram_ptr(addr, &cpu->mem);
      DBG_PRINT("ld [0x%04X], 0x%04X", addr, cpu->regs.sp);
      *ram_ptr = cpu->regs.sp;
      break;
    }
    case 0b0011: {  // inc r16
      DBG_PRINT("inc r16 (%d)", opcode_data.YY);
      uint16_t* r16_ptr = get_r16_ptr(opcode_data.YY, cpu);
      *r16_ptr += 1;
      break;
    }
    case 0b1011: {  // dec r16
      DBG_PRINT("dec r16 (%d)", opcode_data.YY);
      uint16_t* r16_ptr = get_r16_ptr(opcode_data.YY, cpu);
      *r16_ptr -= 1;
      break;
    }
    case 0b1001: {  // add hl, r16
      DBG_PRINT("add hl, r16 (0x%02X)", opcode_data.YY);
      const uint16_t r16_val = *get_r16_ptr(opcode_data.YY, cpu);
      const bool set_h =
          ((cpu->regs.hl.reg & 0xFFF) + (r16_val & 0xFFF)) > 0xFFF;
      const bool set_c = cpu->regs.hl.reg + r16_val > 0xFFFF;
      cpu->regs.hl.reg += r16_val;

      flags_reg_t* flags = &cpu->regs.af.f;
      flags->n = 0;
      flags->h = (int)set_h;
      flags->c = (int)set_c;
      break;
    }
    default: {
      return false;
    }
  }

  return true;
}

// Gets the immediate 8 bit value after the opcode at the given address
static int8_t get_imm8(const uint16_t op_addr, const cpu_mem_t mem) {
  return read_mem(op_addr + 1, mem);
}

// Checks if the given condition is met. cond should not be more than 2 bits wide.
static bool is_cond_met(const uint8_t cond, const cpu_t cpu) {
  flags_reg_t flags = cpu.regs.af.f;
  switch (cond) {
    case 0: {  // nz
      return flags.z == 0;
    }
    case 1: {  // z
      return flags.z == 1;
    }
    case 2: {  // nc
      return flags.c == 0;
    }
    case 3: {  // c
      return flags.c == 1;
    }
    default:  // Should not happen (unless cond is more than 2 bits wide)
      exit(EXIT_FAILURE);
  }
}

static bool handle_block0_3bit_opcodes(opcode_t opcode_data, cpu_t* cpu) {
  flags_reg_t* flags = get_flags_ptr(cpu);

  switch (opcode_data.ZZZ) {
    case 0b100: {  // inc r8
      DBG_PRINT("inc r8 (%d)", opcode_data.YYZ);
      uint8_t* r8_ptr = get_r8_ptr(opcode_data.YYZ, cpu);
      bool set_h = (*r8_ptr & 0xF) ==
                   0xF;  // If lower nibble is 0xF, there will be a carry

      (*r8_ptr)++;

      flags->z = (*r8_ptr == 0) ? 1 : 0;
      flags->n = 0;
      flags->h = (int)set_h;
      break;
    }
    case 0b101: {  // dec r8
      DBG_PRINT("dec r8 (%d)", opcode_data.YYZ);
      uint8_t* r8_ptr = get_r8_ptr(opcode_data.YYZ, cpu);
      bool set_h = ((*r8_ptr) & 0xF) == 0;
      (*r8_ptr)--;

      flags->z = (*r8_ptr == 0) ? 1 : 0;
      flags->n = 1;
      flags->h = (int)set_h;
      break;
    }
    case 0b110: {  // ld r8, imm8
      const uint8_t imm8 = get_imm8(cpu->regs.pc, cpu->mem);
      DBG_PRINT("ld r8 (%d), 0x%02X", opcode_data.YYZ, imm8);
      uint8_t* r8 = get_r8_ptr(opcode_data.YYZ, cpu);
      *r8 = imm8;

      break;
    }
    case 0b000: {  // jr cond, imm8
      const uint8_t cond = (opcode_data.opcode >> 3) & 0b11;
      if (is_cond_met(cond, *cpu)) {
        const int8_t imm8 = get_imm8(cpu->regs.pc, cpu->mem);
        cpu->regs.pc += imm8;
      }
      break;
    }
    default: {
      return false;
    }
  }

  return true;
}

// Handles opcodes uniquely identified by all 8 bits in block 0
static bool handle_block0_8bit_opcodes(uint8_t opcode, cpu_t* cpu) {
  flags_reg_t* flags = get_flags_ptr(cpu);

  switch (opcode) {
    case 0x0: {  // nop
      DBG_PRINT("nop");
      break;
    }
    case 0x07:    // rlca
    case 0x0F: {  // rrca
      uint8_t carry_bit;
      if (opcode == 0x07) {
        DBG_PRINT("rlca");
        carry_bit = (cpu->regs.af.a >> 7) & 0b1;  // Get MSB
        cpu->regs.af.a <<= 1;
      } else {
        DBG_PRINT("rrca");
        carry_bit = (cpu->regs.af.a) & 0b1;  // Get LSB
        cpu->regs.af.a >>= 1;
      }

      flags->z = 0;
      flags->n = 0;
      flags->h = 0;
      flags->c = carry_bit;
      break;
    }
    case 0x17:    // rla
    case 0x1F: {  // rra
      uint8_t carry_bit;
      if (opcode == 0x17) {
        DBG_PRINT("rla");
        carry_bit = (cpu->regs.af.a >> 7) & 0b1;  // Get MSB
        cpu->regs.af.a <<= 1;
        cpu->regs.af.a |= flags->c;  // OR with LSB (empty spot)
      } else {
        DBG_PRINT("rra");
        carry_bit = (cpu->regs.af.a) & 0b1;  // Get LSB
        cpu->regs.af.a >>= 1;
        cpu->regs.af.a |= (flags->c << 7);  // OR with MSB (empty spot)
      }

      // Update flags
      flags->z = 0;
      flags->n = 0;
      flags->h = 0;
      flags->c = carry_bit;
      break;
    }
    case 0x27: {  // daa
      DBG_PRINT("daa");
      uint8_t adj = 0;
      if (flags->n) {
        if (flags->h) {
          adj += 0x6;
        }
        if (flags->c) {
          adj += 0x60;
        }

        cpu->regs.af.a -= adj;
      } else {
        if (flags->h || (cpu->regs.af.a & 0xF) > 0x9) {
          adj += 0x6;
        }
        if (flags->c || cpu->regs.af.a > 0x99) {
          adj += 0x60;
          flags->c = 1;
        }

        cpu->regs.af.a += adj;
      }

      flags->z = (cpu->regs.af.a) == 0 ? 1 : 0;
      flags->h = 0;
      break;
    }
    case 0x2F: {  // cpl
      DBG_PRINT("cpl");
      cpu->regs.af.a = ~cpu->regs.af.a;

      flags->n = 1;
      flags->h = 1;
      break;
    }
    case 0x37: {  // scf
      DBG_PRINT("scf");
      flags->n = 0;
      flags->h = 0;
      flags->c = 1;
      break;
    }
    case 0x3F: {  // ccf
      DBG_PRINT("ccf");
      flags->n = 0;
      flags->h = 0;
      flags->c = ~flags->c;
      break;
    }
    case 0x18: {                                             // jr imm8
      const int8_t imm8 = get_imm8(cpu->regs.pc, cpu->mem);  // Signed value
      DBG_PRINT("jr 0x%04X", imm8);
      cpu->regs.pc += imm8;
      break;
    }
    case 0x10: {  // stop
      DBG_PRINT("stop");
      cpu->halt = true;
      break;
    }
    default: {
      return false;
    }
  }

  return true;
}

// Interprets block zero instructions. Updates the PC accordingly
static void do_block0_insns(const opcode_t opcode_data, cpu_t* cpu) {
  if (handle_block0_8bit_opcodes(opcode_data.opcode, cpu)) {
    return;
  } else if (handle_block0_4bit_opcodes(opcode_data, cpu)) {
    return;
  } else if (handle_block0_3bit_opcodes(opcode_data, cpu)) {
    return;
  }
}

// Handles all block 1 instructions
static void do_block1_insns(const opcode_t opcode_data, cpu_t* cpu) {
  if (opcode_data.opcode == 0x76) {  // halt
    return;
  }

  // ld r8, r8
  uint8_t* dest = get_r8_ptr(opcode_data.YYZ, cpu);
  uint8_t* src = get_r8_ptr(opcode_data.ZZZ, cpu);
  *dest = *src;
}

/**
 * Performs 1 cycle of the fetch-decode-execute cycle.
 */
void perform_cycle(cpu_t* cpu) {
  uint8_t opcode = read_mem(cpu->regs.pc, cpu->mem);

  // Consider the opcode's bits as XXYYZZZZ
  uint8_t XX = (opcode >> 6);
  uint8_t YY = (opcode >> 4) & 0b11;
  uint8_t ZZZZ = (opcode) & 0xFl;
  const opcode_t opcode_data = {
      .YY = YY,
      .ZZZZ = ZZZZ,
      .YYZ = (YY << 1) | ((opcode >> 3) & 0b1),
      .ZZZ = ZZZZ >> 1,
      .opcode = opcode,
  };

  // Each helper increments the PC
  switch (XX) {  // Identify block
    case 1:
      do_block0_insns(opcode_data, cpu);
      break;
    case 2:
      break;
    default:
      update_cpu(0, cpu);
      return;
  }

  update_cpu(opcode, cpu);
}