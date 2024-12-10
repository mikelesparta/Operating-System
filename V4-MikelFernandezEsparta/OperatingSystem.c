#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
//#include "Processor.c"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include "ComputerSystem.h"
#include "ComputerSystemBase.h"

// Functions prototypes
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun(int);
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_MoveToTheBLOCKEDState(int);
void OperatingSystem_PreemptRunningProcessBlocked();
int OperatingSystem_ExtractFromSleepingQueue();
int OperatingSystem_GetExecutingProcessID();
void OperatingSystem_ReleaseMainMemory(int);

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
int initialPID = PROCESSTABLEMAXSIZE - 1;

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 

// Array that contains the identifiers of the READY processes
//heapItem readyToRunQueue[PROCESSTABLEMAXSIZE];
//int numberOfReadyToRunProcesses=0;
heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES]={0,0};
char * queueNames [NUMBEROFQUEUES]={"USER","DAEMONS"};

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

char DAEMONS_PROGRAMS_FILE[MAXIMUMLENGTH]="teachersDaemons";

char * statesNames[5]={"NEW","READY","EXECUTING","BLOCKED","EXIT"};

int numberOfClockInterrupts = 0;

// V2-EX5
// Heap with blocked processes sorted by when to wakeup
heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses=0;

//V4-EX2 auxiliary to acces the names of the exceptions
char * exceptionNames[4]={"division by zero","invalid process mode","invalid adress","invalid instruction"};

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code
	programFile=fopen("OperatingSystemCode", "r");
	
	if (programFile==NULL){
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing Operating System!\n");
		exit(1);		
	}

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(programFile);

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++){
		processTable[i].busy=0;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+2);
		
	OperatingSystem_InitializePartitionTable(); //V4-EX5

	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);
	
	//V3-EX1C
	ComputerSystem_FillInArrivalTimeQueue();

	//V3-EX1D
	OperatingSystem_PrintStatus();

	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();

	//START V1-EX15
	//Simulation ends immediately when the long-term scheduler is incapable to create any process
	if(numberOfNotTerminatedUserProcesses == 0 && OperatingSystem_IsThereANewProgram() == EMPTYQUEUE){
		//OperatingSystem_TerminatingSIP();
		OperatingSystem_ReadyToShutdown();
	}
	//END V1-15

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")) {
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing SIP program!\n");
		exit(1);		
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command line and daemons programs
int OperatingSystem_LongTermScheduler() {
  
	int PID, i,
		numberOfSuccessfullyCreatedProcesses=0;
	
	//V3-EX3	
	while(OperatingSystem_IsThereANewProgram() == YES){
		i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL ,&numberOfProgramsInArrivalTimeQueue);
		PID=OperatingSystem_CreateProcess(i);
		
		switch(PID){
			case NOFREEENTRY:
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(103, ERROR, programList[i]->executableName);
				break;
		
			case PROGRAMDOESNOTEXIST:
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "it does not exist");
				break;

			case PROGRAMNOTVALID:
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "invalid priority or size");
				break;

			case TOOBIGPROCESS:
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
				break;

			case MEMORYFULL: //V4-EX6
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(144, ERROR, programList[i]->executableName);
				break;
			
			default:
				numberOfSuccessfullyCreatedProcesses++;
				if (programList[i]->type==USERPROGRAM) 
					numberOfNotTerminatedUserProcesses++;

				// Move process to the ready state
				OperatingSystem_MoveToTheREADYState(PID);
		}
	}
/*
	for (i=0; programList[i]!=NULL && i<PROGRAMSMAXNUMBER ; i++) {
		PID=OperatingSystem_CreateProcess(i);
		
		switch(PID){
			case NOFREEENTRY:
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(103, ERROR, programList[i]->executableName);
				break;
		
			case PROGRAMDOESNOTEXIST:
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "it does not exist");
				break;

			case PROGRAMNOTVALID:
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "invalid priority or size");
				break;

			case TOOBIGPROCESS:
				OperatingSystem_ShowTime(ERROR);
				ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
				break;
			

			default:
				numberOfSuccessfullyCreatedProcesses++;
				if (programList[i]->type==USERPROGRAM) 
					numberOfNotTerminatedUserProcesses++;

				// Move process to the ready state
				OperatingSystem_MoveToTheREADYState(PID);
		}		
	}
	*/

	if(numberOfSuccessfullyCreatedProcesses > 0)
		OperatingSystem_PrintStatus();

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	int assignedPartition;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID = OperatingSystem_ObtainAnEntryInTheProcessTable();
	
	if(PID == NOFREEENTRY)
		return NOFREEENTRY;
		
	// Check if programFile exists
	programFile = fopen(executableProgram->executableName, "r");

	if(programFile == NULL)
		return PROGRAMDOESNOTEXIST;

	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(programFile);	

	if(processSize == PROGRAMDOESNOTEXIST || processSize == PROGRAMNOTVALID)
		return processSize;	

	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);	
	
	if(priority == PROGRAMNOTVALID)
		return priority;	

	// Obtain enough memory space
	//V4-EX6
	OperatingSystem_ShowTime(SYSMEM);
	ComputerSystem_DebugMessage(142, SYSMEM, PID, programList[processTable[PID].programListIndex]->executableName, processSize);

 	assignedPartition = OperatingSystem_ObtainMainMemory(processSize, PID);

	if (assignedPartition == MEMORYFULL)
		return MEMORYFULL;

	loadingPhysicalAddress = partitionsTable[assignedPartition].initAddress;

	if(loadingPhysicalAddress == TOOBIGPROCESS)
		return TOOBIGPROCESS;
	
	// Load program in the allocated memory
	if( OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize) < 0 )
		return TOOBIGPROCESS;
	
	
	OperatingSystem_ShowPartitionTable("before allocating memory"); 
	partitionsTable[assignedPartition].PID = PID;

//V4-EX7
	OperatingSystem_ShowTime(SYSMEM);
	ComputerSystem_DebugMessage(143, SYSMEM, assignedPartition, partitionsTable[assignedPartition].initAddress, partitionsTable[assignedPartition].size, PID, programList[processTable[PID].programListIndex]->executableName);
	
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	
	OperatingSystem_ShowPartitionTable("after allocating memory"); //V4-EX7

	// Show message "Process [PID] created from program [executableName]\n"
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(70,INIT,PID,executableProgram->executableName);
	
	return PID;
}

// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
// If there were more than one eligible partitions, the chosen one should be that using the lowest physical addresses
int OperatingSystem_ObtainMainMemory(int processSize, int PID) { //V4-EX6
	int partitionID, i, patitionSize;
	int foundPartition = 0;

	for (i = 0; i < PARTITIONTABLEMAXSIZE; i++) {
		//In order to be valid, it needs to be inside the scope of the partitionstable and CANNOT be too big
		if(partitionsTable[i].initAddress >= 0 && partitionsTable[i].size < PARTITIONTABLEMAXSIZE && partitionsTable[i].size >= processSize){
			//If there exists a valid partition
			foundPartition++;

			if (partitionsTable[i].PID == NOPROCESS) {
				partitionID = i;					
				patitionSize = partitionsTable[i].size;
			}
		}
	}
	
	if (foundPartition == 0)
		return NOPROCESS;

	if (partitionID == TOOBIGPROCESS)
		return TOOBIGPROCESS;

	if (processSize > patitionSize)
		return TOOBIGPROCESS;

	if (partitionID == MEMORYFULL)
		return MEMORYFULL;

	return partitionID;
}

// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {

	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	processTable[PID].state=NEW;
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;
	processTable[PID].queueID=programList[processPLIndex]->type;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;
	} 
	else {
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;
		processTable[PID].copyOfAccumulatorRegister=0; //V1-E13

	}
	//processTable[PID]-queueID=programlist....
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(111, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[NEW]); 
}


// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {
	if (Heap_add(PID, readyToRunQueue[processTable[PID].queueID], QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[processTable[PID].queueID] ,PROCESSTABLEMAXSIZE)>=0) {
		processTable[PID].state = READY;
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[NEW], statesNames[READY]); 
	} 
	//OperatingSystem_PrintReadyToRunQueue(); //V1-EX9
}

//V2-EX15D auxiliary
// Move a process to the BLOCKED state: it will be inserted, depending on its priority, in
// a queue of identifiers of EXECUTING processes
void OperatingSystem_MoveToTheBLOCKEDState(int PID) {
	if (Heap_add(PID, sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses ,PROCESSTABLEMAXSIZE)>=0) {
		processTable[PID].state = BLOCKED;
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[EXECUTING], statesNames[BLOCKED]); 
	} 
	//OperatingSystem_PrintReadyToRunQueue(); //V1-EX9
}

// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess=NOPROCESS;
	int i;

	for(i=0; i<NUMBEROFQUEUES && selectedProcess==NOPROCESS; i++)
		selectedProcess=OperatingSystem_ExtractFromReadyToRun(i);
	
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun(int queue) {
  
	int selectedProcess=NOPROCESS;

	selectedProcess=Heap_poll(readyToRunQueue[queue] ,QUEUE_PRIORITY ,	&numberOfReadyToRunProcesses[queue]);
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}

// Return PID of more priority process in the sleeping queue
int OperatingSystem_ExtractFromSleepingQueue() {  
	int selectedProcess=NOPROCESS;

	selectedProcess = Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP , &numberOfSleepingProcesses);

	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	// Change the process' state
	processTable[PID].state=EXECUTING;
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[READY], statesNames[EXECUTING]);
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-3,processTable[PID].copyOfAccumulatorRegister); //V1-E13

	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
void OperatingSystem_PreemptRunningProcess() {
	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}

// Aux function invoked when the executing process leaves the CPU and goes to the BLOCKED state
void OperatingSystem_PreemptRunningProcessBlocked() {
	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state to blocked
	OperatingSystem_MoveToTheBLOCKEDState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}

// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID) {
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-1);
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-2);	

	// Load Accumulator saved for interrupt manager
	processTable[PID].copyOfAccumulatorRegister=Processor_GetAccumulator(); ////V1-E13
}


// Exception management routine to handle exceptions
void OperatingSystem_HandleException() {
	// Show message " Process [1 – ProcessName1] has caused an exception (invalid address) and is being terminated\n"
	//V4-EX2
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(140, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, exceptionNames[Processor_GetRegisterB() ]);
	
	OperatingSystem_TerminateProcess();
	OperatingSystem_PrintStatus();
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {  
	int selectedProcess;
  	
	//processTable[executingProcessID].state=EXIT;
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, statesNames[processTable[executingProcessID].state], statesNames[EXIT]);

	//V4-EX8
	OperatingSystem_ShowPartitionTable("before releasing memory");
	OperatingSystem_ReleaseMainMemory(executingProcessID); 
	OperatingSystem_ShowPartitionTable("after releasing memory");

	if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	if (numberOfNotTerminatedUserProcesses==0) {
		if (executingProcessID==sipID) {
			// finishing sipID, change PC to address of OS HALT instruction
			OperatingSystem_TerminatingSIP();
			OperatingSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(99,SHUTDOWN,"The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID;
	int nextPID;
	int previousPID;

	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterA();
	
	switch (systemCallID) {
		case SYSCALL_PRINTEXECPID:
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(72,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_END:
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(73,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateProcess();
			OperatingSystem_PrintStatus();
			break;

		//START V1-EX12
		case SYSCALL_YIELD:			
			//Return top item on the readyToRunQueue (next process)
			nextPID = Heap_getFirst(readyToRunQueue[processTable[executingProcessID].queueID], numberOfReadyToRunProcesses[processTable[executingProcessID].queueID]);
			
			//In case that process wouldn´t exist, it has to be different from -1
			if(nextPID != -1){
				//We compare both priorities are equal in order to transfer it
				if (processTable[executingProcessID].priority != processTable[nextPID].priority)
					break;
				else{
					previousPID = executingProcessID;

					OperatingSystem_ShortTermScheduler();

					//Executing process leaves the CPU
					OperatingSystem_PreemptRunningProcess();

					//Assigns processor to the next process executing
					OperatingSystem_Dispatch(nextPID);

					OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
					ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, previousPID, programList[processTable[previousPID].programListIndex]->executableName , executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);			
					OperatingSystem_PrintStatus();
				}								
			}
			OperatingSystem_TerminateProcess();

			break;	
			//END V1-E12

			//START V2-EX5D
			// will block the executing process (moving it to the BLOCKED state) and will insert it in the appropriate position (ascending order of whenToWakeUp) of the sleepingProcessesQueue
			case SYSCALL_SLEEP:
				//The value of whenToWakeUp will be obtained by adding the absolute value of the accumulator register current value to the just occurred number of clock interrupts plus one
				processTable[executingProcessID].whenToWakeUp = abs(processTable[executingProcessID].copyOfAccumulatorRegister) + numberOfClockInterrupts + 1;;

				OperatingSystem_PreemptRunningProcessBlocked(executingProcessID);

				//We need to save the context
				OperatingSystem_SaveContext(executingProcessID);

				// Choose the next process that is going to be executed
				int nextProcess = OperatingSystem_ShortTermScheduler();

				//Assigns processor to the next process executing
				OperatingSystem_Dispatch(nextProcess);			

				OperatingSystem_PrintStatus();

				break;
			//END V2-EX5D

			//V4-EX4
			default :
				OperatingSystem_ShowTime(INTERRUPT);
				ComputerSystem_DebugMessage(141, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, systemCallID);
				OperatingSystem_TerminateProcess();
				OperatingSystem_PrintStatus();
	}
}					
	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();
			break;
		case CLOCKINT_BIT: // CLOCKINT_BIT=9
			OperatingSystem_HandleClockInterrupt();
			break;
	}
}

/*V1-EX9-begin
void OperatingSystem_PrintReadyToRunQueue(){
	int i;
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);

	for(i = 0; i < numberOfReadyToRunProcesses - 1; i++)
		ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, readyToRunQueue[i].info, processTable[readyToRunQueue[i]->info].priority);
	
	ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, readyToRunQueue[i].info, processTable[readyToRunQueue[i]->info].priority);
}
V1-EX9-end*/

//V1-EX11-begin
void OperatingSystem_PrintReadyToRunQueue() {
	int i;
	int PID;
	
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);
	
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE);
	for (i = 0; i < numberOfReadyToRunProcesses[USERPROCESSQUEUE] - 1; i++) {
		PID = readyToRunQueue[USERPROCESSQUEUE][i].info;
		ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, PID, processTable[PID].priority);			
	}	
	PID = readyToRunQueue[USERPROCESSQUEUE][i].info;
	//OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, PID, processTable[PID].priority);

	ComputerSystem_DebugMessage(114, SHORTTERMSCHEDULE);
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(113, SHORTTERMSCHEDULE);
	for (i = 0; i < numberOfReadyToRunProcesses[DAEMONSQUEUE] - 1; i++) {
		PID = readyToRunQueue[DAEMONSQUEUE][i].info;
		ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, PID, processTable[PID].priority);
	}
	PID = readyToRunQueue[DAEMONSQUEUE][i].info;
	//OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, PID, processTable[PID].priority);	
}
//V1-EX11-end

// V2-EX2 && V2-EX4 && V2-EX6
void OperatingSystem_HandleClockInterrupt(){ 
	int PIDexecutingFirst, selectedProcess, numCreatedProcesses;

	numberOfClockInterrupts++;

	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts);

	// If the value of the field whenToWakeUp of a (or more than one) process of the mentioned queue equals the number of occurred clock interrupts
	while( numberOfSleepingProcesses > 0 &&  processTable[executingProcessID].whenToWakeUp  == numberOfClockInterrupts){
		// Extract process from the sleeping queue
		int PID = OperatingSystem_ExtractFromSleepingQueue();
		if(PID >= 0 ){	
			// Process will be unblocked, then it moves to the READY state	
			OperatingSystem_MoveToTheREADYState(PID);

			OperatingSystem_PrintStatus();			
		}
	}
	
	//V3-EX4
	numCreatedProcesses = OperatingSystem_LongTermScheduler();
	if(numberOfNotTerminatedUserProcesses == 0 && OperatingSystem_IsThereANewProgram() == EMPTYQUEUE){
		//OperatingSystem_TerminatingSIP();
		OperatingSystem_ReadyToShutdown();
	}
	
	if(numCreatedProcesses > 0)
		OperatingSystem_PrintStatus();
		
	// If it is a daemon program
	if(programList[processTable[executingProcessID].programListIndex]->type == DAEMONPROGRAM){	
		PIDexecutingFirst = processTable[executingProcessID].queueID;	
		//If there is a user process that is going to be executed
		if(numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0){	
			//Change processes	
			OperatingSystem_PreemptRunningProcess();

			selectedProcess = OperatingSystem_ShortTermScheduler();

			OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
			ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, PIDexecutingFirst, programList[processTable[PIDexecutingFirst].programListIndex]->executableName, selectedProcess, programList[processTable[selectedProcess].programListIndex]->executableName); 

			// Assign the processor to the selected process
			OperatingSystem_Dispatch(selectedProcess);
			OperatingSystem_PrintStatus();
		}
	}

	// User program
	else{	
		int nextUserProcessPID = Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE], numberOfReadyToRunProcesses[USERPROCESSQUEUE]);	
		PIDexecutingFirst = processTable[executingProcessID].queueID;	

		//If there is a user process that is going to be executed
		//Check it is the one with the highest priority
		if(nextUserProcessPID != -1 && processTable[nextUserProcessPID].priority < processTable[executingProcessID].priority){	
			//Change processes
			OperatingSystem_PreemptRunningProcess();

			selectedProcess = OperatingSystem_ShortTermScheduler();

			OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
			ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, PIDexecutingFirst, programList[processTable[PIDexecutingFirst].programListIndex]->executableName, selectedProcess, programList[processTable[selectedProcess].programListIndex]->executableName); 
			
			// Assign the processor to the selected process
			OperatingSystem_Dispatch(selectedProcess);
			OperatingSystem_PrintStatus();
		}
	}
}

//V3-EX2
int OperatingSystem_GetExecutingProcessID(){
	return executingProcessID;
}

//V4-EX8
void OperatingSystem_ReleaseMainMemory(int PID){
	int i = 0;
	
	while(i <= PARTITIONTABLEMAXSIZE)
	{
		if (partitionsTable[i].PID == PID)
		{
			partitionsTable[i].PID = NOPROCESS;			
			break;
		}
		i++;
	}
	
	OperatingSystem_ShowTime(SYSMEM);
	ComputerSystem_DebugMessage(145, SYSMEM, i, partitionsTable[i].initAddress, partitionsTable[i].size, PID, programList[processTable[PID].programListIndex]->executableName);
}