#!/bin/sh

# clean all
#rm -f *.lst *.out *.cdp *.err *.txt
#exit 0

# variables
M20RU="./m20ru"
CP=cp

# build IS2
${M20RU} is2_build.simh >is2_build_ru.out 2>is2_build_ru.err
${M20RU} is2_build_mt.simh >is2_build_mt_ru.out 2>is2_build_mt_ru.err

# Transfer drumX images
${CP} -pf  is2_build.drum0 is2_test_0001.drum0
${CP} -pf  is2_build.drum1 is2_test_0001.drum1
${CP} -pf  is2_build.drum2 is2_test_0001.drum2

# Transfer drumX images
${CP} -pf  is2_build.drum0 is2_test_0007.drum0
${CP} -pf  is2_build.drum1 is2_test_0007.drum1
${CP} -pf  is2_build.drum2 is2_test_0007.drum2

# Transfer drumX images
${CP} -pf  is2_build.drum0 is2_test_0009.drum0
${CP} -pf  is2_build.drum1 is2_test_0009.drum1
${CP} -pf  is2_build.drum2 is2_test_0009.drum2

# IS2+SP
${M20RU} is2_test_0001.simh >is2_test_0001_ru.out 2>is2_test_0001_ru.err
${M20RU} is2_test_0007.simh >is2_test_0007_ru.out 2>is2_test_0007_ru.err
${M20RU} is2_test_0009.simh >is2_test_0009_ru.out 2>is2_test_0009_ru.err

