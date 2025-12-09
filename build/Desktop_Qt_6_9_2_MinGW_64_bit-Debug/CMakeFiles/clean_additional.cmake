# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\MiniIDE_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\MiniIDE_autogen.dir\\ParseCache.txt"
  "MiniIDE_autogen"
  )
endif()
