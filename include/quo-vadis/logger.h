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

#ifndef QUO_VADIS_LOGGER_H
#define QUO_VADIS_LOGGER_H

#include "quo-vadis/config.h"

// TODO(skg) Add compile-time switch to toggle debug level
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/syslog_sink.h"

class qvi_logger {
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
    //
    qvi_logger(void);
    //
    ~qvi_logger(void);

public:
    //
    static qvi_logger &
    the_qvi_logger(void);
    //Disable copy constructor.
    qvi_logger(const qvi_logger &) = delete;
    // Just return the singleton.
    qvi_logger &
    operator=(const qvi_logger &);
    //
    // console
    //
    static ilogger_t
    console_info(void);
    //
    static ilogger_t
    console_warn(void);
    //
    static ilogger_t
    console_error(void);
    //
    static ilogger_t
    console_debug(void);
    //
    // syslog
    //
    static ilogger_t
    syslog_info(void);
    //
    static ilogger_t
    syslog_warn(void);
    //
    static ilogger_t
    syslog_error(void);
    //
    static ilogger_t
    syslog_debug(void);
};

//
// console
//
#define QVI_LOG_INFO(...) \
SPDLOG_LOGGER_INFO(qvi_logger::console_info(), __VA_ARGS__)

#define QVI_LOG_WARN(...) \
SPDLOG_LOGGER_WARN(qvi_logger::console_warn(), __VA_ARGS__)

#define QVI_LOG_ERROR(...) \
SPDLOG_LOGGER_ERROR(qvi_logger::console_error(), __VA_ARGS__)

#define QVI_PANIC_LOG_ERROR(...)                                               \
do {                                                                           \
    SPDLOG_LOGGER_ERROR(qvi_logger::console_error(), __VA_ARGS__);             \
    _exit(EXIT_FAILURE);                                                       \
} while (0)

#define QVI_LOG_DEBUG(...) \
SPDLOG_LOGGER_DEBUG(qvi_logger::console_debug(), __VA_ARGS__)
//
// syslog
//
#define QVI_SYSLOG_INFO(...) \
SPDLOG_LOGGER_INFO(qvi_logger::syslog_info(), __VA_ARGS__)

#define QVI_SYSLOG_WARN(...) \
SPDLOG_LOGGER_WARN(qvi_logger::syslog_warn(), __VA_ARGS__)

#define QVI_SYSLOG_ERROR(...) \
SPDLOG_LOGGER_ERROR(qvi_logger::syslog_error(), __VA_ARGS__)

#define QVI_PANIC_SYSLOG_ERROR(...)                                            \
do {                                                                           \
    SPDLOG_LOGGER_ERROR(qvi_logger::syslog_error(), __VA_ARGS__);              \
    _exit(EXIT_FAILURE);                                                       \
} while (0)

#define QVI_SYSLOG_DEBUG(...) \
SPDLOG_LOGGER_DEBUG(qvi_logger::syslog_debug(), __VA_ARGS__)

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
