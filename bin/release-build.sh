#!/bin/sh

# Build configuration
CONFIGURATION=""

# Platforms that should be combined into a single iPhone output directory
# The iPhoneOS/iPhoneSimulator platforms are unique (and, arguably, broken) in this regard
IPHONE_PLATFORMS="iPhoneOS iPhoneSimulator"

# Platforms to build
PLATFORMS="MacOSX ${IPHONE_PLATFORMS}"

# The primary platform, from which headers will be copied
PRIMARY_PLATFORM="MacOSX"

# Base product name
PRODUCT="CrashReporter"

VERSION="`date +%Y%m%d`-snap"

# List of all iPhone static libs. Populated by copy_build()
IPHONE_PRODUCT_LIBS=""

print_usage () {
    echo "`basename $0` <options> [-c configuration] [-v version]"
    echo "Options:"
    echo "-c:    Specify the build configuration (Release or Debug)"
    echo "-v:    Specify the release version. If none is supplied, ${VERSION} will be used"
    echo "-p:    Specify a platform to build (Available: ${PLATFORMS})."
    echo "       Multiple -p options may be supplied"
    echo "\nExample:"
    echo "    $0 -p MacOSX -c Release"
    echo "    Build a Mac OS X-only release"
    echo ""
    echo "    $0 -c Release -v 1.3"
    echo "    Build a standard release ($PLATFORMS)"
}

check_supported_platform () {
    local REQ_PLATFORM=$1
    for platform in ${PLATFORMS}; do
        if [ "${platform}" = "${REQ_PLATFORM}" ]; then
            REQ_PLATFORM_VALID=YES
            return
        fi
    done
    REQ_PLATFORM_VALID=NO
}

# Read in the command line arguments
while getopts hc:v:p: OPTION; do
    case ${OPTION} in
        p)
            check_supported_platform $OPTARG
            if [ "${REQ_PLATFORM_VALID}" != "YES" ]; then
                echo "Platform ${OPTARG} is not supported"
                exit 1
            fi
            CUSTOM_PLATFORM="${CUSTOM_PLATFORM} $OPTARG"
            ;;
        c)
            CONFIGURATION=${OPTARG}
            ;;
        v)
            VERSION=${OPTARG}
            ;;
        h)
            print_usage
            exit 1;;
        *)
            print_usage
            exit 1;;
    esac
done
shift $(($OPTIND - 1))

if [ ! -z "${CUSTOM_PLATFORM}" ]; then
    PLATFORMS="${CUSTOM_PLATFORM}"
fi

if [ -z "${CONFIGURATION}" ]; then
    print_usage
    exit 1
fi

# What are we building
echo "Building ${PRODUCT}"
echo "Platforms: ${PLATFORMS}"
echo "Config: ${CONFIGURATION}"
sleep 1

# Check for program failure
check_failure () {
    if [ $? != 0 ]; then
        echo "ERROR: $1"
        exit 1
    fi
}

# Build all platforms targets, and execute their unit tests.
for platform in ${PLATFORMS}; do
    xcodebuild -configuration $CONFIGURATION -target ${PRODUCT}-${platform}
    check_failure "Build for ${PRODUCT}-${platform} failed"

    # Check for unit tests, run them if available
    xcodebuild -list | grep -q Tests-${platform}
    if [ $? = 0 ]; then
        xcodebuild -configuration ${CONFIGURATION} -target Tests-${platform}
        check_failure "Unit tests for ${PRODUCT}-${platform} failed"
    fi
done

# Build the output directory
copy_build () {
    # Arguments
    local PLATFORM=$1
    local ROOT_OUTPUT_DIR=$2

    # Determine if this platform is an iPhone OS platform
    for phone_platform in ${IPHONE_PLATFORMS}; do
        if [ "${PLATFORM}" = "${phone_platform}" ]; then
            local IPHONE_PLATFORM=YES
        fi
    done

    # Input files/directories
    local PLATFORM_BUILD_DIR="build/${CONFIGURATION}-${platform}"
    local PLATFORM_FRAMEWORK="${PLATFORM_BUILD_DIR}/${PRODUCT}.framework"
    local CANONICAL_FRAMEWORK="build/${CONFIGURATION}-${PRIMARY_PLATFORM}/${PRODUCT}.framework"
    local PLATFORM_STATIC_LIB="${PLATFORM_BUILD_DIR}/lib${PRODUCT}.a"
    local PLATFORM_SDK_NAME=`echo ${PLATFORM} | tr '[A-Z]' '[a-z]'`
    local PLATFORM_SPECIFIC_STATIC_LIB="${PLATFORM_BUILD_DIR}/lib${PRODUCT}-${PLATFORM_SDK_NAME}.a"

    # Output files/directories
    local PLATFORM_OUTPUT_DIR="${ROOT_OUTPUT_DIR}/${PRODUCT}-${PLATFORM}"

    # For the iPhone-combined simulator/device platforms, a platform-specific static library name is used
    if [ "${IPHONE_PLATFORM}" = "YES" ]; then
        local PLATFORM_STATIC_LIB="${PLATFORM_SPECIFIC_STATIC_LIB}"
        IPHONE_STATIC_LIBS="${IPHONE_STATIC_LIBS} ${PLATFORM_STATIC_LIB}"
    fi


    # Check if the platform was built
    if [ ! -d "${PLATFORM_BUILD_DIR}" ]; then
        echo "Missing build results for ${PLATFORM_BUILD_DIR}"
        exit 1
    fi

    if [ ! -d "${PLATFORM_FRAMEWORK}" ]; then
        echo "Missing framework build for ${PLATFORM_BUILD_DIR}"
        exit 1
    fi

    # Create the output directory if it does not exist
    mkdir -p "${PLATFORM_OUTPUT_DIR}"
    check_failure "Could not create directory: ${PLATFORM_OUTPUT_DIR}"

    # Copy in built framework
    echo "${PLATFORM}: Copying ${PLATFORM_FRAMEWORK}"
    tar -C `dirname "${PLATFORM_FRAMEWORK}"` -cf - "${PRODUCT}.framework" | tar -C "${PLATFORM_OUTPUT_DIR}" -xf -
    check_failure "Could not copy framework ${PLATFORM_FRAMEWORK}"

    # Update the framework version
    /usr/libexec/PlistBuddy -c "Set :CFBundleVersion ${VERSION}" "${PLATFORM_OUTPUT_DIR}/${PRODUCT}.framework/Versions/Current/Resources/Info.plist"

    # Copy in static lib, if it exists
    if [ -f "${PLATFORM_STATIC_LIB}" ]; then
        cp "${PLATFORM_STATIC_LIB}" "${PLATFORM_OUTPUT_DIR}"
    fi 
}

# Copy the platform build results
OUTPUT_DIR="${PRODUCT}-${VERSION}"
if [ -d "${OUTPUT_DIR}" ]; then
    echo "Output directory ${OUTPUT_DIR} already exists"
    exit 1
fi
mkdir -p "${OUTPUT_DIR}"

# Standard builds
for platform in ${PLATFORMS}; do
    echo "Copying ${platform} build to ${OUTPUT_DIR}"
    copy_build ${platform} "${OUTPUT_DIR}"
done

# Build a single iPhoneOS/iPhoneSimulator static framework
for platform in ${IPHONE_PLATFORMS}; do
    if [ -d "${OUTPUT_DIR}/${PRODUCT}-${platform}" ]; then
        mkdir -p  "${OUTPUT_DIR}/${PRODUCT}-iPhone/"

        tar -C "${OUTPUT_DIR}/${PRODUCT}-${platform}" -cf - "${PRODUCT}.framework" | tar -C "${OUTPUT_DIR}/${PRODUCT}-iPhone/" -xf -
        check_failure "Could not copy framework ${platform} framework"

        rm -r "${OUTPUT_DIR}/${PRODUCT}-${platform}"
        check_failure "Could not delete framework ${platform} framework"
    fi
done

if [ ! -z "${IPHONE_STATIC_LIBS}" ]; then
    lipo $IPHONE_STATIC_LIBS -create -output "${OUTPUT_DIR}/${PRODUCT}-iPhone/${PRODUCT}.framework/Versions/Current/${PRODUCT}"
    check_failure "Could not lipo iPhone framework"
fi

# Build the documentation
doxygen
check_failure "Documentation generation failed"

mv Documentation "${OUTPUT_DIR}"
check_failure "Documentation generation failed"

# Copy in the README file (TODO)
#

# Build the DMG
hdiutil create -srcfolder "${OUTPUT_DIR}" "${OUTPUT_DIR}.dmg"
check_failure "DMG generation failed"
