
#include "../include/cpu.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/utils.h"

// Consider the opcode's bits as XXYYZZZZ
typedef struct {
  uint8_t YY : 2;
  uint8_t ZZZZ : 4;
} opcode_t;

bool read_file_into_rom(char* file_path, uint8_t rom[]) {
  FILE* file = fopen(file_path, "r");
  if (file == NULL) {
    perror("Could not find file");
    return false;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    perror("Failed to find end of file");
    return false;
  }

  const long file_size = ftell(file);
  if (file_size == -1) {
    perror("Couldn't locate current file pointer");
    return false;
  } else if (file_size > ROM_SIZE) {
    // ! Implement MBC1 later for other tests
    perror("Provided file is too big");
    return false;
  }
  rewind(file);

  if ((long)fread(&rom[0x0100], file_size, 1, file) < file_size) {
    perror("Could not read complete file contents into ROM");
    return false;
  }

  fclose(file);
  return true;
}

void init_cpu(cpu_t* cpu) {
  cpu = malloc(sizeof(cpu_t));
  cpu->regs = (cpu_regs_t){0};
}

/**
 * Translates r16 placeholder to register value pointer. Returns NULL if YY 
 * is not recognized
 */
uint16_t* get_r16(uint8_t YY, cpu_t* cpu) {
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
      return NULL;
  }
}

bool get_mem_ptr(uint8_t* ptr, uint16_t addr) {}

/**
 * Translates r16mem placeholder to register value. Returns true if YY
 * is not recognized, and false otherwise. This modifies the HL register if necessary 
 * (HL+ or HL-).
 */
bool get_r16mem(uint8_t YY, cpu_t* cpu, uint16_t* out) {
  switch (YY) {
    case 0:
      *out = cpu->regs.bc.reg;
      break;
    case 1:
      *out = cpu->regs.de.reg;
      break;
    case 2:
      *out = cpu->regs.hl.reg++;
      break;
    case 3:
      *out = cpu->regs.hl.reg--;
      break;
    default:
      PERRORF("Could not identify YY value when getting r16mem: %d", YY);
      return false;
  }

  return true;
}

/**
 * Fetches the immediate 16 bits after the opcode address given in big-endian. Returns 0
 * if successful, or -1 if out of bounds. This assumes that the opcode is 8 bits long, and 
 * that the address was originally stored in little-endian.
 */
int get_imm16(const uint16_t op_addr, const cpu_t cpu, uint16_t* out) {
  if (op_addr + 2 >= ROM_SIZE) {
    perror("Attempted to access immediate 16 bits outside of ROM");
    return -1;
  }

  *out = (cpu.rom[op_addr + 1] << 8) | cpu.rom[op_addr + 2];
  return 0;
}

// Interprets block zero instructions. Updates the PC accordingly
bool do_block_zero_insns(const opcode_t opcode_data, cpu_t* cpu, bool debug) {
  switch (opcode_data.ZZZZ) {
    case 0b0000:
      if (debug) {
        printf("nop");
      }

      cpu->regs.pc += 1;
      break;
    case 0b0001:
      uint16_t* reg_ptr = get_r16(opcode_data.YY, cpu);
      if (reg_ptr == NULL) {
        return false;
      }
      uint16_t imm16;
      if (get_imm16(cpu->regs.pc, *cpu, &imm16) != 0) {
        return false;
      }

      if (debug) {
        printf("ld r16 (%d) 0x%04X", opcode_data.YY, imm16);
      }

      *reg_ptr = imm16;
      cpu->regs.pc += 3;
      break;
    case 0b0010:
      uint16_t addr;
      if (!get_r16mem(opcode_data.YY, cpu, &addr)) {
        return false;
      }
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
  const uint8_t opcode = cpu->rom[cpu->regs.pc];

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
      PERRORF("Could not identify instruction block XX = 0x%4X\n", XX);
      return false;
  }

  return true;
}