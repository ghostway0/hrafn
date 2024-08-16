find_path(ASIO_INCLUDE_DIR
  NAMES asio.hpp
  PATH_SUFFIXES asio
  PATHS
    /usr/local/include
    /usr/include
    /opt/local/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/asio/include
    /opt/
)

if(ASIO_INCLUDE_DIR)
  set(ASIO_FOUND TRUE)
  message(STATUS "Found ASIO: ${ASIO_INCLUDE_DIR}")
else()
  set(ASIO_FOUND FALSE)
  message(STATUS "Could not find ASIO")
endif()

if(ASIO_FOUND AND ASIO_NO_DEPRECATED)
  add_definitions(-DASIO_NO_DEPRECATED)
endif()

mark_as_advanced(ASIO_INCLUDE_DIR)

if(ASIO_FOUND)
  add_library(asio::asio INTERFACE IMPORTED)

  set_target_properties(asio::asio PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ASIO_INCLUDE_DIR}"
  )

  if(ASIO_NO_DEPRECATED)
    target_compile_definitions(asio::asio INTERFACE ASIO_NO_DEPRECATED)
  endif()

  message(STATUS "Configured target asio::asio")
endif()
