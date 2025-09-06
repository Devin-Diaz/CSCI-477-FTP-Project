#include <stdio.h>

// type *P = &someVar; // P is a pointer to someVar (stores the address of someVar)
// type P = someVar; // P is a regular variable holding the value of someVar


// int main() is our method signature, returning 0 indicates a successful run, any other int would indicate some type of error
int main() {

    // Strings in C are defined as a pointer because it points to the first character of the string
    char *name = "Devin Diaz";
    // Here our %s formatter expects a pointer to a char which then proceeds to print each character in memory via the pointer
    printf("My name is %s.\n", name);

    // Printing out memory address of our name variable
    printf("Address of name: %p\n", (void *)name);

    // Same thing but with integers
    int num = 10;
    int *num_ptr = &num;
    printf("num value: %d | num address: %p\n", num,  (void *)num_ptr);

    return 0;
}