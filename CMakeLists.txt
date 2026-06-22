cmake_minimum_required(VERSION 3.16)
project(KeynetikPOS VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Find Qt
# ============================================================
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core Sql PrintSupport Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Sql PrintSupport Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS SerialPort)

# ============================================================
# App Metadata
# ============================================================
set(APP_NAME "KeynetikPOS")
set(APP_VERSION "2.0.0")
set(APP_PUBLISHER "Keynetik Solutions")
set(APP_DESCRIPTION "Next-GEN Professional Point of Sale System")
set(APP_URL "https://www.keynetik.com")

# ============================================================
# Resources
# ============================================================
set(WINDOWS_RC_FILE "")
if(WIN32)
    set(RC_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    if(EXISTS "${RC_SOURCE}")
        set(WINDOWS_RC_FILE "${RC_SOURCE}")
    endif()
endif()

set(QT_RESOURCE_FILE "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc")
    set(QT_RESOURCE_FILE "resources/resources.qrc")
endif()

# ============================================================
# Sources
# ============================================================
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp mainwindow.h
    database.cpp database.h
    salerepository.cpp salerepository.h
    CartItem.h
    cart.h
    cartservice.cpp cartservice.h
    cartmodel.cpp cartmodel.h
    productgridmodel.cpp productgridmodel.h
    checkoutservice.cpp checkoutservice.h
    ledger.cpp ledger.h
    ledgerdialog.cpp ledgerdialog.h
    salejournal.cpp salejournal.h
    payroll.cpp payroll.h
    payrolldialog.cpp payrolldialog.h
    vat.cpp vat.h
    vatdialog.cpp vatdialog.h
    etims.cpp etims.h
    carttotals.cpp carttotals.h
    providersetup.cpp providersetup.h
    appstyle.cpp appstyle.h
    discountdialog.cpp discountdialog.h
    discount.cpp discount.h
    reportsdialog.cpp reportsdialog.h
    paymentdialog.cpp paymentdialog.h
    paymentmanager.cpp paymentmanager.h
    inventorymanager.cpp inventorymanager.h
    inventorydialog.cpp inventorydialog.h
    analyticsdashboard.cpp analyticsdashboard.h
    receiptprinter.cpp receiptprinter.h
    smtpclient.cpp smtpclient.h
    secretstore.cpp secretstore.h
    saleshistorydialog.cpp saleshistorydialog.h
    colorscheme.cpp colorscheme.h
    user.cpp user.h
    usermanager.cpp usermanager.h
    usermanagementdialog.cpp usermanagementdialog.h
    logindialog.cpp logindialog.h
    changepassworddialog.cpp changepassworddialog.h
    messageprovider.h
    passwordhasher.h
    schedulemanager.cpp schedulemanager.h
    scheduledialog.cpp scheduledialog.h
    scheduleeditordialog.cpp scheduleeditordialog.h
    barcodescannermanager.cpp barcodescannermanager.h
    whatsappmanager.cpp whatsappmanager.h
    Africatalkingprovider.h
    shiftmanager.cpp shiftmanager.h
    shift_dialogs.cpp shift_dialogs.h shift_type.h
    settingsmanager.cpp settingsmanager.h
    settingsdialog.cpp settingsdialog.h
    zreportdialog.cpp zreportdialog.h
    lowstockdialog.cpp lowstockdialog.h
    supplierdialog.cpp supplierdialog.h
    purchaseorderdialog.cpp purchaseorderdialog.h
    refunddialog.cpp refunddialog.h
    stocktakedialog.cpp stocktakedialog.h
    onboardingwizard.cpp onboardingwizard.h
    expensedialog.cpp expensedialog.h
    customerdialog.cpp customerdialog.h
    MessageSchedule.h
    licensemanager.h licensemanager.cpp
    mpesaclient.h mpesaclient.cpp
    antidebug.h
    ${QT_RESOURCE_FILE}
    ${WINDOWS_RC_FILE}
)

# ============================================================
# Build
# ============================================================
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(KeynetikPOS WIN32 MANUAL_FINALIZATION ${PROJECT_SOURCES}
        barcodereader.h barcodereader.cpp)
else()
    add_executable(KeynetikPOS WIN32 ${PROJECT_SOURCES})
endif()

target_include_directories(KeynetikPOS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================
# Warnings — surface bugs the compiler can already see. Not -Werror:
# the existing tree has pre-existing warnings, and a hard failure would
# block builds before they can be cleaned up incrementally.
# ============================================================
if(MSVC)
    # /utf-8: the sources are UTF-8 (no BOM); without this MSVC reads string
    # literals in the system code page and mangles emoji/accents into mojibake.
    # (MinGW/GCC already defaults to a UTF-8 input/exec charset.)
    set(KEYNETIK_WARNINGS /W4 /utf-8)
else()
    set(KEYNETIK_WARNINGS -Wall -Wextra)
endif()
target_compile_options(KeynetikPOS PRIVATE ${KEYNETIK_WARNINGS})

target_link_libraries(KeynetikPOS PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
)
target_link_libraries(KeynetikPOS PRIVATE Qt${QT_VERSION_MAJOR}::SerialPort)

# DPAPI (CryptProtectData) for SecretStore — Windows credential encryption.
if(WIN32)
    target_link_libraries(KeynetikPOS PRIVATE crypt32)
endif()

# ============================================================
# Deployment Logic (windeployqt)
# ============================================================
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/windeployqt_stage")
        add_custom_command(TARGET KeynetikPOS POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:KeynetikPOS>" "${DEPLOY_DIR}/KeynetikPOS.exe"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations --no-system-d3d-compiler
                "${DEPLOY_DIR}/KeynetikPOS.exe"
            VERBATIM
        )
    endif()
endif()

# ============================================================
# Installation
# ============================================================
include(GNUInstallDirs)
if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    install(DIRECTORY "${DEPLOY_DIR}/" DESTINATION "." USE_SOURCE_PERMISSIONS)
else()
    install(TARGETS KeynetikPOS RUNTIME DESTINATION ".")
endif()

# ============================================================
# CPack Setup
# ============================================================
set(LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
if(EXISTS "${LICENSE_FILE}")
    set(CPACK_PACKAGE_NAME "${APP_NAME}")
    set(CPACK_PACKAGE_VENDOR "${APP_PUBLISHER}")
    set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${APP_NAME}")
    set(CPACK_RESOURCE_FILE_LICENSE "${LICENSE_FILE}")
    set(CPACK_PACKAGE_FILE_NAME "${APP_NAME}-${APP_VERSION}-Setup")

    if(WIN32)
        set(CPACK_GENERATOR "NSIS")
        set(CPACK_NSIS_DISPLAY_NAME "${APP_NAME} ${APP_VERSION}")
        set(CPACK_NSIS_PACKAGE_NAME "${APP_NAME}")
        set(CPACK_NSIS_URL_INFO_ABOUT "${APP_URL}")
        set(CPACK_NSIS_CONTACT "support@keynetik.com")

        # ── Icon (only set if the file actually exists) ──────────────
        set(_ICO "${CMAKE_SOURCE_DIR}/resources/app.ico")
        if(EXISTS "${_ICO}")
            # NSIS needs a native Windows path with forward slashes
            file(TO_NATIVE_PATH "${_ICO}" _ICO_NATIVE)
            # CMake stores native paths with backslashes; convert them so
            # the string reaches NSIS correctly (NSIS uses forward slashes
            # in most contexts but accepts both – use forward slashes to
            # stay safe inside CMake string handling).
            string(REPLACE "\\" "/" _ICO_FWD "${_ICO_NATIVE}")
            set(CPACK_NSIS_MUI_ICON    "${_ICO_FWD}")
            set(CPACK_NSIS_MUI_UNIICON "${_ICO_FWD}")
        endif()

        # ── Shortcuts created during install ─────────────────────────
        # CPACK_NSIS_CREATE_ICONS_EXTRA is raw NSIS script injected into
        # the installer Section.  Use NSIS variables ($SMPROGRAMS etc.)
        # with their own backslash syntax – but we must escape each
        # backslash as \\ so CMake doesn't treat them as escape sequences.
        set(CPACK_NSIS_CREATE_ICONS_EXTRA
            "CreateDirectory '$SMPROGRAMS\\\\${APP_NAME}'
             CreateShortCut  '$SMPROGRAMS\\\\${APP_NAME}\\\\${APP_NAME}.lnk' '$INSTDIR\\\\KeynetikPOS.exe'
             CreateShortCut  '$DESKTOP\\\\${APP_NAME}.lnk' '$INSTDIR\\\\KeynetikPOS.exe'")

        # Matching removal script for uninstaller
        set(CPACK_NSIS_DELETE_ICONS_EXTRA
            "Delete '$SMPROGRAMS\\\\${APP_NAME}\\\\${APP_NAME}.lnk'
             Delete '$DESKTOP\\\\${APP_NAME}.lnk'
             RMDir  '$SMPROGRAMS\\\\${APP_NAME}'")

        # Launch app after install finishes
        set(CPACK_NSIS_MUI_FINISHPAGE_RUN "KeynetikPOS.exe")

        # ── CD-Key page injected via EXTRA_INSTALL_COMMANDS ──────────
        # Writing a full custom nsDialogs page through CMake string
        # variables is error-prone because of nested escaping.  Instead
        # we use a simpler approach: prompt for the key with a standard
        # NSIS InputBox via CPACK_NSIS_EXTRA_INSTALL_COMMANDS, which
        # doesn't require a separate Page declaration and avoids the
        # escaping nightmare of nsDialogs inside CMake strings.
        # set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        #     "InputBox 'CD Key' 'Please enter your ${APP_NAME} CD Key:' '' $0
        #      WriteRegStr HKCU 'Software\\\\${APP_NAME}' 'CDKey' '$0'")

    endif() # WIN32

    include(CPack)
endif() # LICENSE_FILE exists

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(KeynetikPOS)
endif()

# ============================================================
# Tests (QtTest) — unit tests for the pure money math.
# Build with -DKEYNETIK_BUILD_TESTS=ON (default), run with ctest.
# The test target links ONLY carttotals.cpp (dependency-free), so it
# never drags in the DB/UI and stays fast to build and run.
# ============================================================
option(KEYNETIK_BUILD_TESTS "Build KeynetikPOS unit tests" ON)
if(KEYNETIK_BUILD_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    enable_testing()

    add_executable(carttotals_test
        tests/carttotals_test.cpp
        carttotals.cpp carttotals.h
    )
    set_target_properties(carttotals_test PROPERTIES AUTOMOC ON)
    target_include_directories(carttotals_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(carttotals_test PRIVATE ${KEYNETIK_WARNINGS})
    # Qt::Sql is linked only so settingsmanager.h's <QSqlDatabase> include
    # resolves when carttotals.cpp pulls in BusinessSettings — the test uses no
    # database itself.
    target_link_libraries(carttotals_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME carttotals_test COMMAND carttotals_test)

    # Characterization tests for the money path (recordSale / processRefund).
    # Links database.cpp against an in-memory SQLite DB — no Widgets, no UI.
    add_executable(database_test
        tests/database_test.cpp
        database.cpp database.h
        salerepository.cpp salerepository.h
    )
    set_target_properties(database_test PROPERTIES AUTOMOC ON)
    target_include_directories(database_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(database_test PRIVATE ${KEYNETIK_WARNINGS})
    target_link_libraries(database_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME database_test COMMAND database_test)

    # General-ledger double-entry invariants (in-memory SQLite, no UI).
    add_executable(ledger_test
        tests/ledger_test.cpp
        ledger.cpp ledger.h
    )
    set_target_properties(ledger_test PROPERTIES AUTOMOC ON)
    target_include_directories(ledger_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(ledger_test PRIVATE ${KEYNETIK_WARNINGS})
    target_link_libraries(ledger_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME ledger_test COMMAND ledger_test)

    # Kenya statutory-deduction engine (pure math; links ledger.cpp for posting).
    add_executable(payroll_test
        tests/payroll_test.cpp
        payroll.cpp payroll.h
        ledger.cpp ledger.h
    )
    set_target_properties(payroll_test PROPERTIES AUTOMOC ON)
    target_include_directories(payroll_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(payroll_test PRIVATE ${KEYNETIK_WARNINGS})
    target_link_libraries(payroll_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME payroll_test COMMAND payroll_test)

    # VAT engine + VAT-3 aggregation (in-memory SQLite; links ledger.cpp).
    add_executable(vat_test
        tests/vat_test.cpp
        vat.cpp vat.h
        ledger.cpp ledger.h
    )
    set_target_properties(vat_test PROPERTIES AUTOMOC ON)
    target_include_directories(vat_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(vat_test PRIVATE ${KEYNETIK_WARNINGS})
    target_link_libraries(vat_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME vat_test COMMAND vat_test)

    # Sale -> GL journal builder (pure; balanced double-entry per sale).
    add_executable(salejournal_test
        tests/salejournal_test.cpp
        salejournal.cpp salejournal.h
    )
    set_target_properties(salejournal_test PROPERTIES AUTOMOC ON)
    target_include_directories(salejournal_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(salejournal_test PRIVATE ${KEYNETIK_WARNINGS})
    # Qt::Sql only so ledger.h's <QSqlDatabase> include (via salejournal.h)
    # resolves — the builder uses no database itself.
    target_link_libraries(salejournal_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME salejournal_test COMMAND salejournal_test)
endif()