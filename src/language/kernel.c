#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "compiler.h"
#include "../hardware/processor.h"

static void Instr3(uint16_t *ptr, uint16_t opCode, uint16_t regA, uint16_t regB, uint16_t comp, uint16_t regC)
{
	Instruction *i = (void *)ptr;
	i->opCode = opCode;
	i->regA = regA;
	i->regB = regB;
	i->comp = comp;
	i->regC = regC;
}

static void Instr2(uint16_t *ptr, uint16_t opCode, uint16_t regA, uint16_t regB, uint16_t immed7)
{
	Instruction *i = (void *)ptr;
	i->opCode = opCode;
	i->regA = regA;
	i->regB = regB;
	i->immed7 = immed7;
}

static void Instr1(uint16_t *ptr, uint16_t opCode, uint16_t regA, uint16_t immed10)
{
	Instruction *i = (void *)ptr;
	i->opCode = opCode;
	i->regA = regA;
	i->immed10 = immed10;
}

static void LoadFullWord(uint16_t *ptr, uint16_t reg, uint16_t immed16)
{
	Instr1(ptr, INSTR_LUI, reg, immed16 >> 6);
	Instr2(ptr + 1, INSTR_ADDI, reg, reg, immed16 & 0x003f);
}

// This initializes memory with the IDT, ISRs, boot procedure, and kernel.
uint16_t Kernel_Load(uint16_t *m)
{
	const uint16_t stackStart = 4096;
	const uint16_t bootStart = 1024;
	const uint16_t kernelAddress = bootStart + 64;
	uint16_t *s = m + bootStart;

	// compile the "kernel" program and its dependency
	// TODO: automatically load dependencies
	Program *kernel = Compiler_BuildFile("res/code/kernel.tmp");
	Program *lib = Compiler_BuildFile("res/code/stdlib.txt");
	if (kernel == NULL || lib == NULL) return 0xffff;

	// load modules into memory
	int mainAddress = kernelAddress + kernel->mainAddress;
	printf("Kernel main location: %d\n", mainAddress);
	kernel->bin[0] = bootStart + 512;
	memcpy(s + 64, kernel->bin, kernel->length * sizeof(uint16_t));
	memcpy(s + 512, lib->bin, lib->length * sizeof(uint16_t));
	Compiler_Destroy(kernel);
	Compiler_Destroy(lib);

	// interrupt descriptor table
	// each entry is just the address of an isr
	// there are 16 irqs but not all have an entry
	m[3] = 32;
	m[7] = 64;
	m[9] = 96;

	// interrupt service routines
	// these just return without taking action
	Instr2(m + 32, INSTR_EXT, OPX_MORE, OPXX_IRET, 0);
	Instr2(m + 64, INSTR_EXT, OPX_MORE, OPXX_IRET, 0);
	Instr2(m + 96, INSTR_EXT, OPX_MORE, OPXX_IRET, 0);

	// bootstrap procedure: execution starts here
	LoadFullWord(s + 0, REG_MODULE_PTR, kernelAddress);				// MP points to the kernel module
	LoadFullWord(s + 2, REG_RESULT, mainAddress);					// get address of first function to call
	LoadFullWord(s + 4, REG_STACK_PTR, stackStart);					// initialize SP
	Instr2(s + 6, INSTR_ADDI, REG_FRAME_PTR, REG_STACK_PTR, 0);		// FP starts out equal to SP
	Instr2(s + 7, INSTR_ADDI, REG_RET_ADDR, REG_ZERO, 0);			// initialize RA (boot procedure never returns anyway)
	Instr2(s + 8, INSTR_EXT, OPX_MORE, OPXX_IEN, 1);				// enable interrupts
	Instr2(s + 9, INSTR_EXT, OPX_PUSH, REG_FRAME_PTR, 0);			// push FP
	Instr2(s + 10, INSTR_EXT, OPX_PUSH, REG_RESULT, 0);				// push function address
	Instr2(s + 11, INSTR_EXT, OPX_CALL, REG_RESULT, 0);				// kernel entry point
	Instr2(s + 12, INSTR_EXT, OPX_POP, REG_RESULT, 0);				// pop function address
	Instr2(s + 13, INSTR_EXT, OPX_POP, REG_FRAME_PTR, 0);			// pop FP
	Instr2(s + 14, INSTR_EXT, OPX_MORE, OPXX_HALT, 0);				// if kernel ever exits, halt until interrupt and then loop
	Instr1(s + 15, INSTR_BEZ, REG_ZERO, -7);						// interrupt returns here; go back to kernel

	return bootStart;
}
