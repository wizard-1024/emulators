#!/bin/sh

# clean all
#rm -f *.lst *.out *.cdp *.err *.txt
#exit 0

# variables
M20RU="./m20ru"

# hello
${M20RU} hello.simh >hello_ru.out 2>hello_ru.err
${M20RU} hello1.simh >hello1_ru.out 2>hello1_ru.err
${M20RU} hello2.simh >hello2_ru.out 2>hello2_ru.err

# add
${M20RU} add_0001.simh >add_0001_ru.out 2>add_0001_ru.err
${M20RU} add_0002.simh >add_0002_ru.out 2>add_0002_ru.err

# IS2+SP
${M20RU} is2_test_0002.simh >is2_test_0002_ru.out 2>is2_test_0002_ru.err
${M20RU} is2_test_0007.simh >is2_test_0007_ru.out 2>is2_test_0007_ru.err
${M20RU} is2_test_0009.simh >is2_test_0009_ru.out 2>is2_test_0009_ru.err

# samples
${M20RU} sample_0032.simh >sample_0032_ru.out 2>sample_0032_ru.err
${M20RU} sample_0033.simh >sample_0033_ru.out 2>sample_0033_ru.err
${M20RU} sample_0034.simh >sample_0034_ru.out 2>sample_0034_ru.err
${M20RU} sample_0053.simh >sample_0053_ru.out 2>sample_0053_ru.err

