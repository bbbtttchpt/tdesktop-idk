# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

add_executable(Telegram WIN32 MACOSX_BUNDLE)
init_non_host_target(Telegram)

add_subdirectory(lib_rpl)
add_subdirectory(lib_crl)
add_subdirectory(lib_base)
add_subdirectory(lib_ui)
add_subdirectory(lib_tl)
add_subdirectory(lib_spellcheck)
add_subdirectory(lib_storage)
add_subdirectory(lib_lottie)
add_subdirectory(lib_qr)
add_subdirectory(lib_translate)
add_subdirectory(lib_webrtc)
add_subdirectory(lib_webview)
add_subdirectory(codegen)

get_filename_component(src_loc SourceFiles REALPATH)
get_filename_component(res_loc Resources REALPATH)

include(cmake/telegram_options.cmake)
if (APPLE
    OR WIN32
    OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(cmake/init_standalone_target.cmake)
endif()
include(cmake/lib_ffmpeg.cmake)
include(cmake/lib_stripe.cmake)
include(cmake/lib_tgcalls.cmake)
include(cmake/lib_prisma.cmake)
include(cmake/lib_fido2.cmake)
include(cmake/lib_zxcvbn.cmake)
include(cmake/lib_widget_renderer.cmake)
include(cmake/lib_widget_sandbox.cmake)
include(cmake/td_export.cmake)
include(cmake/td_gram.cmake)
include(cmake/td_iv.cmake)
include(cmake/td_lang.cmake)
include(cmake/td_mtproto.cmake)
include(cmake/td_scheme.cmake)
include(cmake/td_tde2e.cmake)
include(cmake/td_ui.cmake)
include(cmake/td_webauthn.cmake)
include(cmake/telegram_apple_swift_runtime.cmake)
include(cmake/generate_appstream_changelog.cmake)

if (DESKTOP_APP_TEST_APPS)
    include(cmake/tests.cmake)
    include(cmake/test_web_proxy_protocol.cmake)
endif()

if (WIN32)
    include(cmake/generate_midl.cmake)
    generate_midl(Telegram ${src_loc}
        platform/win/windows_quiethours.idl
        platform/win/windows_toastactivator.idl
    )
endif()

include(cmake/generate_update_keys.cmake)
generate_update_keys(Telegram ${res_loc}/update)

if (DESKTOP_APP_TEST_APPS OR TDESKTOP_BUILD_UPDATE_TESTS)
    include(cmake/test_update_verify.cmake)
    if (NOT DESKTOP_APP_DISABLE_AUTOUPDATE)
        include(cmake/test_update_unpack.cmake)
        if (WIN32 AND NOT build_winstore)
            include(cmake/test_updater_win.cmake)
        endif()
    endif()
endif()
include(cmake/test_core_settings.cmake)
include(cmake/test_export_formats.cmake)
include(cmake/test_iv_editor_state.cmake)
include(cmake/test_iv_editor_table_grid.cmake)
include(cmake/test_text_contracts.cmake)
include(cmake/test_wallet_card_tint.cmake)
include(cmake/test_wallet_phrase_shares.cmake)

set_target_properties(Telegram PROPERTIES AUTOMOC ON)

target_link_libraries(Telegram
PRIVATE
    tdesktop::lib_fido2
    tdesktop::lib_widget_renderer_platform
    tdesktop::lib_widget_sandbox_host
    tdesktop::lib_tgcalls
    tdesktop::td_export
    tdesktop::td_gram
    tdesktop::td_iv
    tdesktop::td_lang
    tdesktop::td_mtproto
    tdesktop::td_scheme
    tdesktop::td_tde2e
    tdesktop::td_ui