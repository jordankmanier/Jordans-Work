#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//Definitions
#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define MULT 5
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000

//Global list

typedef struct IFIDStruct {
    	int instr;
    	int pcPlus1;
} IFIDType;

typedef struct IDEXStruct {
    	int instr;
    	int pcPlus1;
    	int readRegA;
    	int readRegB;
    	int offset;
} IDEXType;

typedef struct EXMEMStruct {
    	int instr;
	int pcPlus1;
    	int branchTarget;
    	int aluResult;
    	int readRegB;
} EXMEMType;

typedef struct MEMWBStruct {
	int instr;
    	int writeData;
} MEMWBType;

typedef struct WBENDStruct {
    	int instr;
    	int writeData;
} WBENDType;

enum branchPredict {SNT, WNT, WT, ST};

typedef struct branchTable {
	enum branchPredict predict[4];
	int branchPC[4];
	int branchTarget[4];
} branchType;

typedef struct stateStruct {
    	int pc;
    	int instrMem[NUMMEMORY];
    	int dataMem[NUMMEMORY];
    	int reg[NUMREGS];
    	int numMemory;
    	IFIDType IFID;
    	IDEXType IDEX;
    	EXMEMType EXMEM;
    	MEMWBType MEMWB;
    	WBENDType WBEND;
	branchType branchPredictor;
    	int cycles; /* number of cycles run so far */
} stateType;

int fetched = 0;
int retired = 0;
int branches = 0;
int mispred = 0;

// End Globals

//Function Decs
void run(stateType *state);
void printState(stateType *statePtr);
void printInstruction(int instr);
int field0(int instruction);
int field1(int instruction);
int field2(int instruction);
int opcode(int instruction);
int readInstructions(char* filename, stateType *state);
int clear(stateType *state);	//Make sure processor has a fresh-slate
int convertNum(int num); //convert a 16-bit number into a 32-bit Sun integer

int main(int argc, char* argv[]){
	if(argc != 2){
		printf("Usage: lpsim <machine code>\n");
		return -1;
	}
	
	//Initialize State
	stateType startState;
	clear(&startState);

	int r_inst = readInstructions(argv[1], &startState);
	if(r_inst == -1){
		printf("Error: file `%s` could not be opened.\n", argv[1]);
		return -1;
	}
	else if(r_inst == -2){
		printf("Error: Not enough memory to run program `%s`\n", argv[1]);
	}
	
	for(int i = 0; i < startState.numMemory; i++)
		printf("memory[%d] = %d\n", i, startState.instrMem[i]);
	
	printf("%d memory words\n\tinstruction memory:\n", startState.numMemory);

	for(int i = 0; i < startState.numMemory; i++){
		printf("\t\tinstrMem[ %d ] ", i);
		printInstruction(startState.instrMem[i]);
	}

	run(&startState);


	return 0;
}



void run(stateType *startState){
	int offset;

	stateType state = *startState;

    	while(1) {
			state.reg[0] = 0;
			
		printState(&state);

		// check for halt
		if (opcode(state.MEMWB.instr) == HALT) {
			fetched = fetched - 3;
	    		printf("machine halted\n");
	    		printf("CYCLES:\t\t%d\n", state.cycles);
			printf("FETCHED:\t%d\n", fetched);
			printf("RETIRED:\t%d\n", retired);
			printf("BRANCHES:\t%d\n", branches);
			printf("MISPRED:\t%d\n", mispred);
	    		exit(0);
		}

		stateType newState = state;
		newState.reg[0] = 0;
		newState.cycles++;

		// --------------------- IF stage ---------------------  //

		//If instruction isn't BEQ, fetch instruction from memory, then save instruction and pc+1 to IFID reg
		if(opcode(state.IFID.instr) != BEQ){
			newState.IFID.instr = state.instrMem[state.pc];
			newState.pc++;
			newState.IFID.pcPlus1 = newState.pc;
		}
		
		//If state.IFID.instr is branch, check BTB for prediction
		else if(opcode(state.IFID.instr) == BEQ){
			//Scan for PC match
			for(int i = 0; i < 4; i++){
				if(state.branchPredictor.branchPC[i] == state.pc){
					printf("Found branch!\n");
					//Found this branch in BTB, check prediction
					switch(state.branchPredictor.predict[i]){
						case SNT:
						case WNT:
							//Don't take branch
							newState.IFID.instr = state.instrMem[state.pc];
							newState.pc++;
							newState.IFID.pcPlus1 = newState.pc;
							//printf("Not taking branch.\n");
							i = 4;
							break;
						case WT:
						case ST:
							//Take branch
							newState.IFID.instr = state.instrMem[state.branchPredictor.branchTarget[i]];
							newState.pc = state.branchPredictor.branchTarget[i] + 1;
							newState.IFID.pcPlus1 = newState.pc;
							//printf("Taking branch. Target = %d\n", state.branchPredictor.branchTarget[i]);
							i = 4;
							break;
					}
				}
				else if(i == 3){
					//Didn't find branch
					//Don't take branch
					newState.IFID.instr = state.instrMem[state.pc];
					newState.pc++;
					newState.IFID.pcPlus1 = newState.pc;
				}
			}
		}

		fetched++;

		// --------------------- ID stage ---------------------  //
			
		//Pass along PC+1 and instruction bits from IFID to IDEX
		newState.IDEX.instr = state.IFID.instr;
		newState.IDEX.pcPlus1 = state.IFID.pcPlus1;
	
		//Read offset field from instruction as 32bit signed int
		offset = convertNum(field2(state.IFID.instr));
	
		//Read data from regA, regB and offset and save in IDEX reg
		newState.IDEX.readRegA = state.reg[field0(state.IFID.instr)];
		newState.IDEX.readRegB = state.reg[field1(state.IFID.instr)];
		newState.IDEX.offset = offset;

		
		//Check if stall is necessary
		if (opcode(state.IDEX.instr) == LW) {
			if (field1(state.IDEX.instr) == field0(state.IFID.instr)) {
				newState.IDEX.instr = 0x1c00000;
				newState.IFID.instr = state.IFID.instr;
				newState.IFID.pcPlus1 = state.IFID.pcPlus1;// + 1;
				newState.pc--;
			}
			else if (field1(state.IDEX.instr) == field1(state.IFID.instr)) {
				newState.IDEX.instr = 0x1c00000;
				newState.IFID.instr = state.IFID.instr;
				newState.IFID.pcPlus1 = state.IFID.pcPlus1;// + 1;
				newState.pc--;
			}
		}
		

		// --------------------- EX stage ---------------------  //

		//Pass along instructions bits and regB data from IDEX to EXMEM
		newState.EXMEM.instr = state.IDEX.instr;
		newState.EXMEM.readRegB = state.IDEX.readRegB;
	
		//Calculate branch target
		newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + state.IDEX.offset;
		
		//Forward pcPlus1 in case branch needs to be added to BTB
		newState.EXMEM.pcPlus1 = state.IDEX.pcPlus1;
	
		//Manage ALU
		if (opcode(state.IDEX.instr) == ADD){
			newState.EXMEM.aluResult = state.IDEX.readRegA + state.IDEX.readRegB;
		}
		else if (opcode(state.IDEX.instr) == NAND) {
			newState.EXMEM.aluResult = ~(state.IDEX.readRegA + state.IDEX.readRegB);
		}
		else if (opcode(state.IDEX.instr) == MULT) {
			newState.EXMEM.aluResult = state.IDEX.readRegA * state.IDEX.readRegB;
		}
		else if ((opcode(state.IDEX.instr) == LW) || (opcode(state.IDEX.instr) == SW)) {
			newState.EXMEM.aluResult = state.IDEX.readRegA + state.IDEX.offset;
		}
		else if (opcode(state.IDEX.instr) == BEQ) {
			if(state.IDEX.readRegA == state.IDEX.readRegB)
				newState.EXMEM.aluResult = 1;
			else
				newState.EXMEM.aluResult = 0;
		}
	
		
		//Forward data if necessary 
		//newState.IDEX registers need to be updated with aluResult from state.EXMEM if state.IDEX destReg = newState.IDEX.regA/B; only works for ADD, NAND and MULT because aluResult == destReg data
		if ((opcode(state.IDEX.instr) == ADD) || (opcode(state.IDEX.instr) == NAND) || (opcode(state.IDEX.instr) == MULT)){
			if (field2(state.IDEX.instr) == field0(state.IFID.instr)) {
				newState.IDEX.readRegA = newState.EXMEM.aluResult;
				//printf("Forwarded RegA\n");
			}
			if (field2(state.IDEX.instr) == field1(state.IFID.instr)) {
				newState.IDEX.readRegB = newState.EXMEM.aluResult;
				//printf("Forwarded RegB\n");
			}
		}
		

		// --------------------- MEM stage --------------------- //		
		//Pass along instruction bits from EXMEM to MEMWB
		newState.MEMWB.instr = state.EXMEM.instr;
	
		//Handle SW
		if(opcode(state.EXMEM.instr) == SW){
			newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.readRegB;
		}
	
		//Handle BEQ
		if(opcode(state.EXMEM.instr) == BEQ){
			branches++;
			if(state.EXMEM.aluResult == 1){
				//Branch should have been taken
				//Check if branch wasn't already taken
				if(state.IDEX.pcPlus1 != state.EXMEM.branchTarget + 1){
					mispred++;
					//So add to BTB and kill pipeline and set pc to branchTarget
					//Shift BTB
					for(int i = 0; i < 3; i++){
						newState.branchPredictor.predict[i+1] = state.branchPredictor.predict[i];
						newState.branchPredictor.branchPC[i+1] = state.branchPredictor.branchPC[i];
						newState.branchPredictor.branchTarget[i+1] = state.branchPredictor.branchTarget[i];
					}
					//Add to BTB
					newState.branchPredictor.predict[0] = WT;
					newState.branchPredictor.branchPC[0] = state.EXMEM.pcPlus1;
					newState.branchPredictor.branchTarget[0] = state.EXMEM.branchTarget;

					//Kill everything in pipeline
					newState.EXMEM.instr = 0x1c00000;
					newState.IDEX.instr = 0x1c00000;
					newState.IFID.instr = 0x1c00000;
					//Set pc to branch target
					newState.pc = state.EXMEM.branchTarget;
				}
				//Else we are fine
			}
			else{
				//Branch should not have been taken
				//Check if it was
				if(state.IDEX.pcPlus1 == state.EXMEM.branchTarget + 1){
					mispred++;
					//It was, kill everything in pipeline and set pc to pc+1
					//Kill everything in pipeline
					newState.EXMEM.instr = 0x1c00000;
					newState.IDEX.instr = 0x1c00000;
					newState.IFID.instr = 0x1c00000;
					//Set pc to pc+1
					newState.pc = state.EXMEM.pcPlus1;
				}
			}
		}

		//Calculate writeData
		if ((opcode(state.EXMEM.instr) == ADD) || (opcode(state.EXMEM.instr) == NAND) || (opcode(state.EXMEM.instr) == MULT)){
			newState.MEMWB.writeData = state.EXMEM.aluResult;
			//printf("ADD/NAND\n");
		}
		else if (opcode(state.EXMEM.instr) == LW){
			newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
			
			//Data forward newState.MEMWB.writeData to newState.IDEX.readRegA/B if newState.MEMWB regB = newState.IDEX regA/B
			if(field1(state.EXMEM.instr) == field0(state.IFID.instr)){
				newState.IDEX.readRegA = newState.MEMWB.writeData;
				//printf("LW Forwarded RegA\n");
			}
			if (field1(state.EXMEM.instr) == field1(state.IFID.instr)) {
				newState.IDEX.readRegB = newState.MEMWB.writeData;
				//printf("LW Forwarded RegB\n");
			}
			
		}

		if(opcode(newState.MEMWB.instr) != NOOP)
			retired++;

		// --------------------- WB stage ---------------------  //

		//Pass along instruction bits from MEMWB to WBEND
		newState.WBEND.instr = state.MEMWB.instr;
		newState.WBEND.writeData = 0;
	
		//Write to registers if part of opcode(state.IDEX.instr)
		if ((opcode(state.MEMWB.instr) == ADD) || (opcode(state.MEMWB.instr) == NAND) || (opcode(state.MEMWB.instr) == MULT)){
			newState.reg[field2(state.MEMWB.instr)] = state.MEMWB.writeData;
			newState.WBEND.writeData = state.MEMWB.writeData;
		}
		else if (opcode(state.MEMWB.instr) == LW) {
			newState.reg[field1(state.MEMWB.instr)] = state.MEMWB.writeData;
			newState.WBEND.writeData = state.MEMWB.writeData;
		
		//Data forward
			if(field1(state.MEMWB.instr) == field0(state.IFID.instr)){
				newState.IDEX.readRegA = newState.WBEND.writeData;
				//printf("WBEND Forward A\n");
			}
			if (field1(state.MEMWB.instr) == field1(state.IFID.instr)) {
				newState.IDEX.readRegB = newState.WBEND.writeData;
				//printf("WBEND Forward B\n");
			}
		
		}

		state = newState; 
    	}

}

void printState(stateType *statePtr){
	//printf("statePtr->pc = %d instrMem[statePte->pc] = %d\n", statePtr->pc, statePtr->instrMem[statePtr->pc]);
	//printf("statePtr->IFID.instr = %d\n", statePtr->IFID.instr);

   	int i;
    	printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
    	printf("\tpc %d\n", statePtr->pc);

    	printf("\tdata memory:\n");
		for (i=0; i<statePtr->numMemory; i++) {
	    		printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
		}
    	printf("\tregisters:\n");
		for (i=0; i<NUMREGS; i++) {
	    		printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
		}
   	printf("\tIFID:\n");
		printf("\t\tinstruction ");
		printInstruction(statePtr->IFID.instr);
		printf("\t\tpcPlus1 %d\n", statePtr->IFID.pcPlus1);
    	printf("\tIDEX:\n");
		printf("\t\tinstruction ");
		printInstruction(statePtr->IDEX.instr);
		printf("\t\tpcPlus1 %d\n", statePtr->IDEX.pcPlus1);
		printf("\t\treadRegA %d\n", statePtr->IDEX.readRegA);
		printf("\t\treadRegB %d\n", statePtr->IDEX.readRegB);
		printf("\t\toffset %d\n", statePtr->IDEX.offset);
    	printf("\tEXMEM:\n");
		printf("\t\tinstruction ");
		printInstruction(statePtr->EXMEM.instr);
		printf("\t\tbranchTarget %d\n", statePtr->EXMEM.branchTarget);
		printf("\t\taluResult %d\n", statePtr->EXMEM.aluResult);
		printf("\t\treadRegB %d\n", statePtr->EXMEM.readRegB);
    	printf("\tMEMWB:\n");
		printf("\t\tinstruction ");
		printInstruction(statePtr->MEMWB.instr);
	printf("\t\twriteData %d\n", statePtr->MEMWB.writeData);
    		printf("\tWBEND:\n");
		printf("\t\tinstruction ");
		printInstruction(statePtr->WBEND.instr);
	printf("\t\twriteData %d\n", statePtr->WBEND.writeData);
}

int field0(int instruction){
    	return( (instruction>>19) & 0x7);
}

int field1(int instruction){
    	return( (instruction>>16) & 0x7);
}

int field2(int instruction){
    	return(instruction & 0xFFFF);
}

int opcode(int instruction){
    	return(instruction>>22);
}

void printInstruction(int instr){
	char opcodeString[10];
    	if (opcode(instr) == ADD) {
		strcpy(opcodeString, "add");
    	} 
    	else if (opcode(instr) == NAND) {
		strcpy(opcodeString, "nand");
    	} 
    	else if (opcode(instr) == LW) {
		strcpy(opcodeString, "lw");
    	} 
    	else if (opcode(instr) == SW) {
		strcpy(opcodeString, "sw");
    	} 
    	else if (opcode(instr) == BEQ) {
		strcpy(opcodeString, "beq");
    	} 
    	else if (opcode(instr) == MULT) {
		strcpy(opcodeString, "mult");
    	} 
    	else if (opcode(instr) == HALT) {
		strcpy(opcodeString, "halt");
    	} 
    	else if (opcode(instr) == NOOP) {
		strcpy(opcodeString, "noop");
    	} 
    	else {
		strcpy(opcodeString, "data");
    	}

    	printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr), field2(instr));
}

int readInstructions(char* filename, stateType *state){
	int c_instr;
	FILE* fp;
	fp = fopen(filename, "r");
	if(!fp){
		return -1;	
	}
	
	for(state->numMemory = 0; state->numMemory < NUMMEMORY; state->numMemory++){
		fscanf (fp, "%d", &c_instr);
		if(!feof(fp)){
			state->instrMem[state->numMemory] = c_instr;
			state->dataMem[state->numMemory] = c_instr;
		}
		else{
			fclose(fp);;
			return 0;
		}
	}
	return -2;
}

int clear(stateType *state){
	for(int i=0; i < NUMREGS; i++){
		state->reg[i] = 0;
	}
	for(int i=0; i < NUMMEMORY; i++){
		state->instrMem[i] = 0;
		state->dataMem[i] = 0;
	}
	state->pc = 0;
	state->IFID.instr = NOOPINSTRUCTION;
	state->IDEX.instr = NOOPINSTRUCTION;
	state->EXMEM.instr = NOOPINSTRUCTION;
	state->MEMWB.instr = NOOPINSTRUCTION;
	state->WBEND.instr = NOOPINSTRUCTION;
	state->cycles = 0;

	//Clear Branch Predictor
	for(int i = 0; i < 4; i++){
		state->branchPredictor.predict[i] = WNT;
		state->branchPredictor.branchPC[i] = 0;
		state->branchPredictor.branchTarget[i] = 0;	
	}

	return 0;
}

int convertNum(int num){
	if (num & (1<<15) ) {
	    num -= (1<<16);
	}
	return(num);
}