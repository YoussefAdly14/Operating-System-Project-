#include <stdio.h>
#include "memory.h"
#include "process.h"

int main() {
    clearMemory();

    createProcess(1, "programs/Program_1.txt", 0);
    createProcess(2, "programs/Program_2.txt", 1);

    printMemory();
    printProcess(1);
    printProcess(2);

    return 0;
}