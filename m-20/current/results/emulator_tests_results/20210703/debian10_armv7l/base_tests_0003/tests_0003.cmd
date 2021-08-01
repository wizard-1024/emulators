@REM build IS2
m20ru.exe is2_build.simh >is2_build_ru.out 2>is2_build_ru.err
m20ru.exe is2_build_mt.simh >is2_build_mt_ru.out 2>is2_build_mt_ru.err
@REM transfer drumX images
copy is2_build.drum0 is2_test_0001.drum0
copy is2_build.drum1 is2_test_0001.drum1
copy is2_build.drum2 is2_test_0001.drum2
@REM transfer drumX images
copy is2_build.drum0 is2_test_0007.drum0
copy is2_build.drum1 is2_test_0007.drum1
copy is2_build.drum2 is2_test_0007.drum2
@REM transfer drumX images
copy is2_build.drum0 is2_test_0009.drum0
copy is2_build.drum1 is2_test_0009.drum1
copy is2_build.drum2 is2_test_0009.drum2
@REM IS2+SP
m20ru.exe is2_test_0001.simh >is2_test_0001_ru.out 2>is2_test_0001_ru.err
m20ru.exe is2_test_0007.simh >is2_test_0007_ru.out 2>is2_test_0007_ru.err
m20ru.exe is2_test_0009.simh >is2_test_0009_ru.out 2>is2_test_0009_ru.err

