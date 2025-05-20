#include <stdio.h>
#include <stdint.h>
#include "xoshiro128plusplus.hpp"

extern uint32_t s[4];
uint32_t next(void);

Xoshiro128plusplus rng;

static void setState(int i, uint32_t value) {
    s[i] = value;
    rng.state[i] = value;
}

int main(int argc, char** argv) {
    setState(0, 0x12345678);
    setState(1, 0x23456789);
    setState(2, 0x34567890);
    setState(3, 0x45678901);

    constexpr int N = 0x1000000;

    int numFail = 0;
    int numSuccess = 0;
    for (int i = 0; i < N; i++) {
        if (rng.next() == next()) {
            numSuccess++;
        } else {
            numFail++;
        }
    }

    printf("Success: %d, Fail: %d\n", numSuccess, numFail);

    if (numFail == 0 && numSuccess == N) {
        printf("Test passed!\n");
    } else {
        printf("Test failed!\n");
    }
}
