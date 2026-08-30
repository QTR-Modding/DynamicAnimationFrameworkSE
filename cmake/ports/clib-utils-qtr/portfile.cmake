# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF ddf9d104c9ac30e23735e8934a707e7dadcc2a05
    SHA512 8fa022970d8bfb9c5cb114f5d25dd3f1bc4839c8743dbc7ecc676cdd22bcff4a2720a093eceab2e8e6bcddf50ef12b38aa461ea74ddd36b53e303da680e7d783
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
