cmake_minimum_required(VERSION 3.19)

if(NOT ZENO_WITH_zenvdb OR NOT ZENO_WITH_ZenoFX)
  message(FATAL_ERROR "zenvdb and ZenoFX must be ON when CUDA is ON! "
    "Please specify: -DZENO_WITH_zenvdb:BOOL=ON -DZENO_WITH_ZenoFX:BOOL=ON")
endif()

if(ZENO_SYSTEM_OPENVDB)
  message(FATAL_ERROR "ZENO_SYSTEM_OPENVDB must be OFF when CUDA is ON! "
    "Please specify: -DZENO_SYSTEM_OPENVDB:BOOL=ON")
endif()

# assume cuda
project(ZENO_CUDA CXX CUDA)
set(ZS_ENABLE_CUDA ON)
set(ZS_ENABLE_OPENVDB ON)
set(ZENSIM_INSTALL OFF)
set(ZS_BUILD_SHARED_LIBS ON)
set(ZS_ENABLE_ZENO_CU_WRANGLE ON)
set(CMAKE_CUDA_RESOLVE_DEVICE_SYMBOLS ON)
set(PYZPC_GIT_PROXY "none" CACHE STRING "Git proxy used to clone PyZPC repository.")

find_package(CUDAToolkit REQUIRED COMPONENTS cufft REQUIRED)
find_package(Eigen3 REQUIRED)

set(WHEREAMI_BUILD_SHARED_LIBS ON)
add_subdirectory(zpc)

if (ZS_PYTHON_FOUND)
  target_link_libraries(zeno PRIVATE zpc_jit_py)
endif(ZS_PYTHON_FOUND)

if (ZS_PYTHON_FOUND AND ZENO_WITH_PyZpc)

  if (DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(RESOURCE_BASE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  elseif (DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(RESOURCE_BASE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  else()
    set(RESOURCE_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  get_target_property(ZPC_BIN_DIR zeno RUNTIME_OUTPUT_DIRECTORY)
  add_custom_target(copy_py)
  add_dependencies(zeno copy_py)
  add_dependencies(copy_py zpc_jit_py)
  # message(STATUS "retrieved python impl: ${Python3_INTERPRETER_ID}")
  message(STATUS "python3 libraries: ${ZS_OVERWRITE_PYTHON_LIBRARIES}")
  message(STATUS "python3 include directories: ${ZS_OVERWRITE_PYTHON_INCLUDE_DIR}")
  set(ENV{ZPC_BIN_DIR} "${ZPC_BIN_DIR}")
  add_custom_command(
    TARGET copy_py
    POST_BUILD
    COMMAND 
    ${CMAKE_COMMAND} -E env ZPC_BIN_DIR="${ZPC_BIN_DIR}"
    ${ZS_OVERWRITE_PYTHON_EXECUTABLE} -m ensurepip
    COMMENT "preparing pip module"
  )
  add_custom_command(
    TARGET copy_py
    POST_BUILD
    COMMAND 
    ${ZS_OVERWRITE_PYTHON_EXECUTABLE} -m pip install numpy --user
    COMMENT "installing numpy"
  )
  set(PY_LIBS_DIR ${RESOURCE_BASE_DIR}/resource/py_libs)
  set(PREFIX "lib")
  if (WIN32)
    set(SUFFIX ".dll")
    set(PREFIX "")
  elseif (APPLE)
    set(SUFFIX ".dylib")
  elseif (UNIX)
    set(SUFFIX ".so")
  else()
    message(FATAL_ERROR "unknown system to determine shared library suffix")
  endif()

  target_compile_definitions(zeno PRIVATE -DZENO_OUTPUT_BINARY_CAPIS="${PREFIX}zpc_py_interop${SUFFIX}")
  target_compile_definitions(zeno PRIVATE -DZENO_OUTPUT_BINARY_NVRTC="${PREFIX}zpc_jit_nvrtc${SUFFIX}")
  target_compile_definitions(zeno PRIVATE -DZENO_OUTPUT_BINARY_CLANG="${PREFIX}zpc_jit_clang${SUFFIX}")
  add_custom_command(
    TARGET copy_py
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PY_LIBS_DIR}/zpcjit
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PY_LIBS_DIR}/zpcjit/zpc_jit/lib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PY_LIBS_DIR}/zpcjit/zpc_jit/zpc/include/zensim
    COMMENT "creating pyzpc jit module directory at ${PY_LIBS_DIR}/zpcjit"
  )
  file(GLOB ZPC_JIT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/zpc_jit/pyzpc/*.py)
  message("ZPC_JIT_SRC : ${ZPC_JIT_SRC}")

  foreach(ZPCJIT_FILE IN LISTS ZPC_JIT_SRC)
    add_custom_command(
      TARGET copy_py POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZPCJIT_FILE} ${PY_LIBS_DIR}/zpcjit
      COMMENT "Copying zpcjit src file: ${ZPCJIT_FILE}")
  endforeach()
  if (WIN32)
    cmake_path(GET ZS_OVERWRITE_PYTHON_EXECUTABLE PARENT_PATH PYTHON_ENV_PATH)
    message(STATUS "python3 parent: ${PYTHON_ENV_PATH}")
    if (NOT EXISTS ${RESOURCE_BASE_DIR}/Lib/site-packages)
      add_custom_command(
        TARGET copy_py
        POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/lib ${RESOURCE_BASE_DIR}/lib
        # COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/dlls ${RESOURCE_BASE_DIR}/dlls
        COMMAND ${CMAKE_COMMAND}
        -DOBJECTS=${PYTHON_ENV_PATH}/Lib
        -DOUTPUT=${RESOURCE_BASE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_dir.cmake

        COMMAND ${CMAKE_COMMAND}
        -DOBJECTS=${PYTHON_ENV_PATH}/DLLs
        -DOUTPUT=${RESOURCE_BASE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_dir.cmake

      # COMMAND ${CMAKE_COMMAND}
      # -DOBJECTS=${PYTHON_ENV_PATH}/library
      # -DOUTPUT=${RESOURCE_BASE_DIR}
      # -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_dir.cmake

      # COMMAND ${CMAKE_COMMAND}
      # -DLIB="${PYTHON_ENV_PATH}/zlib.dll"
      # -DDLL_DIR=${PYTHON_ENV_PATH}
      # -DOUTPUT=$<TARGET_FILE_DIR:zpc_py_interop>
      # -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_dll.cmake

        COMMENT "copying python lib directories at ${PYTHON_ENV_PATH} to ${RESOURCE_BASE_DIR}"
      )
    endif()
    if (EXISTS ${PYTHON_ENV_PATH}/library)
      add_custom_command(
        TARGET copy_py
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PYTHON_ENV_PATH}/library" ${RESOURCE_BASE_DIR}/library
        COMMENT "copying python library directory at ${PYTHON_ENV_PATH} to ${RESOURCE_BASE_DIR}"
      )
    endif()
    if (EXISTS ${PYTHON_ENV_PATH}/zlib.dll)
      add_custom_command(
        TARGET copy_py
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_ENV_PATH}/zlib.dll" ${RESOURCE_BASE_DIR}
        COMMENT "copying python zlib.dll ${PYTHON_ENV_PATH} to ${RESOURCE_BASE_DIR}"
      )
    endif()
    foreach(lib ${ZS_PYTHON_LIBS})
      add_custom_command(
        TARGET copy_py
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy lib $<TARGET_FILE_DIR:zpc_py_interop>
      )
    endforeach()
  else()
    cmake_path(GET ZS_OVERWRITE_PYTHON_EXECUTABLE PARENT_PATH PYTHON_ENV_PATH)
    cmake_path(GET PYTHON_ENV_PATH PARENT_PATH PYTHON_ENV_PATH)
    message(STATUS "python3 parent: ${PYTHON_ENV_PATH}")
    if (NOT EXISTS ${RESOURCE_BASE_DIR}/lib/libtinfo.a)
      add_custom_command(
        TARGET copy_py
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/bin ${RESOURCE_BASE_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/lib ${RESOURCE_BASE_DIR}/lib

        COMMENT "copying python lib directory ${PYTHON_ENV_PATH}/lib to ${RESOURCE_BASE_DIR}/lib"
      )
    endif()
  endif()

  # prepare zpc headers for pyzpc
  add_custom_command(
    TARGET copy_py POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/zpc/include/zensim ${PY_LIBS_DIR}/zpcjit/zpc_jit/zpc/include/zensim
    COMMENT "Copying zpc source files: ${CMAKE_CURRENT_SOURCE_DIR}/zpc/include/zensim")
  # prepare zpc libs for pyzpc
  
endif(ZS_PYTHON_FOUND AND ZENO_WITH_PyZpc)

add_library(zshelper INTERFACE)
target_include_directories(zshelper INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(zshelper INTERFACE zensim ZFX)
target_compile_features(zshelper INTERFACE cuda_std_17)

target_link_options(zeno
  PRIVATE $<DEVICE_LINK:-dlto>
)
target_link_libraries(zeno PRIVATE Eigen3::Eigen)
target_link_libraries(zeno PRIVATE zshelper)

target_sources(zeno PRIVATE
  iw_query/Query.cpp
)

target_sources(zeno PRIVATE
  #test1.cu
  test.cu
  test.cpp
  # vktest.cpp
)
# target_link_libraries(zeno PRIVATE vulkan shaderc_shared SPIRV)
# target_link_libraries(zeno PRIVATE Vulkan::Vulkan ${Vulkan_LIBRARY})
#[=[
#]=]
set_target_properties(zeno
  PROPERTIES
  CUDA_SEPARABLE_COMPILATION ON
  CUDA_RESOLVE_DEVICE_SYMBOLS ON
)

#
target_sources(zeno PRIVATE
  pyzfx/pyzfx.cpp
)

# wrangler
target_sources(zeno PRIVATE
  wrangle/PW.cu
  wrangle/P2W.cu
  wrangle/PNW.cu
  wrangle/PNBVHW.cu
  wrangle/PPW.cu
  wrangle/TVW.cu
  wrangle/VW.cu
)

# utils
target_sources(zeno PRIVATE
  Structures.cu
  utils/Conversion.cpp
  utils/Primitives.cpp
  utils/IndexBuckets.cu
  utils/Primitives.cu
  utils/TopoUtils.cu
  utils/Tracing.cu
  utils/Noise.cu
  utils/Groom.cpp

  graph/BasicTask.cpp
)

if (ZS_PYTHON_FOUND)
target_sources(zeno PRIVATE
  graph/Task.cpp
)
endif(ZS_PYTHON_FOUND)

add_subdirectory(remesh)
add_subdirectory(cluster)