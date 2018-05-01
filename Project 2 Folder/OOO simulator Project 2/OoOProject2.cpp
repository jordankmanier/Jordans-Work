#define NUM_MEMORY 65536 /* maximum number of data words in memory */
#define NUM_REGS 8 /* number of machine registers */

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define MULT 5
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000

enum branchPredict {SNT, WNT, WT, ST};

typedef struct branchTable {
    enum branchPredict predict[4];
    int branchPC[4];
    int branchTarget[4];
} branchType;

int fetched = 0;
int retired = 0;
int branches = 0;
int mispred = 0;

int field0(int instruction);
int field1(int instruction);
int field2(int instruction);
int opcode(int instruction);

#include <iostream>
#include <vector>
#include <fstream>
using namespace std;
const int NUMMEMORY = 65000;
const int NUMREGS = 8;


/* Fetch needs to be able to fetch and contain a maximum number of instructions */
struct fetch{
    vector<int> instructionVector;
    bool branch = false;
    int maxFetch = 2;
};

/* Rename needs to be able to rename based on the current physical valuen*/
struct rename{
    int pValue = 0;
};

/* A reservation entry is ready when A and B are ready */
struct reservationEntry{
    int phA = 0;
    int phB = 0;
    int phC = 0;
    int vaA = 0;
    int vaB = 0;
    int vaC = 0;
    bool reA = 0;
    bool reB = 0;
    bool reC = 0;
    int instruction = 0;
    int pc = 0;
};

/* An ROB is indexed by its physical registers */
struct robEntry{
    int phA = 0;
    int phB = 0;
    int phC = 0;
    int vaC = 0;
    int instruction = 0;
    bool valid = false;
    bool doJump = false;
};

struct state{
    vector<reservationEntry> anb;
    vector<reservationEntry> ls;
    vector<reservationEntry> m;
    vector<reservationEntry> loadStore;
    vector<reservationEntry> multiply;
    vector<int> multiplyCount;
    struct rename renameStage;
    struct fetch fetchStage;
    bool branch = false;
    int lastM = 0;
    int lsCount = 0;
    int cycles = 0;
    int pc = 0;
    int branches = 0;
    int mispredictions = 0;
    int fetched = 0;
    int instrMem[NUMMEMORY];
    int dataMem[NUMMEMORY];
    bool valid[NUMREGS];
    int physical[NUMREGS];
    int reg[NUMREGS];
    vector<robEntry> rob;
};

int main(){
    
    state old;
    state newState;
    // insert file loader
    // insert initialization
    int i;
    for (i = 0; i < NUMREGS; i++) {
        old.valid[i] = true;
        old.physical[i] = i;
    }
    ifstream opener;
    string mcFile;
    cout << "Enter in file name NAME.txt \n";
    cin >> mcFile;
    int mcInstruction;
    opener.open(mcFile);
    int co = 0;
    while(!opener.eof()){
        opener >> mcInstruction;
        old.instrMem[co] = mcInstruction;
        cout << mcInstruction << endl;
        old.dataMem[co] = mcInstruction;
        co++;
    }
    newState = old;
    old.renameStage.pValue = 8;
    
    
    
    while(1){
        
        
        
        // Commit
        while(newState.rob.size() > 0){ // we have an instruction to commit
            if(newState.rob[0].valid){ // see if the inustruction is ready to commit
                // commit
                cout << "COMMIT " << opcode(newState.rob[0].instruction) << endl;
                if(opcode(newState.rob[0].instruction) == HALT){ // halt needs to end the simulation, insert statistics here
                    cout << "Halting.\n";
                    return 0;
                }
                else if(opcode(newState.rob[0].instruction) == ADD || opcode(newState.rob[0].instruction) == NAND || opcode(newState.rob[0].instruction == MULT)){ // these map to register c
                    for(i = 0; i < 8; i++){
                        if(newState.physical[i] == newState.rob[0].phC){ // see if any physical registers match the one from the ROB
                            newState.reg[i] = newState.rob[0].vaC; // update the value if so
                            newState.valid[i] = true; // flag the register as valid
                        }
                    }
                    cout << "Committing an ADD/NAND/MULT\n";
                }
                else if(opcode(newState.rob[0].instruction) == BEQ){ // we're committing a branch
                    if(newState.rob[0].doJump){ // the branch told us to jump so change the pc
                        cout << "Branching\n";
                        newState.branch = true; // we need to branch
                        newState.pc = newState.rob[0].vaC;    // update the pc
                        // insert branch prediction correction HERE
                    }
                    else{
                        // insert branch prediction correction here
                    }
                }
                newState.rob.erase(newState.rob.begin());    // erase the entry
            }
            break;
        }
        while(newState.rob.size() > 0){
            if(newState.rob[0].valid){
                // commit
                cout << "COMMIT " << opcode(newState.rob[0].instruction) << endl;
                if(opcode(newState.rob[0].instruction) == HALT){
                    cout << "Halting\n";
                    return 0;
                }
                else if(opcode(newState.rob[0].instruction) == ADD || opcode(newState.rob[0].instruction) == NAND || opcode(newState.rob[0].instruction == MULT )){
                    for(i = 0; i < 8; i++){
                        if(newState.physical[i] == newState.rob[0].phC){
                            newState.reg[i] = newState.rob[0].vaC;
                            newState.valid[i] = true;
                        }
                    }
                    cout << "Committing an ADD/NAND/BEQ\n";
                }
                else if(opcode(newState.rob[0].instruction) == BEQ){
                    if(newState.rob[0].doJump){
                        newState.branch = true;
                        newState.pc = newState.rob[0].vaC;
                    }
                }
                newState.rob.erase(newState.rob.begin());
            }
            break;
        }
        
        // add beq nand
        for(i = 0; i < newState.anb.size(); i++){ // may be executed in one cycle
            if(newState.anb[i].reA && newState.anb[i].reB){ // we found one we can work on
                int ind = i;
                cout << "Found instruction " << i << " oc" << opcode(newState.anb[i].instruction) << "\n";
                int code = opcode(newState.anb[i].instruction);  // get the code for the instruction
                int calculation = 0; // store the calulcated value
                if(code == ADD){
                    calculation = newState.anb[i].vaA + newState.anb[i].vaB; // add so add them
                }
                else if (code == NAND){
                    calculation = ~(newState.anb[i].vaA & newState.anb[i].vaB); // nand so nand them bitches
                }
                else{ // otherwise were a beq
                    // do beq stuff
                    for(int j = 0; j < newState.rob.size(); j++){
                        if(newState.rob[j].phA == newState.anb[i].phA && newState.rob[j].phB == newState.anb[i].phB){ // we need to branch
                            if(newState.anb[i].vaA == newState.anb[i].vaB){
                                newState.rob[j].doJump = true;
                                newState.rob[j].vaC = newState.anb[i].pc + 1 + field2(newState.rob[j].instruction);
                            }
                            else{
                                newState.rob[j].doJump = false; // don't need to branch
                            }
                            newState.rob[j].valid = true; // update ROB entry
                            break;
                        }
                    }
                }
                if(code == ADD || code == NAND){ // add and nand need to broadcast to other stations
                    for(int j = 0; j < newState.rob.size(); j++){ // update the ROB entry
                        if(newState.rob[j].phA == newState.anb[ind].phA && newState.rob[j].phB == newState.anb[ind].phB){
                            newState.rob[j].valid = true;
                            newState.rob[j].vaC = calculation;
                            i = j;
                        }
                    }
                    for(int j = 0; j < newState.anb.size(); j++){ // go through each station
                        if(newState.anb[j].phA == newState.rob[i].phC){ // is a regA entry waiting for the physical reg we just calulcated?
                            newState.anb[j].reA = true; // then flag it as valid
                            newState.anb[j].vaA = newState.rob[i].vaC; // and update its value
                        }
                        if(newState.anb[j].phB == newState.rob[i].phC){ // ditto for B
                            newState.anb[j].reB = true;
                            newState.anb[j].vaB = newState.rob[i].vaC;
                        }
                    }
                    for(int j = 0; j < newState.ls.size(); j++){ // do the same for the LS station
                        if(newState.ls[j].phA == newState.rob[i].phC){
                            newState.ls[j].reA = true;
                            newState.ls[j].vaA = newState.rob[i].vaC;
                        }
                        if(newState.ls[j].phB == newState.rob[i].phC){
                            newState.ls[j].reB = true;
                            newState.ls[j].vaB = newState.rob[i].vaC;
                        }
                    }
                    for(int j = 0; j < newState.m.size(); j++){ // do the same for the mult station
                        if(newState.m[j].phA == newState.rob[i].phC){
                            newState.m[j].reA = true;
                            newState.m[j].vaA = newState.rob[i].vaC;
                        }
                        if(newState.m[j].phB == newState.rob[i].phC){
                            newState.m[j].reB = true;
                            newState.m[j].vaB = newState.rob[i].vaC;
                        }
                    }
                    
                }
                newState.anb.erase(newState.anb.begin() + ind); // now erase the entry
                break;
            }
        }
        
        
        
        
        // load store
        
        // executing an instruction
        if(newState.loadStore.size() > 0){ // we're currently executing an instruction
            newState.lsCount++;
            if(newState.lsCount == 3){ // we reached the three cycles to finish execution of a load or store
                cout << " Finshed a load store \n";
                if(opcode(newState.loadStore[0].instruction) == LW){ // we need to load from memory
                    for(i = 0; i < 8; i++){
                        if(newState.physical[i] == newState.loadStore[0].phB){
                            newState.reg[i] = newState.dataMem[newState.loadStore[0].vaA  + field2(newState.loadStore[0].instruction)]; // load value from memory
                            newState.valid[i] = true;
                            for(int j = 0; j < newState.anb.size(); j++){ // broadcast to reservation stations
                                if(newState.anb[j].phA == newState.loadStore[0].phB){
                                    newState.anb[j].reA = true;
                                    newState.anb[j].vaA = newState.reg[i];
                                }
                                if(newState.anb[j].phB ==  newState.loadStore[0].phB){
                                    newState.anb[j].reB = true;
                                    newState.anb[j].vaB = newState.reg[i];
                                }
                            }
                            for(int j = 0; j < newState.ls.size(); j++){
                                if(newState.ls[j].phA ==  newState.loadStore[0].phB){
                                    newState.ls[j].reA = true;
                                    newState.ls[j].vaA = newState.reg[i];
                                }
                                if(newState.ls[j].phB ==  newState.loadStore[0].phB){
                                    newState.ls[j].reB = true;
                                    newState.ls[j].vaB = newState.reg[i];
                                }
                            }
                            for(int j = 0; j < newState.m.size(); j++){
                                if(newState.m[j].phA ==  newState.loadStore[0].phB){
                                    newState.m[j].reA = true;
                                    newState.m[j].vaA = newState.reg[i];
                                }
                                if(newState.m[j].phB ==  newState.loadStore[0].phB){
                                    newState.m[j].reB = true;
                                    newState.m[j].vaB = newState.reg[i];
                                }
                            }
                        }
                    }
                }
                else{
                    newState.dataMem[newState.loadStore[0].vaA + field2(newState.loadStore[0].instruction)] = newState.loadStore[0].vaC; // store to memory
                }
                for(i = 0; i < newState.rob.size(); i++){
                    if(newState.rob[i].phA == newState.loadStore[0].phA && newState.rob[i].phB == newState.rob[i].phB){ // flag the ROB entry as valid
                        newState.rob[i].valid = true;
                    }
                }
                newState.loadStore.erase(newState.loadStore.begin()); // now erase the instruction we just completed
                newState.lsCount = 0; // reset execution count to 0
            }
        }
        // otherwise grab an instruction
        else{
            for(i = 0; i < newState.ls.size(); i++){
                if(opcode(newState.ls[i].instruction) == SW){
                    if(newState.ls[i].reB && newState.ls[i].reA){ // SW needs regB and regA to be valid
                        cout << "Loaded a SW to execute\n";
                        newState.loadStore.emplace_back(newState.ls[i]);
                        newState.ls.erase(newState.ls.begin() + i);
                        break;
                    }
                }
                else if (opcode(newState.ls[i].instruction) == LW  && newState.ls[i].reA){ // LW just needs A for the offset to be valid
                    newState.loadStore.emplace_back(newState.ls[i]);
                    cout << "Loaded a LW to execute\n";
                    newState.ls.erase(newState.ls.begin() + i);
                    break;
                }
            }
        }
        
        
        // multiply store
        // try to fetch a newState instruction
        if(newState.lastM > 1){ // last multiply was 2 or more cycles ago so we can fetch a new one since its pipelined
            for(i = 0; i < newState.m.size(); i++){
                if(newState.m[i].reA && newState.m[i].reB){ // find a valid multiply which is waiting and commit it
                    cout << "Loaded a mult to execute\n";
                    newState.multiply.emplace(newState.multiply.begin(), newState.m[i]);
                    newState.multiplyCount.emplace(newState.multiplyCount.begin(),0);
                    newState.m.erase(newState.m.begin() + i);
                    break;
                }
            }
        }
        else{
            newState.lastM++;
        }
        // now do multiply work
        for(i = 0; i < newState.multiply.size(); i++){
            if(newState.multiplyCount[i] > 5){ // a multiply reached the end of its execution
                int calculation = newState.multiply[i].vaA * newState.multiply[i].vaB; // get the multiplied value
                for(int j = 0; j < newState.rob.size(); j++){
                    if(newState.rob[j].phC == newState.multiply[i].phC && newState.rob[j].phA == newState.multiply[i].phA){
                        newState.rob[j].valid = true;
                        newState.rob[j].vaC = calculation;
                        i = j;
                    }
                }
                for(int j = 0; j < newState.anb.size(); j++){
                    if(newState.anb[j].phA == newState.rob[i].phC){ // broadcast
                        newState.anb[j].reA = true;
                        newState.anb[j].vaA = newState.rob[i].vaC;
                    }
                    if(newState.anb[j].phB == newState.rob[i].phC){
                        newState.anb[j].reB = true;
                        newState.anb[j].vaB = newState.rob[i].vaC;
                    }
                }
                for(int j = 0; j < newState.ls.size(); j++){
                    if(newState.ls[j].phA == newState.rob[i].phC){
                        newState.ls[j].reA = true;
                        newState.ls[j].vaA = newState.rob[i].vaC;
                    }
                    if(newState.ls[j].phB == newState.rob[i].phC){
                        newState.ls[j].reB = true;
                        newState.ls[j].vaB = newState.rob[i].vaC;
                    }
                }
                for(int j = 0; j < newState.m.size(); j++){
                    if(newState.m[j].phA == newState.rob[i].phC){
                        newState.m[j].reA = true;
                        newState.m[j].vaA = newState.rob[i].vaC;
                    }
                    if(newState.m[j].phB == newState.rob[i].phC){
                        newState.m[j].reB = true;
                        newState.m[j].vaB = newState.rob[i].vaC;
                    }
                }
                newState.multiplyCount.erase(newState.multiplyCount.begin() + i);
                newState.multiply.erase(newState.multiply.begin() + i);
                break;
            }
            else{
                newState.multiplyCount[i]++;
            }
        }
        
        /*
         Fetch operates until up to 2 instructions are fetched, the rob must not be full. A branch terminates following a fetched BEQ
         */
        newState.fetchStage.branch = false;
        while(newState.fetchStage.instructionVector.size() < 2 && old.rob.size() < 16 && !newState.fetchStage.branch){
            cout << "Fetched an instruction" << opcode(newState.instrMem[newState.pc]) << "\n";
            newState.fetchStage.instructionVector.emplace_back(newState.pc);
            newState.pc++;
            if(opcode(newState.instrMem[newState.pc - 1]) == BEQ){
                newState.fetchStage.branch = true;
                break;
            }
        }
        
        // RENAME
        while(newState.fetchStage.instructionVector.size() > 0){
            int instruction = newState.instrMem[newState.fetchStage.instructionVector[0]]; // get the instruction
            int type = opcode(instruction); // get its type
            reservationEntry entry; // create an entry with the proper values
            entry.phA = newState.physical[field0(instruction)];
            entry.reA = newState.valid[field0(instruction)];
            entry.phB = newState.physical[field1(instruction)];
            entry.reB = newState.valid[field1(instruction)];
            entry.vaA = newState.reg[field0(instruction)];
            entry.vaB = newState.reg[field1(instruction)];
            entry.instruction = instruction; // also store the instruction
            entry.pc = newState.fetchStage.instructionVector[0]; // store the pc
            if(type == ADD || type == NAND || type == MULT){
                entry.phC = newState.renameStage.pValue; //these instructions modify register C
            }
            if(type == NOOP || type == HALT){ // commit straight to the ROB
                cout << "Fetched a NOOP or HALT\n";
                robEntry entr;
                entr.instruction = instruction;
                entr.valid = true;
                newState.rob.emplace_back(entr);
                newState.fetchStage.instructionVector.erase(newState.fetchStage.instructionVector.begin());
            }
            
            if(type == ADD || type == NAND || type == BEQ){ // add an instruction to add nand or beq
                cout << "Renaming ADD or NAND or Branch\n";
                if (newState.anb.size() < 3){
                    if(type == ADD || type == NAND){
                        newState.physical[field2(instruction)] = entry.phC;
                        newState.renameStage.pValue = (newState.renameStage.pValue + 1) % 50;
                        newState.valid[field2(instruction)] = false;
                    }
                    newState.anb.emplace_back(entry);
                    robEntry toCommit;
                    toCommit.phA = entry.phA;
                    toCommit.phB = entry.phB;
                    toCommit.phC = entry.phC;
                    toCommit.instruction = entry.instruction;
                    newState.rob.emplace_back(toCommit);
                    newState.fetchStage.instructionVector.erase(newState.fetchStage.instructionVector.begin());
                }
                else{
                    break;
                }
            }
            else if(type == LW || type == SW){ // add an instruction to lw or sw
                if (newState.ls.size() < 3){
                    if(type == LW){
                        entry.phB = newState.renameStage.pValue;
                        newState.physical[field1(instruction)] = entry.phB;
                        newState.valid[field1(instruction)] = false;
                        newState.renameStage.pValue = (newState.renameStage.pValue + 1) % 50;
                    }
                    newState.ls.emplace_back(entry);
                    robEntry toCommit;
                    toCommit.phA = entry.phA;
                    toCommit.phB = entry.phB;
                    toCommit.phC = entry.phC;
                    toCommit.instruction = entry.instruction;
                    newState.rob.emplace_back(toCommit);
                    newState.fetchStage.instructionVector.erase(newState.fetchStage.instructionVector.begin());
                }
                else{
                    break;
                }
            }
            else if(type == MULT){ // otherwise were adding an instruction to the multiply station
                if(newState.m.size() < 3){
                    newState.physical[field2(instruction)] = entry.phC;
                    newState.renameStage.pValue = (newState.renameStage.pValue + 1) % 50;
                    newState.valid[field2(instruction)] = false;
                    newState.m.emplace_back(entry);
                    robEntry toCommit;
                    toCommit.phA = entry.phA;
                    toCommit.phB = entry.phB;
                    toCommit.phC = entry.phC;
                    toCommit.instruction = entry.instruction;
                    newState.rob.emplace_back(toCommit);
                    newState.fetchStage.instructionVector.erase(newState.fetchStage.instructionVector.begin());
                }
                else{
                    break;
                }
            }
        }
        cout << "ROB Validity: ";
        for(i = 0; i < newState.rob.size(); i++){
            cout << opcode(newState.rob[i].instruction) << "-" << newState.rob[i].valid << " ";
        }
        cout << endl;
        newState.cycles++;
    }
    cout << "Simulation terminated\n";
    return 0;
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
