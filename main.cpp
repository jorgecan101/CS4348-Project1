#include <iostream> //for c++ stuff
#include <stdio.h>
#include <fstream> //file IO
#include <sys/types.h>
#include <sys/wait.h> //For creating forks and processes
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
using namespace std;

//int *openFile(string fileName);
int *openFile(const char* fileName);
void instructions(string fileName, int timer);
bool timerChecker(int timer, int timerCount);
void unreachableMemory(int address, bool kernelMode);
int getData(int *cpu2mem, int *mem2cpu, int PC, int data);
int getAddress(int *cpu2mem, int *mem2cpu, int PC, int address);
int address2data(int *cpu2mem, int *mem2cpu, int address, int data);
int same2same(int *cpu2mem, int *mem2cpu, int value);
int reg2reg(int *cpu2mem, int *mem2cpu, int reg1, int reg2);
int writeInto(int *cpu2mem, int writeCommand, int stored, int returned);

//void timerInterruptCheck(bool timerInterrupt, int interruptInstances, bool kernelMode, int operand, int SP, int PC,
//int cpu2mem[], int writeCommand);

int main(int argc, char *argv[]){
    //Start by taking the arguments
    string fileName = argv[1];
    int timer = atoi(argv[2]);
    //TODO: ADD THINGS TO CHECK FOR FILE AND TIMER VALIDITY
    if (timer < 0) {
        cout << "Invalid timer number entered!" <<endl;
        return 1;
    }
    //One thing to note about the timer is that whenever the timer is too small, the program might
    //end up in a loop forever. However this is supposed to be like that it seems.
    //Begins to read file and fetch instructions
    instructions(fileName, timer);
    return 0;
}

int *openFile(const char* fileName){
    FILE *filePtr; //for opening files 
    char temp[1000]; //temporarly holds values for us to parse through
    static int memory[2000]; //our memory array that will be used for everything
    int currLine = 0; //goes to next lines/based on what line we at atm

    // zero out the entire array to remove garbage numbers
    int n = sizeof(memory)/sizeof(memory[0]);
    for (size_t i = 0; i < n; i++) {
        memory[i]=0;
    }
    //open file to be parsed 
    filePtr =fopen(fileName,"r"); //using FILE *ptr logic from C
    if (!filePtr){
        //if the file is bad, then this case happens and program ends
        cout << "Invalid or nonexistent file entered" << endl;
        exit(0);
    }

    while (fgets(temp, sizeof(temp), filePtr) != NULL){
        //loop will go through the file, and temporarly hold values in temp array
        int i = (int) temp[0]; //grabs if number
        char c = (char) temp[0]; //grabs if . (like for .1000, etc)
        if (isdigit(i) == 1 || (temp[0] == '.')){
            //since we want to ignore comments and all other sort of cases, we check if we have number or .
            int num; 
            if(temp[0] == '.'){
                //if it is a period specifically
                //Reads data from s and stores them according to parameter format into the locations given by the additional arguments
                sscanf(temp, ".%d %*s\n", &num);
                currLine = num;
                //we add it to the array memory
                memory[currLine] = num;
            }
            else{
                //Reads data from s and stores them according to parameter format into the locations given by the additional arguments
                sscanf(temp, "%d %*s\n", &num);
                memory[currLine] = num;
                //since we are at a number, and its not before a period, we can move to next line 
                currLine++;
            }
        }
    }
    fclose(filePtr);
/* Checking if the file correctly parsed the data
        for (int i = 0; i < n; i++) {
            cout << memory[i] << endl;
        }
*/
    return memory;
}

void instructions(string fileName, int timer) {
    //cout << "Hello world, from instructions!" << endl;
    //The memory that stores 2000 integer entries, where the first half is for the user program and the second half is for system code
    //User program starts at address 0
    //int address = 0; //temp
    //int data = 0; //temp
    int cpu2mem[2];
    int mem2cpu[2];
    int res1 = pipe(cpu2mem);
    int res2 = pipe(mem2cpu);
    //result = pipe(pdfs);
    if (res1 == -1 || res2 == -1) {
    	//Failure in creating a pipe for either cpu2mem or mem2cpu
	    cout << "Failed to create a pipe" << endl;
	    exit(3); //Terminate the program due to process creation failure
    }
    //fflush(0);
    int forkRes = fork();
    if (forkRes == -1) {
        //Failure in creating a fork in the processes
        cout << "Fork Failure" << endl;
        exit(1); //Terminate the program due to process creation failure
    }
	//---------------MEMORY STUFF--------------
    if (forkRes == 0) {
        //If we have reached forkRes of 0, then that means that we are going to be able to access the memory
        //cout << "Writing into the pipe as a child (AKA MEMORY)" << endl;
        //Inside the child, so can take it as the memory 

        /*
        Some notes to mention about memory:
        0-999 is reserved for user
        1000-1999 is reserved for the system code
        Must be able to read and write (no real logic behind read and write)
        Reads program file into array before CPU fetching begins (done)
        */      
        //So we should start by reading the file from openfile into memory
        int *memory;
        memory = openFile(fileName.c_str());
        //Initialize variables needed such as as PC, and our data and address for read and write
        int PC;
        int data;
        int address;
        bool inMem = true;
        //Loop for indicating we are in our memory
        while (inMem) {
            //Memory instructions should be able to read from the CPU based on current PC
            read(cpu2mem[0], &PC, sizeof(PC));
            if (PC == -1) { 
                //when our PC is -1, it should indicate that we must write something
                read(cpu2mem[0], &address, sizeof(address)); //we take in the address we want to write at
                read(cpu2mem[0], &data, sizeof(data)); //we get the data we want to write at
                memory[address] = data; //we write into it 
            }
            else { 
                //we take our instruction from the cpu and read it
                int instruction = memory[PC];
                write(mem2cpu[1], &instruction, sizeof(instruction));
            }
        }
    }
    //--------------------------CPU STUFF------------------------------
    else {
        //cout << "Reading from the parent" << endl;
        //Do parent stuff inside
	    //Here we will read from the memory and instructions

    	//Registers:
    	//These include: PC(Program Counter), SP(Stack Pointer), IR(Instruction Register), AC(Accumulator), X, Y
		//int PC, SP, IR, AC, X, Y;
        int PC = 0;
        int SP = 1000; //Stack Pointer should start at 1000 for user (should be 999?)
        int IR = 0;
        int AC = 0;
        int X = 0;
        int Y = 0;

        //Operand can be fetched into a local variable
        int data = 0;
        int address = 0;
        int operand = 0;

        //We need a way to know whether we are in kernel mode or in user mode
        //We can use booleans to understand
        bool kernelMode = false;

        //We need to be able to increment the timer in order to ever reach timer interrupts
        int timerCount = 0;
        //Using writeCommand would be a symbol that we need to write something into our data
        int writeCommand = -1;

        //Before we move on we need to address interrupts and how to spot them
        //There are timer interrupts and system call interrupts
        bool timerInterrupt = false;
        bool systemInterrupt = false;
        //indicate types of interrupts that we might be in
        //0 means none, -1 means timer, 1 means syscall
        int interruptInstances = 0; 

        bool inCPU = true;
        while (inCPU) {
            //Before we start looking at cases, we need to know if we are in an interrupt for timing
            //if (timerInterrupt == true && systemInterrupt == false) {
            if (timerInterrupt == true && interruptInstances == 0) {

                //Doing this we know we are in a timer interrupt, and not in a system interrupt
                //We can get rid of timerInterrupt now since we are in it currently
                timerInterrupt = false;
                interruptInstances = -1;
                //Since we have confimed that we are in an interrupt, we must switch to kernel mode
                kernelMode = true;
                //Then we must switch our stack pointer to system stack

                //We need to hold our original SP value with a temp
                operand = SP; //since i defined operand earlier and haven't used it yet, we can use it to hold stack
                SP = 2000;

                //We need to save the SP and PC registers on the stack by the CPU
                SP--;
                PC++;
                PC = writeInto(cpu2mem, writeCommand, SP, PC);
               
                //Saving the other value here
                SP--;
                operand = writeInto(cpu2mem, writeCommand, SP, operand);
                //We also have to move our program counter to 1000 since we are in a timer interrupt here
                PC = 1000;
            }

            //We want to start by fetching an instruction from memory to look into
            IR = reg2reg(cpu2mem, mem2cpu, PC, IR);

            //Now we have our cases to look into what exactly we want our program to do
            switch(IR){
                case 1:
                    //Case 1: Load the value into the AC
                    PC++; //We must first increment the PC to the instruction we're on
                    //Gets the data(value) so it can be put into the AC
                    data = getData(cpu2mem, mem2cpu, PC, data); 
                    //loads value into AC
                    AC = data;
                    break;
                case 2:
                    //Case 2: Load the value at the address into the AC
                    PC++;
                    //Note: The difference here is that we want to load from address rather than the data
                    //Gets the address so we can find the value at it
                    address = getAddress(cpu2mem, mem2cpu, PC, address);
                    //Now we want to make sure that the user does not enter space that is invalid
                    unreachableMemory(address, kernelMode);
                    //Gets the value/data at the address to put into AC
                    data = address2data(cpu2mem, mem2cpu, address, data);
                    //loads value into AC
                    AC = data;
                    break;
                case 3:
                    //Case 3: Load the value from the address found in the given address into the AC
                    //So we want an address that points to another address??
                    PC++;
                    //We start by loading from the addess
                    address = getAddress(cpu2mem, mem2cpu, PC, address);
                    //Now we want to get the value at that address
                    address = same2same(cpu2mem, mem2cpu, address);
                    //Now we want to make sure that the user does not enter space that is invalid
                    unreachableMemory(address, kernelMode);
                    //we want to get the data we got from the address so we can put it into data
                    data = address2data(cpu2mem, mem2cpu, address, data);
                    //loads value into AC
                    AC = data;
                    break;
                case 4:
                    //Case 4: Load the value at (address + X) into the AC
                    PC++;
                    //get the address so we can use it
                    address = getAddress(cpu2mem, mem2cpu, PC, address);
                    //make it address + X since we need it for this case
                    address = address + X;
                    //We should do another bounds check to see if we are at the bounds for user
                    unreachableMemory(address, kernelMode);
                    //get the data from the address
                    data = address2data(cpu2mem, mem2cpu, address, data);
                    //loads value into the AC
                    AC = data;
                    break;
                case 5:
                    //Case 5: Load the value at (address + Y) into the AC
                    PC++;
                    //get address so we can use it
                    address = getAddress(cpu2mem, mem2cpu, PC, address);
                    //make it address + Y for this case
                    address = address + Y;
                    //We should do another bounds check to see if we are at the bounds for user
                    unreachableMemory(address, kernelMode);
                    //get data from the address
                    data = address2data(cpu2mem, mem2cpu, address, data); 
                    //loads value into the AC
                    AC = data;
                    break;
                case 6:
                    //Case 6: Load from (SP + X) into the AC (if SP is 990, and X is 1, load from 991)
                    //PC++;
                    if (SP == 990 && X == 1) {
                        //We load from 991
                        //Not sure if this is an example or some specific case to look out for
                        data = 991;
                    }
                    else {
                        data = SP + X;
                    }
                    //Asks for value at SP + X, which is one of our registers, so we use 
                    //same2same here
                    data = same2same(cpu2mem, mem2cpu, data);
                    //loads value into the AC
                    AC = data;
                    break;
                case 7:
                    //Case 7: Store the value in the AC into the address
                    PC++;
                    //gets data we want thats in memory
                    data = getData(cpu2mem, mem2cpu, PC, data);
                    //write it into the address AC
                    AC = writeInto(cpu2mem, writeCommand, address, AC);
                    break;
                case 8:
                    //Case 8: Gets a random int from 1 to 100 into the AC
                    srand(time(NULL)); //srand with a seed in order to have true pseudorandomness
                    AC = rand() % 100 + 1;
                    //PC++;
                    break;
                case 9:
                    //Case 9: If port=1, writes AC as an int to the screen, if port=2, write AC as a char
                    PC++;
                    //Note here: In this case, data is port
                    data = getData(cpu2mem, mem2cpu, PC, data);
                    //int port = data;
                    if (data == 1) {
                        //print as int
                        cout << (int)AC;
                    }
                    if (data == 2) {
                        cout << (char)AC;
                        //print as char 
                    }
                    break;
                //Seems to give issues if incrementing PC inside case 10-19?? Not needed? Since increments at end!
                case 10:
                    //Case 10: Add the value of X into the AC
                    AC = AC + X;
                    //PC++; 
                    break;
                case 11:
                    //Case 11: Add the value of Y into the AC
                    AC = AC + Y;
                    //PC++;
                    break;
                case 12:
                    //Case 12: Subtract the value in X from the AC
                    AC = AC - X;
                    //PC++;
                    break;
                case 13:
                    //Case 13: Subtract the value in Y from the AC
                    AC = AC - Y;
                    //PC++;
                    break;
                case 14:
                    //Case 14: Copy the value in the AC to X
                    X = AC;
                    //PC++;
                    break;
                case 15:
                    //Case 15: Copy the value in X to the AC
                    AC = X;
                    //PC++;
                    break;
                case 16:
                    //Case 16: Copy the value in the AC to Y
                    Y = AC;
                    //PC++;
                    break;
                case 17:
                    //Case 17: Copy the value in Y to the AC
                    AC = Y;
                    //PC++;
                    break;
                case 18:
                    //Case 18: Copy the value in AC to the SP
                    SP = AC;
                    //PC++;
                    break;
                case 19:
                    //Case 19: Copy the value in SP to the AC
                    AC = SP;
                    //PC++;
                    break;
                case 20:
                    //Case 20: Jump to the address
                    PC++;
                    //get the address that we want to jump to
                    address = getAddress(cpu2mem, mem2cpu, PC, address);
                    //jump to address
                    //should be correct, since we have to account for extra PC from EOF
                    PC = address - 1; 
                    break;
                case 21:
                    //Case 21: Jump to the address only if the value in the AC is zero
                    PC++;
                    //gets the address we want to jump to
                    address = getAddress(cpu2mem, mem2cpu, PC, address);
                    if (AC == 0) {
                        //Now we jump into the address knowing that AC is 0
                        //should be correct, since we have to account for the extra PC from EOF
                        PC = address - 1;
                    }
                    break;
                case 22:
                    //Case 22: Jump to the address only if the value in the AC is not zero
                    PC++;
                    //gets address we want to jump to
                    address = getAddress(cpu2mem, mem2cpu, PC, address);
                    if (AC != 0) {
                        //Now we jump to the address knowing AC is not 0
                        //should be correct, since we have to account for the extra PC from EOF
                        PC = address - 1;
                    }
                    break;
                case 23:
                    //Case 23: Push return address onto onto the stack, jump to the address
                    PC++;
                    //get the return address we are going to push into the stack
                    address = getAddress(cpu2mem, mem2cpu, PC, address);
                    //push the return address we got onto the stack
                    SP--; //do it with SP decrement
                    PC++; //fix program counter
                    //store address into the stack pointer
                    PC = writeInto(cpu2mem, writeCommand, SP, PC);
                    //now we jump to the address
                    //should be correct, since we have to account for te extra PC from EOF
                    PC = address - 1;
                    break;
                case 24:
                    //Case 24: Pop return address from the stack, jump to the address
                    //Use reg2reg to pop the return address and jump to our address
                    PC = reg2reg(cpu2mem, mem2cpu, SP, PC);
                    SP++;
                    PC--; //rm from PC since we popped
                    break;
                case 25:
                    //Case 25: Increment the value in X
                    X++;
                    break;
                case 26:
                    //Case 26: Decrement the value in X
                    X--;
                    break;
                case 27:
                    //Case 27: Push AC onto the stack
                    SP--;
                    //We want to use writeInto to indicate we are pushing our AC into the stack
                    AC = writeInto(cpu2mem, writeCommand, SP, AC);
                    break;
                case 28:
                    //Case 28: Pop from stack into the AC
                    //using reg2reg similar to the other case with popping
                    //we take whats at SP and save what is at the AC
                    AC = reg2reg(cpu2mem, mem2cpu, SP, AC);
                    SP++; //increment our stack pointer
                    break;
                case 29:
                    //Case 29: Perform syscall
                    //Note: Performing syscall should put us in a interrupt that we must check for
                    //if (timerInterrupt == true || systemInterrupt == true) {
                    if (interruptInstances != 0) {
                        //we use this to check if we are already in an interrupt
                        break;
                    }
                    interruptInstances = 1;

                    //systemInterrupt = true;
                    kernelMode = true; //switch to kernelMode
                    operand = SP; //must hold this temperarly
                    SP = 2000;

                    SP--;
                    //We want to save our PC and SP to the stack since those are the only ones that should be saved
                    PC = writeInto(cpu2mem, writeCommand, SP, PC);
                    SP--;
                    //We also need to save our original stack pointer saved at operand into the SP so we can get it
                    operand = writeInto(cpu2mem, writeCommand, SP, operand);
                    PC = 1499; //Since we are in a syscall, we want to move to address 1499 (1500 at the end)
                    break;
                case 30:
                    //Case 30: Return syscall
                    //we want to pop the return address from the stack for SP
                    operand = reg2reg(cpu2mem, mem2cpu, SP, operand);
                    SP++;
                    //we want a reg2reg here to pop and get PC back
                    PC = reg2reg(cpu2mem, mem2cpu, SP, PC);
                    PC = PC - 2;
                    SP++;

                    //Remove interrupts
                    //if (timerInterrupt == true) {
                    if (interruptInstances == -1) {
                        timerInterrupt = false;
                    }
                    interruptInstances = 0;
                    //systemInterrupt = false; //in case
                    SP = operand;
                    kernelMode = false;

                    break;
                case 50:
                    //Case 50: End execution
                    exit(0);
                    break;
                    //break; //Since we are ending execution here there should be no need for the break after
                default:
                    //If we ever reach here, then we know there is something wrong since it would be an invalid instruction
                    cout << "Invalid Instruction Reached at: " << IR << endl;
            }
            
            //Since timer will always count no matter if we are in user mode or kernel mode, we can increment at the end
            timerCount++;
            PC++;
            //Once done we can check if the timer has reached a possible interrupt
            if (timerChecker(timer, timerCount)) {
                timerInterrupt = true;
            }   
        }  
        waitpid(-1,NULL,0); //Must wait for child to be written into first/ finish first
    }
}

bool timerChecker(int timer, int timerCount) {
    if (timerCount % timer == 0) {
        //if it reaches every X amount of instructions, then we have a timer interrupt
        return true;
    }
    //Same as, else return false.
    return false;
}
void unreachableMemory(int address, bool kernelMode) {
    if (address >= 1000 && kernelMode == false) {
        //output based on violation given in sample
        cout << "Memory violation: accessing system address " << address << " in user mode " << endl;
        exit(2); //exit with 2 (just used so i know it worked)
    }
}
//We can make a function to read from memory to make things more easier to look at in our switch statements
int getData(int *cpu2mem, int *mem2cpu, int PC, int data){
    //Goes and asks memory for the data using the PC
    write(cpu2mem[1], &PC, sizeof(PC));
    //From what is read in the memory, return the data
    read(mem2cpu[0], &data, sizeof(data));
    return data;
}
int getAddress(int *cpu2mem, int *mem2cpu, int PC, int address) {
    //Goes and asks memory for what address we are looking for
    write(cpu2mem[1], &PC, sizeof(PC));
    //from what is read, return the address gotten
    read(mem2cpu[0], &address, sizeof(address));
    return address;
}
int address2data(int *cpu2mem, int *mem2cpu, int address, int data) {
    //Goes and asks memory for what address we are looking for
    write(cpu2mem[1], &address, sizeof(address));
    //gets the data at that address
    read(mem2cpu[0], &data, sizeof(data));
    return data;
}
int same2same(int *cpu2mem, int *mem2cpu, int value) {
    //Used for getting 1 same value into another same value 
    //ex: write and read both using address, or both using data
    write(cpu2mem[1], &value, sizeof(value));
    read(mem2cpu[0], &value, sizeof(value));
    return value;
}
int reg2reg(int *cpu2mem, int *mem2cpu, int reg1, int reg2){
    //Used for taking registers like AC, PC, IR ,etc with write and read commands
    write(cpu2mem[1], &reg1, sizeof(reg1));
    read(mem2cpu[0], &reg2, sizeof(reg2));
    return reg2;
}
int writeInto(int *cpu2mem, int writeCommand, int stored, int returned){
    write(cpu2mem[1], &writeCommand, sizeof(writeCommand)); //Tell that we are writing
    write(cpu2mem[1], &stored, sizeof(stored)); //store it
    write(cpu2mem[1], &returned, sizeof(returned)); //return what we want
    return returned;
}