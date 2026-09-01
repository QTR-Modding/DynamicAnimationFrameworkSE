# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF c9b7e5d8f447cc9ad1cd312feac5be9d1ce3dd66
    SHA512 e07a0d25b9a06d84e7d48a1258c1e1a28d1c13e8bd92140d0b9c37068128785d7694890f4b664ab075331e180186172ef7f092b35ace5d3a9e2760e3313dc196
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
