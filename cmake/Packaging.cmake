set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RamBam - Stress-test your website/API")
set(CPACK_PACKAGE_VENDOR "Melroy van den Berg")
set(CPACK_PACKAGE_CONTACT "Melroy van den Berg <melroy@melroy.org>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://gitlab.melroy.org/melroy/rambam")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/desc.txt")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_TARGET}-${CPACK_PACKAGE_VERSION}")
set(CPACK_DEBIAN_PACKAGE_SECTION "web")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-v${CPACK_PACKAGE_VERSION}") # Without '-Linux', '-Windows' or -Darwin suffix

# include CPack model once all variables are set
include(CPack)
