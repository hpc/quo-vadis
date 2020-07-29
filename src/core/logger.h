/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file logger.h
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/syslog_sink.h"

class Logger {
    // Convenience internal logger type alias.
    using ilogger_t = decltype(spdlog::get(""));
    // Log sinks.
    // console (stdout)
    ilogger_t m_console_info;
    ilogger_t m_console_error;
    ilogger_t m_console_warn;
    ilogger_t m_console_debug;
    // syslog
    ilogger_t m_syslog_info;
    ilogger_t m_syslog_error;
    ilogger_t m_syslog_warn;
    ilogger_t m_syslog_debug;

    Logger(void) {
        // Formatting applied globally to all registered loggers
        spdlog::set_pattern("[" PACKAGE_NAME " %l at (%s::%!::%#)] %v");
        //
        // console
        //
        m_console_info = spdlog::stdout_logger_mt("console_info");
        m_console_info->set_level(spdlog::level::info);
        m_console_info->set_pattern("%v");

        m_console_error = spdlog::stdout_logger_mt("console_error");
        m_console_error->set_level(spdlog::level::err);

        m_console_warn = spdlog::stdout_logger_mt("console_warn");
        m_console_warn->set_level(spdlog::level::warn);

        m_console_debug = spdlog::stdout_logger_mt("console_debug");
        m_console_debug->set_level(spdlog::level::debug);
        m_console_debug->set_pattern("[%H:%M:%S.%e pid=%P tid=%t] %v");
        //
        // syslog
        //
        m_syslog_info = spdlog::syslog_logger_mt("syslog_info");
        m_syslog_info->set_level(spdlog::level::info);

        m_syslog_error = spdlog::syslog_logger_mt("syslog_error");
        m_syslog_error->set_level(spdlog::level::err);

        m_syslog_warn = spdlog::syslog_logger_mt("syslog_warn");
        m_syslog_warn->set_level(spdlog::level::warn);

        m_syslog_debug = spdlog::syslog_logger_mt("syslog_debug");
        m_syslog_debug->set_level(spdlog::level::debug);
    }

    ~Logger(void) {
        spdlog::shutdown();
    }

public:
    //
    static Logger &
    TheLogger(void) {
        static Logger singleton;
        return singleton;
    }

    //Disable copy constructor.
    Logger(const Logger &) = delete;

    // Just return the singleton.
    Logger &
    operator=(const Logger &) { return Logger::TheLogger(); }

    //
    // console
    //
    static ilogger_t
    console_info(void) { return Logger::TheLogger().m_console_info; }
    //
    static ilogger_t
    console_warn(void) { return Logger::TheLogger().m_console_warn; }
    //
    static ilogger_t
    console_error(void) { return Logger::TheLogger().m_console_error; }
    //
    static ilogger_t
    console_debug(void) { return Logger::TheLogger().m_console_debug; }

    //
    // syslog
    //
    static ilogger_t
    syslog_info(void) { return Logger::TheLogger().m_syslog_info; }
    //
    static ilogger_t
    syslog_warn(void) { return Logger::TheLogger().m_syslog_warn; }
    //
    static ilogger_t
    syslog_error(void) { return Logger::TheLogger().m_syslog_error; }
    //
    static ilogger_t
    syslog_debug(void) { return Logger::TheLogger().m_syslog_debug; }

};

#ifdef __cplusplus
extern "C" {
#endif
//
// console
//
#define QVI_LOG_INFO(...) \
SPDLOG_LOGGER_INFO(Logger::console_info(), __VA_ARGS__)

#define QVI_LOG_WARN(...) \
SPDLOG_LOGGER_WARN(Logger::console_warn(), __VA_ARGS__)

#define QVI_LOG_ERROR(...) \
SPDLOG_LOGGER_ERROR(Logger::console_error(), __VA_ARGS__)

#define QVI_LOG_DEBUG(...) \
SPDLOG_LOGGER_DEBUG(Logger::console_debug(), __VA_ARGS__)
//
// syslog
//
#define QVI_SYSLOG_INFO(...) \
SPDLOG_LOGGER_INFO(Logger::syslog_info(), __VA_ARGS__)

#define QVI_SYSLOG_WARN(...) \
SPDLOG_LOGGER_WARN(Logger::syslog_warn(), __VA_ARGS__)

#define QVI_SYSLOG_ERROR(...) \
SPDLOG_LOGGER_ERROR(Logger::syslog_error(), __VA_ARGS__)

#define QVI_SYSLOG_DEBUG(...) \
SPDLOG_LOGGER_DEBUG(Logger::syslog_debug(), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
