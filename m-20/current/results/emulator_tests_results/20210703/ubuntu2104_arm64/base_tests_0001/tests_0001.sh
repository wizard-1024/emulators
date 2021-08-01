#!/bin/sh

# clean all
#rm -f *.lst *.out *.cdp *.err *.txt
#exit 0

# variables
M20RU="./m20ru"

# Emulator
${M20RU}  m-20.simh >m-20_ru.out 2>m-20_ru.err

# hello
${M20RU} hello.simh >hello_ru.out 2>hello_ru.err
${M20RU} hello1.simh >hello1_ru.out 2>hello1_ru.err
${M20RU} hello2.simh >hello2_ru.out 2>hello2_ru.err

# add
${M20RU} add_0001.simh >add_0001_ru.out 2>add_0001_ru.err
${M20RU} add_0002.simh >add_0002_ru.out 2>add_0002_ru.err

# samples
${M20RU} sample_0031.simh >sample_0031_ru.out 2>sample_0031_ru.err
${M20RU} sample_0053.simh >sample_0053_ru.out 2>sample_0053_ru.err

# boot_cdr
${M20RU} boot.simh >boot_ru.out 2>boot_ru.err

# primes
${M20RU} primes_0001.simh >primes_0001_ru.out 2>primes_0001_ru.err

# debug_demo
${M20RU} debug_demo.simh >debug_demo_ru.out 2>debug_demo_ru.err

# constants_test
${M20RU} constants_test.simh >constants_test_ru.out 2>constants_test_ru.err
