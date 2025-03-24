
#include "cpu.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

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

  if (fread(rom[0x0100], file_size, 1, file) < file_size) {
    perror("Could not read complete file contents into ROM");
    return false;
  }
  fclose(file);
}

void init_cpu(cpu_t* cpu, bool debug) {
  cpu = malloc(sizeof(cpu_t));
  cpu->regs = (cpu_regs_t){0};
}

// Translates r16 placeholder to register value
int get_r16(uint8_t YY) {}

// Interprets block zero instructions. Updates the PC accordingly
bool do_block_zero_insns(const opcode_t opcode_data, cpu_t* cpu, bool debug) {
  switch (opcode_data.ZZZZ) {
    case 0b0000:
      if (debug) {
        printf("nop");
        cpu->regs.pc += 1;
        break;
      }
    case 0b0001:
      if (debug) {
        printf("ld %X", opcode_data.YY);
      }
      // ! LITTLE ENDIAN
      // ! I need a Makefile??
      break;
  }
}

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
        perror("Could not recognize instructions in block 0");
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