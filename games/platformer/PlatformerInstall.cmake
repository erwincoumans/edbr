set(GAME_INSTALL_DIR "platformer")

install(TARGETS platformer
  RUNTIME DESTINATION ${GAME_INSTALL_DIR}
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  DESTINATION "${GAME_INSTALL_DIR}"
)

set(SHADERS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
if (MSVC)
  set(SHADERS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/shaders")
endif()

install(DIRECTORY "${SHADERS_BUILD_DIR}"
  DESTINATION "${GAME_INSTALL_DIR}"
)

if (WIN32)
  install(TARGETS OpenAL
  RUNTIME DESTINATION "${GAME_INSTALL_DIR}"
  )
endif()
