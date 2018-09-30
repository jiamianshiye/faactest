//#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "pulse.h"

int main(int argc, char *argv[])
{
    printf("hello world!\n");
    pulse_init();

    pulse_loop_in();
    return 0;
}
