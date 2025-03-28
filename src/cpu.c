
#include "../include/cpu.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/utils.h"

// Consider the opcode's bits as XXYYZZZZ, where XX is the opcode
typedef struct {
  uint8_t YY : 2;
  uint8_t ZZZZ : 4;
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

  read_cart_into_mem(cart_file, &cpu->mem);
  const uint8_t eram_type = cpu->mem.cart[0x0149];
  int eram_size = 0;

  // ! Incomplete
  switch (eram_type) {
    case 0x0:
      cpu->mem.eram = NULL;
      break;
    case 0x2:
      eram_size = 8 * 1000;
      break;
  }
  if (eram_size != 0) {
    cpu->mem.eram = malloc(eram_size);  // Placeholder lol
  }
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

/**
 * Translates r16mem placeholder to register value. Returns true if YY
 * is not recognized, and false otherwise. This modifies the HL register if necessary 
 * (HL+ or HL-).
 */
static uint16_t get_r16mem(uint8_t YY, cpu_t* cpu) {
  switch (YY) {
    case 0:
      return cpu->regs.bc.reg;
      break;
    case 1:
      return cpu->regs.de.reg;
      break;
    case 2:
      return cpu->regs.hl.reg++;
      break;
    case 3:
      return cpu->regs.hl.reg--;
      break;
    default:
      PERRORF("Could not identify YY value when getting r16mem: %d", YY);
      exit(EXIT_FAILURE);
  }

  return true;
}

/**
 * Fetches the immediate 16 bits after the opcode address given in big-endian. Returns 0
 * if successful, or -1 if out of bounds. This assumes that the opcode is 8 bits long, and 
 * that the address was originally stored in little-endian.
 */
static uint16_t get_imm16(const uint16_t op_addr, cpu_mem_t mem) {
  // Upper and lower in big endian
  uint8_t lower = read_mem(op_addr + 1, mem);
  uint8_t upper = read_mem(op_addr + 2, mem);

  return (upper << 8) | lower;
}

// Interprets block zero instructions. Updates the PC accordingly
static bool do_block_zero_insns(const opcode_t opcode_data, cpu_t* cpu,
                                bool debug) {
  switch (opcode_data.ZZZZ) {
    case 0b0000:
      if (debug) {
        printf("nop");
      }

      cpu->regs.pc += 1;
      break;
    case 0b0001:
      uint16_t* reg_ptr = get_r16_ptr(opcode_data.YY, cpu);
      if (reg_ptr == NULL) {
        return false;
      }
      uint16_t imm16 = get_imm16(cpu->regs.pc, cpu->mem);

      if (debug) {
        printf("ld r16 (%d) 0x%04X", opcode_data.YY, imm16);
      }

      *reg_ptr = imm16;
      cpu->regs.pc += 3;
      break;
    case 0b0010:
      uint16_t addr = get_r16mem(opcode_data.YY, cpu);
      (void)addr;
      break;
    default:
      printf("Could not identify last 4 bits %X", opcode_data.ZZZZ);
      return false;
  }

  printf("\n");
  return true;
}

/**
 * Performs 1 cycle of the fetch-decode-execute cycle.
 */
bool perform_cycle(cpu_t* cpu, bool debug) {
  uint8_t opcode = read_mem(cpu->regs.pc, cpu->mem);

  // Consider the opcode's bits as XXYYZZZZ
  uint8_t XX = (opcode >> 6);
  const opcode_t opcode_data = {
      .YY = (opcode >> 4) & 0b11,
      .ZZZZ = (opcode) & 0xF,
  };

  // Each helper increments the PC
  switch (XX) {  // Identify block
    case 1:
      if (!do_block_zero_insns(opcode_data, cpu, debug)) {
        return false;
      }
      break;
    case 2:
      break;
    default:
      fprintf(stderr, "Could not identify instruction block XX = 0x%4X\n", XX);
      return false;
  }

  return true;
}