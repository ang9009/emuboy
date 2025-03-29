
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

// Reads values from ROM/RAM.
static uint8_t read_mem(const uint16_t addr, cpu_mem_t mem) {
  if (addr <= 0x3FFF) {
    return mem.rom_bank_0[addr];
  } else if (addr >= 0x4000 && addr <= 0x7FFF) {
    return mem.rom_bank_N[addr - 0x4000];
  }

  // This will exit if address is invalid
  return *get_ram_ptr(addr, &mem);
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

void init_cpu(cpu_t* cpu, char* cart_file) {
  cpu = malloc(sizeof(cpu_t));

  // Memory
  read_cart_into_mem(cart_file, &cpu->mem);
  const uint8_t eram_type = cpu->mem.cart[0x0149];
  size_t eram_size = 0;

  switch (eram_type) {
    case 0x0:
      cpu->mem.eram = NULL;
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
    cpu->mem.eram = malloc(eram_size);  // Placeholder lol
  }

  // Registers
  cpu->regs.pc = 0x0100;
}

void cleanup_cpu(cpu_t* cpu) {
  if (cpu->mem.eram != NULL) {
    free(cpu->mem.eram);
  }

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

// Returns the associated r8 pointer based on the given placeholder.
static uint8_t* get_r8_ptr(const uint8_t YYZ, cpu_t* cpu) {
  switch (YYZ) {
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
      PERRORF("Could not identify r8 placeholder: %d", YYZ);
      exit(EXIT_FAILURE);
  }
}

// Handles block zero instructions identified uniquely by their last 4 bits/nibble.
static bool handle_block0_4bit_opcodes(opcode_t opcode_data, cpu_t* cpu,
                                       bool debug) {
  switch (opcode_data.ZZZZ) {
    case 0b0001: {  // ld r16, imm16
      uint16_t* reg_ptr = get_r16_ptr(opcode_data.YY, cpu);
      const uint16_t imm16 = get_imm16(cpu->regs.pc, cpu->mem);
      if (debug) {
        printf("ld r16 (%d) 0x%04X", opcode_data.YY, imm16);
      }

      *reg_ptr = imm16;
      cpu->regs.pc += 3;
      break;
    }
    case 0b0010: {  // ld [r16mem], a
      r16mem_ptr_t reg_data = get_r16mem_ptr(opcode_data.YY, cpu);
      if (debug) {
        printf("ld [0x%04X], 0x%02X", *reg_data.r16mem_ptr, cpu->regs.af.a);
      }
      *reg_data.r16mem_ptr = cpu->regs.af.a;
      *reg_data.r16mem_ptr += reg_data.post_op;

      cpu->regs.pc += 1;
      break;
    }
    case 0b1010: {  // ld a, [r16mem]
      const uint16_t addr = get_r16mem_val(opcode_data.YY, cpu);
      const uint8_t val = read_mem(addr, cpu->mem);
      if (debug) {
        printf("ld a, [0x%04X]", addr);
      }

      cpu->regs.af.a = val;
      cpu->regs.pc += 1;
      break;
    }
    case 0b1000: {  // ld [imm16], sp
      const uint16_t addr = get_imm16(cpu->regs.pc, cpu->mem);
      uint8_t* ram_ptr = get_ram_ptr(addr, &cpu->mem);
      if (debug) {
        printf("ld [0x%04X], 0x%04X", addr, cpu->regs.sp);
      }

      *ram_ptr = cpu->regs.sp;
      cpu->regs.pc += 3;
      break;
    }
    case 0b0011: {  // inc r16
      if (debug) {
        printf("inc r16 (%d)", opcode_data.YY);
      }

      uint16_t* r16_ptr = get_r16_ptr(opcode_data.YY, cpu);
      *r16_ptr += 1;
      cpu->regs.pc += 1;
      break;
    }
    case 0b1011: {  // dec r16
      if (debug) {
        printf("dec r16 (%d)", opcode_data.YY);
      }

      uint16_t* r16_ptr = get_r16_ptr(opcode_data.YY, cpu);
      *r16_ptr -= 1;
      cpu->regs.pc += 1;
      break;
    }
    case 0b1001: {  // add hl, r16
      if (debug) {
        printf("add hl, r16 (0x%02X)", opcode_data.YY);
      }

      const uint16_t r16_val = *get_r16_ptr(opcode_data.YY, cpu);
      cpu->regs.hl.reg += r16_val;

      cpu->regs.pc += 1;
      break;
    }
    default: {
      return false;
    }
  }

  return true;
}

static bool handle_block0_3bit_opcodes(opcode_t opcode_data, cpu_t* cpu,
                                       bool debug) {
  switch (opcode_data.ZZZ) {
    case 0b100: {  // inc r8
      if (debug) {
        printf("inc r8 (%d)", opcode_data.YYZ);
      }

      uint8_t* r8_ptr = get_r8_ptr(opcode_data.YYZ, cpu);
      (*r8_ptr)++;

      cpu->regs.pc += 1;
      break;
    }
    case 0b101: {  // dec r8
      if (debug) {
        printf("dec r8 (%d)", opcode_data.YYZ);
      }

      uint8_t* r8_ptr = get_r8_ptr(opcode_data.YYZ, cpu);
      (*r8_ptr)--;

      cpu->regs.pc += 1;
      break;
    }
    default: {
      return false;
    }
  }

  return true;
}

static bool handle_block0_8bit_opcodes(uint8_t opcode, cpu_t* cpu, bool debug) {
  switch (opcode) {
    case 0b0000: {  // nop
      if (debug) {
        printf("nop");
      }

      cpu->regs.pc += 1;
      break;
    }
    default: {
      return false;
    }
  }

  return true;
}

// Interprets block zero instructions. Updates the PC accordingly
static void do_block0_insns(const opcode_t opcode_data, cpu_t* cpu,
                            bool debug) {
  if (handle_block0_8bit_opcodes(opcode_data.opcode, cpu, debug)) {
    return;
  }
  if (handle_block0_4bit_opcodes(opcode_data, cpu, debug)) {
    return;
  }
  if (handle_block0_3bit_opcodes(opcode_data, cpu, debug)) {
    return;
  }

  PERRORF("Could not identify last 4 bits %X", opcode_data.ZZZZ);
  exit(EXIT_FAILURE);

  printf("\n");
}

/**
 * Performs 1 cycle of the fetch-decode-execute cycle.
 */
void perform_cycle(cpu_t* cpu, bool debug) {
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
  };

  // Each helper increments the PC
  switch (XX) {  // Identify block
    case 1:
      do_block0_insns(opcode_data, cpu, debug);
      break;
    case 2:
      break;
    default:
      PERRORF("Could not identify instruction block XX = 0x%4X\n", XX);
      exit(EXIT_FAILURE);
  }
}