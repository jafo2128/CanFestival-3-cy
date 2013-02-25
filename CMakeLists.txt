PROJECT(CanFestival)
cmake_minimum_required(VERSION 2.4.0)


SET(EXECUTABLE_OUTPUT_PATH ${${PROJECT_NAME}_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${${PROJECT_NAME}_SOURCE_DIR}/lib)

#ADD_DEFINITIONS(-DNOT_USE_DYNAMIC_LOADING)

if(UNIX)
INCLUDE_DIRECTORIES(${${PROJECT_NAME}_SOURCE_DIR}/include/unix)
INCLUDE_DIRECTORIES(${${PROJECT_NAME}_SOURCE_DIR}/include/timers_unix)
elseif(WIN32)
INCLUDE_DIRECTORIES(${${PROJECT_NAME}_SOURCE_DIR}/include/win32)
endif(UNIX)

INCLUDE_DIRECTORIES(${${PROJECT_NAME}_SOURCE_DIR}/include)

LINK_DIRECTORIES(${${PROJECT_NAME}_SOURCE_DIR}/lib/)
LINK_DIRECTORIES(${${PROJECT_NAME}_SOURCE_DIR}/drivers/can_ixxat_win32/)


set(CF_SRCS
	${${PROJECT_NAME}_SOURCE_DIR}/src/dcf.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/emcy.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/lifegrd.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/lss.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/nmtMaster.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/nmtSlave.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/objacces.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/pdo.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/sdo.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/states.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/sync.c
	${${PROJECT_NAME}_SOURCE_DIR}/src/timer.c
)

if(UNIX)
set(TIMER_SRCS
	${${PROJECT_NAME}_SOURCE_DIR}/drivers/timers_unix/timers_unix.c
	${${PROJECT_NAME}_SOURCE_DIR}/drivers/unix/unix.c
)
elseif(WIN32)
set(TIMER_SRCS
    ${${PROJECT_NAME}_SOURCE_DIR}/drivers/timers_win32/timers_win32.c
    ${${PROJECT_NAME}_SOURCE_DIR}/drivers/win32/win32.c
    ${${PROJECT_NAME}_SOURCE_DIR}/src/win32/CanFestival-3.def
)
endif(UNIX)
  


set(CANOpenShell_SRCS
	${${PROJECT_NAME}_SOURCE_DIR}/examples/CANOpenShell/CANOpenShell.c
	${${PROJECT_NAME}_SOURCE_DIR}/examples/CANOpenShell/CANOpenShellMasterOD.c
	${${PROJECT_NAME}_SOURCE_DIR}/examples/CANOpenShell/CANOpenShellSlaveOD.c
)

if(UNIX)
set(CF_LIBS
	pthread
	rt
	dl
)
elseif(WIN32)
set(CF_LIBS
)
endif(UNIX)

ADD_LIBRARY(CanFestival-3 SHARED ${CF_SRCS} ${TIMER_SRCS})
TARGET_LINK_LIBRARIES(CanFestival-3 ${CF_LIBS})

if(UNIX)
ADD_LIBRARY(can_kvaser SHARED 
    ${${PROJECT_NAME}_SOURCE_DIR}/drivers/can_kvaser_linux/can_kvaser_linux.c)
TARGET_LINK_LIBRARIES(can_kvaser canlib)

ADD_LIBRARY(can_ixxat SHARED 
    ${${PROJECT_NAME}_SOURCE_DIR}/drivers/can_ixxat_linux/can_ixxat_linux.c)
TARGET_LINK_LIBRARIES(can_ixxat eci109DriverLinux)

ADD_LIBRARY(can_copley SHARED 
    ${${PROJECT_NAME}_SOURCE_DIR}/drivers/can_copley_linux/can_copley_linux.c
    ${${PROJECT_NAME}_SOURCE_DIR}/drivers/can_copley_linux/CopleyCAN.c)

elseif(WIN32)
ADD_LIBRARY(can_ixxat SHARED 
    ${${PROJECT_NAME}_SOURCE_DIR}/drivers/can_ixxat_win32/ixxat.cpp 
    ${${PROJECT_NAME}_SOURCE_DIR}/drivers/can_ixxat_win32/ixxat.def)
TARGET_LINK_LIBRARIES(can_ixxat Vci11un6)
endif(UNIX)

ADD_EXECUTABLE(CANOpenShell ${CANOpenShell_SRCS})
TARGET_LINK_LIBRARIES(CANOpenShell CanFestival-3)



