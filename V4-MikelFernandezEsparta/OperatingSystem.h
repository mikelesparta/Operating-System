#ifndef OPERATINGSYSTEM_H
#define OPERATINGSYSTEM_H

#include "ComputerSystem.h"
#include <stdio.h>


#define SUCCESS 1
#define PROGRAMDOESNOTEXIST -1
#define PROGRAMNOTVALID -2

// In this version, every process occupies a 60 positions main memory chunk 
// so we can use 60 positions for OS code and the system stack
#define MAINMEMORYSECTIONSIZE (MAINMEMORYSIZE / (PROCESSTABLEMAXSIZE+1))

#define NOFREEENTRY -3
#define TOOBIGPROCESS -4

#define NOPROCESS -1

//V1-EX11
#define NUMBEROFQUEUES 2

//V2-EX5C
#define SLEEPINGQUEUE

// Partitions configuration file name definition
#define MEMCONFIG //V4-EX5

#define MEMORYFULL -5 //V4-EX6

enum TypeOfReadyToRunProcessQueues { USERPROCESSQUEUE, DAEMONSQUEUE};

// Contains the possible type of programs
enum ProgramTypes { USERPROGRAM, DAEMONPROGRAM }; 

// Enumerated type containing all the possible process states
enum ProcessStates { NEW, READY, EXECUTING, BLOCKED, EXIT};

// Enumerated type containing the list of system calls and their numeric identifiers
enum SystemCallIdentifiers { SYSCALL_END=3, SYSCALL_YIELD = 4, SYSCALL_PRINTEXECPID=5, SYSCALL_SLEEP=7};

// A PCB contains all of the information about a process that is needed by the OS
typedef struct {
	int busy;
	int initialPhysicalAddress;
	int processSize;
	int state;
	int priority;
	int copyOfPCRegister;
	unsigned int copyOfPSWRegister;
	int copyOfAccumulatorRegister;
	int programListIndex;
	int queueID;		//V1-EX11
	int whenToWakeUp;   // V2-EX5A
} PCB;

// These "extern" declaration enables other source code files to gain access
// to the variable listed


// Functions prototypes
void OperatingSystem_Initialize();
void OperatingSystem_InterruptLogic(int);
int OperatingSystem_PrepareStudentsDaemons(int);
int OperatingSystem_GetExecutingProcessID(); //V3-Ex2

#endif
