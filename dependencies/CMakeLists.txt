# Enable fetching content
include(FetchContent)

# Set the root of the Terrapane source repositories
if(DEFINED ENV{TERRAPANE_SOURCE_URI})
    set(TERRAPANE_SOURCE_URI "$ENV{TERRAPANE_SOURCE_URI}")
else()
    set(TERRAPANE_SOURCE_URI "https://github.com/terrapane")
endif()

# If building tests, bring in the Simple Test Framework
if(memory_manager_BUILD_TESTS)
    # Fetch the Simple Test Framework library
    FetchContent_Declare(stf
        GIT_REPOSITORY GIT_REPOSITORY ${TERRAPANE_SOURCE_URI}/stf.git
        GIT_TAG        v1.0.3
        GIT_SHALLOW    true)

    # It is not necessary to install the STF library
    set(stf_INSTALL OFF)

    # Make STF available
    FetchContent_MakeAvailable(stf)
endif()

# Fetch the Logger
FetchContent_Declare(logger
    GIT_REPOSITORY GIT_REPOSITORY ${TERRAPANE_SOURCE_URI}/logger.git
    GIT_TAG        v1.0.7
    GIT_SHALLOW    true)

# Install dependencies only if this library is installed
set(logger_INSTALL ${memory_manager_INSTALL})

# Make dependencies available
FetchContent_MakeAvailable(logger)
