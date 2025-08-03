# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF 855aa452276438cf66a1bec9aa81d7c6c29b814f
    SHA512 b2c4097a1e725da2b83ba64b8184c94a2d160fb6054d4e8470addb73c8366398801141939809ea099cb23e04bcf0a23ff352371bce74a15e3a9aae9af7c9b79c
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")