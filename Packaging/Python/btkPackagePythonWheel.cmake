SET(CMAKE_MODULE_PATH "${BTK_SOURCE_DIR}/CMake")
FIND_PACKAGE(PythonInterp REQUIRED QUIET)
FIND_PACKAGE(SWIG 2.0 REQUIRED)
FIND_PACKAGE(PythonLibs REQUIRED)
FIND_PACKAGE(NumPy REQUIRED)

SET(BTK_PYTHON_WRAPPING_SOURCE_DIR "${BTK_SOURCE_DIR}/Wrapping/Python")
SET(BTK_PYTHON_WHEEL_SOURCE_DIR "${BTK_SOURCE_DIR}/Packaging/Python/wheel")
SET(BTK_PYTHON_WHEEL_BINARY_DIR "${BTK_BINARY_DIR}/Packaging/Python/wheel")

MESSAGE(STATUS "---------------------------")
MESSAGE(STATUS "Packaging Python Wheel ...")
MESSAGE(STATUS "---------------------------")

FILE(MAKE_DIRECTORY "${BTK_PYTHON_WHEEL_BINARY_DIR}")

MESSAGE(STATUS "Preparing sources in ${BTK_PYTHON_WHEEL_BINARY_DIR}")

# btk.py file
FILE(MAKE_DIRECTORY "${BTK_PYTHON_WHEEL_BINARY_DIR}/btk")
FILE(COPY "${BTK_BINARY_DIR}/bin/btk.py" DESTINATION "${BTK_PYTHON_WHEEL_BINARY_DIR}/btk")
FILE(RENAME "${BTK_PYTHON_WHEEL_BINARY_DIR}/btk/btk.py" "${BTK_PYTHON_WHEEL_BINARY_DIR}/btk/__init__.py")

SET(BTKSWIG_SOURCES
    libBTKBasicFilters.a
    libBTKCommon.a
    libBTKIO.a
    btkSwigPYTHON_wrap.cxx)

# .a files and cxx file
FOREACH(dep IN LISTS BTKSWIG_SOURCES)
  MESSAGE(STATUS " - ${dep}")
  FILE(COPY "${BTK_BINARY_DIR}/bin/${dep}" DESTINATION "${BTK_PYTHON_WHEEL_BINARY_DIR}")
ENDFOREACH(dep)


MESSAGE(STATUS "Creating setup.py for build")
CONFIGURE_FILE(${BTK_PYTHON_WHEEL_SOURCE_DIR}/setup.py.in
               ${BTK_PYTHON_WHEEL_BINARY_DIR}/setup.py @ONLY IMMEDIATE)

# Package files
MESSAGE(STATUS "Creating Python wheel using setup.py bdist_wheel in ${BTK_PYTHON_WHEEL_BINARY_DIR}")
EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} setup.py bdist_wheel
                WORKING_DIRECTORY "${BTK_PYTHON_WHEEL_BINARY_DIR}/")