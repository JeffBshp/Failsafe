#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "compiler.h"
#include "../hardware/cpu.h"
#include "../hardware/device.h"
#include "../hardware/memory.h"

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

// insert an entry in the module registry
static void RegisterModule(uint16_t *ptr, uint16_t pcbAddr, char *name)
{
	*ptr = pcbAddr;
	ptr++;

	int i = 0;
	uint16_t pair = 0;

	do {
		pair = name[i++];

		if (pair != 0)
		{
			pair |= name[i++] << 8;
		}

		*ptr = pair;
		ptr++;
	}
	while (i <= 60 && (pair & 0xff00) != 0);
}

// insert an entry in the list of memory allocations / process control blocks
static void AddMemoryEntry(uint16_t *ptr, uint16_t address, uint16_t size, uint16_t type, uint16_t numDependents, uint16_t stackPointer, uint16_t registryAddress)
{
	ptr[0] = address;				// module address
	ptr[1] = size;					// memory allocation size
	ptr[2] = type;					// allocation type (0=task, 1=library, 2=heap)
	ptr[3] = numDependents;			// number of dependent modules
	ptr[4] = stackPointer;			// stack pointer (type 0) or owner address (type 2)
	ptr[5] = 0;						// exit code
	ptr[6] = 0;						// timeout high bits
	ptr[7] = 0;						// timeout low bits
	ptr[8] = 0;						// mutex to wait for
	ptr[9] = registryAddress;		// registry address
	ptr[10] = type == 0 ? 1 : 0;	// task status (0=new (don't run me yet), 1=running, 2=exited)
	ptr[11] = 1;					// privileged flag
	// offsets 12-19 are reserved
}

// This initializes memory with everything the OS needs to get started.
// Returns the address where execution should start.
// The main function of the kernel program will take it from there.
uint16_t Kernel_Load(Memory memory)
{
	uint16_t *m = memory.data;
	const int stackSize = 256;
	const int bootStart = 300;
	const int libAddress = 3072;

	// compile all programs so that the module files will be available on the virtual disk
	Program *lib = Compiler_BuildFile("res/code/stdlib.txt");
	Program *kernel = Compiler_BuildFile("res/code/kernel.txt");
	Program *demo = Compiler_BuildFile("res/code/demo.tmp");
	Program *test = Compiler_BuildFile("res/code/test.txt");
	Compiler_Destroy(demo);
	Compiler_Destroy(test);
	if (lib == NULL || kernel == NULL) return 0xffff;

	int libSize = lib->length;
	// rather than search by function name, I know at compile time the index of this function
	int switchTaskAddress = libAddress + lib->functions[7].offset;
	int kernelAddress = libAddress + libSize;
	int kernelMain = kernelAddress + kernel->mainAddress;
	int kernelStack = kernelAddress + kernel->length;
	int kernelSize = kernel->length + stackSize;
	int kernelLocals = kernel->functions[0].numLocals;
	printf("stdlib address: %d, size: %d\n", libAddress, libSize);
	printf("kernel address: %d, size: %d\n", kernelAddress, kernelSize);

	// the binary begins with space for addresses of referenced modules
	// insert the address of the library that the kernel needs
	kernel->bin[0] = libAddress;

	// load modules into memory
	memcpy(m + libAddress, lib->bin, lib->length * sizeof(uint16_t));
	memcpy(m + kernelAddress, kernel->bin, kernel->length * sizeof(uint16_t));

	Compiler_Destroy(kernel);
	Compiler_Destroy(lib);

	// interrupt descriptor table
	// each entry is just the address of an ISR
	// the ISRs begin at address 16 and each occupies 16 words
	for (int i = 0; i < 16; i++)
		m[i] = (i + 1) * 16;

	// the first interrupt service routine is for switching tasks
	// it gets triggered by a timer
	Instr2(m + 16, INSTR_ADDI, REG_RESULT, REG_STACK_PTR, 0);		// copy SP of interrupted task to another register
	LoadFullWord(m + 17, REG_STACK_PTR, 768);						// this isr has its own stack
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
	LoadFullWord(m + 32, REG_OPERAND_A, IO_INPUT_CHAR);				// address of the input character
	LoadFullWord(m + 34, REG_OPERAND_B, IO_PRINT_STR_ADDR);			// address for string output
	Instr2(m + 36, INSTR_SW, REG_OPERAND_A, REG_OPERAND_B, 0);		// copy char pointer to string output
	Instr2(m + 37, INSTR_EXT, OPX_MORE, OPXX_IRET, 0);				// resume

	// the rest of the ISRs
	// these just return (IRET) without taking action
	for (int i = 48; i <= 256; i += 16)
		Instr2(m + i, INSTR_EXT, OPX_MORE, OPXX_IRET, 0);

	// task termination procedure: all main functions should return here
	LoadFullWord(m + 280, REG_OPERAND_B, 2050);						// ptr to ptr to pcb
	Instr2(m + 282, INSTR_LW, REG_OPERAND_B, REG_OPERAND_B, 0);		// OB now contains address of current pcb
	Instr2(m + 283, INSTR_SW, REG_RESULT, REG_OPERAND_B, 5);		// store exit code (return value) so kernel can see it
	Instr2(m + 284, INSTR_ADDI, REG_OPERAND_A, REG_ZERO, 2);		// OA now contains 2
	Instr2(m + 285, INSTR_SW, REG_OPERAND_A, REG_OPERAND_B, 10);	// set exited flag so kernel can clean up this task
	Instr2(m + 286, INSTR_EXT, OPX_MORE, OPXX_HALT, 0);				// halt
	Instr1(m + 287, INSTR_BEZ, REG_ZERO, -2);						// go back to halt in case an interrupt wakes this dead task

	// bootstrap procedure: execution starts here
	uint16_t *bs = m + bootStart;
	LoadFullWord(bs + 0, REG_MODULE_PTR, kernelAddress);			// MP points to the kernel module
	LoadFullWord(bs + 2, REG_RESULT, kernelMain);					// get address of first function to call
	LoadFullWord(bs + 4, REG_STACK_PTR, kernelStack);				// initialize SP
	Instr2(bs + 6, INSTR_ADDI, REG_FRAME_PTR, REG_STACK_PTR, 0);	// FP starts out equal to SP
	Instr2(bs + 7, INSTR_ADDI, REG_RET_ADDR, REG_ZERO, 0);			// initialize RA (boot procedure never returns anyway)
	Instr2(bs + 8, INSTR_EXT, OPX_MORE, OPXX_IEN, 1);				// enable interrupts
	Instr2(bs + 9, INSTR_EXT, OPX_CALL, REG_RESULT, kernelLocals);	// kernel entry point
	Instr2(bs + 10, INSTR_EXT, OPX_MORE, OPXX_HALT, 0);				// if kernel ever exits, halt
	Instr1(bs + 11, INSTR_BEZ, REG_ZERO, -2);						// interrupt returns here; go back to halt

	// module registry
	// number of modules is stored at 2052, list begins at 1024
	// each entry is 32 words / 64 bytes
	// first word is the pcb address if the module is loaded, else zero
	// remaining 31 words contain the module name as a string
	RegisterModule(m + 1024, 2060, "stdlib");
	RegisterModule(m + 1056, 2080, "kernel");

	// global metadata about modules and memory records
	m[2048] = 2;				// number of memory allocation entries
	m[2049] = kernelAddress;	// module address of current task
	m[2050] = 2080;				// PCB of current task
	m[2051] = memory.n;			// total memory size (in words, not bytes)
	m[2052] = 2;				// number of registered modules (see registry above)

	// initial memory allocation entries
	// they begin at 2060 and each entry is 20 words / 40 bytes
	AddMemoryEntry(m + 2060, libAddress, libSize, 1, 1, 0, 1024);
	AddMemoryEntry(m + 2080, kernelAddress, kernelSize, 0, 0, kernelStack, 1056);

	return bootStart;
}
