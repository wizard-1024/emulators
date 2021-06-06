#!/bin/sh

# clean all
#rm -f *.lst *.out *.cdp *.err *.txt
#exit 0

# variables
M20RU="./m20ru"
AUTOCODE_M20="./autocode_m20"

# compile symbolic codding sources
${AUTOCODE_M20} -vp -e 1 -i hello14.a20 -o hello14.m20 -l hello14.l20 >hello14.p20
${AUTOCODE_M20} -vp -e 2 -i hello12.a20 -o hello12.m20 -l hello12.l20 >hello12.p20
${AUTOCODE_M20} -vp -e 3 -i hello13.a20 -o hello13.m20 -l hello13.l20 >hello13.p20
${AUTOCODE_M20} -vp -e 4 -i hello16.a20 -o hello16.m20 -l hello16.l20 >hello16.p20
${AUTOCODE_M20} -vp -e 5 -i hello15.a20 -o hello15.m20 -l hello15.l20 >hello15.p20
# test emulator files
${M20RU} hello12.simh >hello12_ru.out
${M20RU} hello13.simh >hello13_ru.out
${M20RU} hello14.simh >hello14_ru.out
${M20RU} hello15.simh >hello15_ru.out
${M20RU} hello16.simh >hello16_ru.out

