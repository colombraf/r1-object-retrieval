################################################################################
#                                                                              #
# Copyright (C) 2020 Fondazione Istituto Italiano di Tecnologia (IIT)          #
# All Rights Reserved.                                                         #
#                                                                              #
################################################################################

set(AUX_NAME backupVAD)
add_executable(${AUX_NAME})
target_include_directories(${AUX_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_sources(${AUX_NAME}
  PRIVATE
    Interfaces/IAudioProcessorFeeder.h
    Interfaces/IAudioProcessorMicrophoneCloser.h
    Interfaces/IAudioProcessorMicrophoneOpener.h
    main.cpp
    AudioProcessor.h
    AudioProcessor.cpp
    MicrophoneStatusCallback.h
    MicrophoneStatusCallback.cpp
    AudioProcessorCreator.h
    AudioProcessorCreator.cpp
    AudioCallback.cpp
    AudioCallback.h
)

target_sources(${AUX_NAME}
  PRIVATE
    ${STATECHARTS_FILES})

target_link_libraries(${AUX_NAME}
  PRIVATE
    YARP::YARP_os
    YARP::YARP_dev
    YARP::YARP_init
    PkgConfig::libfvad
    )
install(TARGETS ${AUX_NAME} DESTINATION bin)
