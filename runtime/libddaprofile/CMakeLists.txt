set(SOURCES
  LDDShadowCommonRT.cpp
  LDDHashRT.cpp
  LDDLightShadowRT.cpp
  LDDCompleteShadowRT.cpp
  RedBlackTree.cpp
  TimerProfilingRuntime.cpp
  LDDParallelAnalysis.cpp
  LDDParallelAnalysis.h
  LDDCommonConst.h
  LDDLightShadowRT.h
  LDDDAShadowRT.h
  LDDDAShadowPlatform.h
  )
#add_definitions(-Winline)
#add_definitions(-D_DDA_PA -D_OPT1 -D_OPT2 -D_OPT4 -D_TIME_PROFx -D__OPT3_POOL -D_MUTEXx -D_GAS_SHADOWx -D_GAS_SHADOW_PARLIDAx -D_DDA_DEFAULT -D_DDA_DEFAULT_PP_PARLIDA -D_GENx -D_DDA_PP -D_RT_PROC -D_BIND -D_PP -D_FTP -I/home/lxj/software/rt.rev0.1/) 
#add_definitions(-D_DDA_PA -D_OPT1 -D_OPT2 -D_OPT4 -D_TIME_PROF -D__OPT3_POOL -D_MUTEXx -D_GAS_SHADOWx -D_GAS_SHADOW_PARLIDAx -D_PARLIDA -D_GENx -D_DDA_PP -D_RT_PROC -D_BIND -D_PP -D_FTP -I/home/lxj/software/rt.rev0.1/) 
#add_definitions(-D_DDA_PA -D_DEBUGINFO -D_DEBUGINFO_LCDA  -D_TIME_PROF -Winline )
#add_definitions(-D_OVERHEAD_PROF -D_TIME_PROF -D_DDA_PA -Winline  )
#add_definitions(-std=c++11 -D_OVERHEAD_PROF -D_TIME_PROF -D_DDA_PA -Winline)
#add_definitions(-D_TIME_PROF -D_DDA_PA -D_DEBUGINFO_LCDA)
#add_definitions(-D_OVERHEAD_PROF -D_DDA_PA)
#add_definitions(-D_OVERHEAD_PROF  -D_DDA_PA -D_DEBUGINFO_LIDA)
#add_definitions(-D_OVERHEAD_PROF -D_DDA_PA -D_DEBUGINFO_LIDA)
#add_definitions(-D_PROF_ADDR_SET -D_OVERHEAD_PROF)

add_llvm_library( LDDProfileRuntime-static ${SOURCES} )
set_target_properties( LDDProfileRuntime-static
  PROPERTIES
  OUTPUT_NAME "LDDProfileRuntime" )

set(BUILD_SHARED_LIBS ON)
add_llvm_library( LDDProfileRuntime-shared ${SOURCES} )
set_target_properties( LDDProfileRuntime-shared
  PROPERTIES
  OUTPUT_NAME "LDDProfileRuntime" )
