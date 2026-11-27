# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

if (DESKTOP_APP_TEST_APPS OR TDESKTOP_BUILD_WEB_PROXY_TESTS)
    add_executable(test_web_proxy_protocol)
    init_target(test_web_proxy_protocol "(tests)")

    nice_target_sources(test_web_proxy_protocol ${src_loc}
    PRIVATE
        mtproto/web_proxy/web_proxy_frame.cpp
        mtproto/web_proxy/web_proxy_frame.h
        mtproto/mtproto_proxy_data.cpp
        mtproto/mtproto_proxy_data.h
        tests/test_named_checks.hpp
        tests/test_web_proxy_protocol.cpp
    )

    target_include_directories(test_web_proxy_protocol
    PRIVATE
        ${src_loc}
    )

    target_link_libraries(test_web_proxy_protocol
    PRIVATE
        desktop-app::lib_base
        desktop-app::external_qt
    )

    set_target_properties(test_web_proxy_protocol PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test_web_proxy_protocol
    )
endif()
