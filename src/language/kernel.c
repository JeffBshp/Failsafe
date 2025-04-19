#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "compiler.h"
#include "../hardware/cpu.h"

// insert an instruction with 3 register operands
static void Instr3(uint16_t *ptr, uint16_t opCode, uint16_t regA, uint16_t regB, uint16_t comp, uint16_t regC)
{
	Instruction *i = (void *)ptr;
	i->opCode = opCode;
	i->regA = regA;
	i->regB = regB;
	i->comp = comp;
	i->regC = regC;
}

// insert an instruction with 2 register operands
static void Instr2(uint16_t *ptr, uint16_t opCode, uint16_t regA, uint16_t regB, uint16_t immed7)
{
	Instruction *i = (void *)ptr;
	i->opCode = opCode;
	i->regA = regA;
	i->regB = regB;
	i->immed7 = immed7;
}

// insert an instruction with 1 register operand
static void Instr1(uint16_t *ptr, uint16_t opCode, uint16_t regA, uint16_t immed10)
{
	Instruction *i = (void *)ptr;
	i->opCode = opCode;
	i->regA = regA;
	i->immed10 = immed10;
}

// insert two instructions that load a full 16-bit immediate word into a register
static void LoadFullWord(uint16_t *ptr, uint16_t reg, uint16_t immed16)
{
	Instr1(ptr, INSTR_LUI, reg, immed16 >> 6);
	Instr2(ptr + 1, INSTR_ADDI, reg, reg, immed16 & 0x003f);
}

// This initializes memory with the IDT, ISRs, boot procedure, kernel, demo program, and stdlib.
uint16_t Kernel_Load(uint16_t *m)
{
	const uint16_t maxStackSize = 256;
	const uint16_t bootStart = 1024;
	const uint16_t kernelAddress = bootStart + 512;
	const uint16_t demoAddress = bootStart + 1024;
	const uint16_t libAddress = bootStart + 1536;

	// compile the "kernel" program and its dependency
	// TODO: automatically load dependencies
	Program *kernel = Compiler_BuildFile("res/code/kernel.txt");
	Program *demo = Compiler_BuildFile("res/code/demo.tmp");
	Program *lib = Compiler_BuildFile("res/code/stdlib.txt");
	if (kernel == NULL || demo == NULL || lib == NULL) return 0xffff;

	int kernelMain = kernelAddress + kernel->mainAddress;
	int kernelStack = kernelAddress + kernel->length;
	int kernelSize = kernel->length + maxStackSize;
	int demoMain = demoAddress + demo->mainAddress;
	int demoStack = demoAddress + demo->length;
	int demoSize = demo->length + maxStackSize;
	int libSize = lib->length;
	// rather than search by function name, I know at compile time the index of this function
	int switchTaskAddress = libAddress + lib->functions[7].offset;

	printf("\nKernel Main: %d\n", kernelMain);
	printf("Demo Main: %d\n", demoMain);
	printf("MoveTheBall: %d\n", demoAddress + demo->functions[1].offset);
	printf("Sleep: %d\n", libAddress + lib->functions[0].offset);
	printf("PrintStr: %d\n", libAddress + lib->functions[1].offset);
	printf("PrintInt: %d\n", libAddress + lib->functions[2].offset);
	printf("BreakBlock: %d\n", libAddress + lib->functions[3].offset);
	printf("ChangeVelocity: %d\n", libAddress + lib->functions[4].offset);
	printf("SwitchTask: %d\n\n", switchTaskAddress);

	// the binary begins with space for addresses of referenced modules
	// insert the address of the library
	kernel->bin[0] = libAddress;
	demo->bin[0] = libAddress;

	// load modules into memory
	memcpy(m + kernelAddress, kernel->bin, kernel->length * sizeof(uint16_t));
	memcpy(m + demoAddress, demo->bin, demo->length * sizeof(uint16_t));
	memcpy(m + libAddress, lib->bin, lib->length * sizeof(uint16_t));

	Compiler_Destroy(kernel);
	Compiler_Destroy(demo);
	Compiler_Destroy(lib);

	// interrupt descriptor table
	// each entry is just the address of an ISR
	// the ISRs begin at address 16 and each occupies 16 words
	for (int i = 0; i < 16; i++)
		m[i] = (i + 1) * 16;

	// the first interrupt service routine is for switching tasks
	// it gets triggered by a timer
	Instr2(m + 16, INSTR_ADDI, REG_RESULT, REG_STACK_PTR, 0);		// copy SP of interrupted task to another register
	LoadFullWord(m + 17, REG_STACK_PTR, 8192);						// this isr has its own stack
	Instr2(m + 19, INSTR_ADDI, REG_FRAME_PTR, REG_STACK_PTR, 0);	// set FP so the function can access its variables
	Instr2(m + 20, INSTR_EXT, OPX_PUSH, REG_RESULT, 0);				// push arg: SP of interrupted task
	LoadFullWord(m + 21, REG_MODULE_PTR, libAddress);				// load MP for the module we're about to call into
	LoadFullWord(m + 23, REG_RESULT, switchTaskAddress);			// load function address
	Instr2(m + 25, INSTR_EXT, OPX_CALL, REG_RESULT, 10);			// call function (and allocate locals on stack)
	// Now the SP of the interrupted task has been saved to its process control block (PCB).
	// RR contains the return value, which is the address of the new task's PCB.
	Instr1(m + 26, INSTR_BEZ, REG_RESULT, 2);						// if no task returned, branch ahead and do not execute IRET
	Instr2(m + 27, INSTR_LW, REG_STACK_PTR, REG_RESULT, 4);			// load SP of new task
	Instr2(m + 28, INSTR_EXT, OPX_MORE, OPXX_IRET, 0);				// resume the new task and pop its registers from its own stack
	Instr2(m + 29, INSTR_EXT, OPX_MORE, OPXX_IEN, 1);				// enable interrupts
	Instr2(m + 30, INSTR_EXT, OPX_MORE, OPXX_HALT, 0);				// halt until an interrupt causes a task to resume
	Instr1(m + 31, INSTR_BEZ, REG_ZERO, -2);						// other interrupts return here; go back to halt

	// this ISR just sends an input character to be printed as a string,
	// showing how the system can handle multiple interrupt types
	LoadFullWord(m + 32, REG_OPERAND_A, 360);						// address of the input character
	LoadFullWord(m + 34, REG_OPERAND_B, 328);						// address for string output
	Instr2(m + 36, INSTR_SW, REG_OPERAND_A, REG_OPERAND_B, 0);		// copy char pointer to string output
	Instr2(m + 37, INSTR_EXT, OPX_MORE, OPXX_IRET, 0);				// resume

	// the rest of the ISRs
	// these just return (IRET) without taking action
	for (int i = 48; i <= 256; i += 16)
		Instr2(m + i, INSTR_EXT, OPX_MORE, OPXX_IRET, 0);

	// pcb / memory allocation records
	// (entries begin at 520)
	m[512] = 3;					// number of entries
	m[513] = kernelAddress;		// current task
	m[514] = 520;				// PCB of current task

	// main kernel task
	m[520] = kernelAddress;					// module address
	m[521] = kernelSize;					// memory allocation size
	m[522] = 0;								// allocation type (0=task, 1=library, 2=heap)
	m[523] = 0;								// number of dependent modules
	m[524] = kernelStack;					// stack pointer
	m[525] = 0;								// reserved
	m[526] = 0;								// timeout high bits
	m[527] = 0;								// timeout low bits
	m[528] = 0;								// reserved
	m[529] = 0;								// reserved
	m[530] = 0;								// reserved
	m[531] = 1;								// privileged flag

	// demo program
	m[532] = demoAddress;
	m[533] = demoSize;
	// 534 - 535
	m[536] = demoStack + 7; // 6 initial register values + IP are pushed at the start as explained below

	// Demo task is not initially running. It will begin after an IRET instruction from the task switcher procedure.
	// IRET pops registers as if resuming a suspended task, so initial values need to be placed on the stack.
	m[demoStack + 4] = demoAddress; // MP
	m[demoStack + 5] = demoStack; // FP
	m[demoStack + 6] = demoMain; // IP

	// stdlib module
	m[544] = libAddress;
	m[545] = libSize;
	m[546] = 1; // type == library
	m[547] = 2; // 2 other modules depend on this one

	// bootstrap procedure: execution starts here
	uint16_t *s = m + bootStart;
	LoadFullWord(s + 0, REG_MODULE_PTR, kernelAddress);				// MP points to the kernel module
	LoadFullWord(s + 2, REG_RESULT, kernelMain);					// get address of first function to call
	LoadFullWord(s + 4, REG_STACK_PTR, kernelStack);				// initialize SP
	Instr2(s + 6, INSTR_ADDI, REG_FRAME_PTR, REG_STACK_PTR, 0);		// FP starts out equal to SP
	Instr2(s + 7, INSTR_ADDI, REG_RET_ADDR, REG_ZERO, 0);			// initialize RA (boot procedure never returns anyway)
	Instr2(s + 8, INSTR_EXT, OPX_MORE, OPXX_IEN, 1);				// enable interrupts
	Instr2(s + 9, INSTR_EXT, OPX_CALL, REG_RESULT, 2);				// kernel entry point
	Instr2(s + 10, INSTR_EXT, OPX_MORE, OPXX_HALT, 0);				// if kernel ever exits, halt
	Instr1(s + 11, INSTR_BEZ, REG_ZERO, -2);						// interrupt returns here; go back to halt

	return bootStart;
}
