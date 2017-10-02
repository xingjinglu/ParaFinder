set(SOURCES
  LDDHash.cpp
  LDDLightShadow.cpp
  LDDCompleteShadow.cpp
  TimerProfiling.cpp
  )

#add_definitions(-D_DDA_OUTERMOST_PROFILING -D_TIMER_OUTERMOST_PROFILING -D_DDA_PA -D_DDA_PP -D_DDA_TWO_VERSION)
#add_definitions(-D_DDA_OUTERMOST_PROFILING -D_TIMER_OUTERMOST_PROFILING -D_DDA_PA)
#add_definitions(-D_DDA_OUTERMOST_PROFILING -D_TIMER_OUTERMOST_PROFILING -D_DDA_DEBUG)
add_definitions(-D_DDA_OUTERMOST_PROFILING -D_TIMER_OUTERMOST_PROFILING)

add_llvm_library( LDDProfileInstrument-static ${SOURCES} )
set_target_properties( LDDProfileInstrument-static
  PROPERTIES
  OUTPUT_NAME "LDDProfileInstrument" )

set(BUILD_SHARED_LIBS ON)
add_llvm_library( LDDProfileInstrument-shared ${SOURCES} )
set_target_properties( LDDProfileInstrument-shared
  PROPERTIES
  OUTPUT_NAME "LDDProfileInstrument")


