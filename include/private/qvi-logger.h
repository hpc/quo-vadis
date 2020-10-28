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
 * @file qvi-logger.h
 */

#ifndef QVI_LOGGER_H
#define QVI_LOGGER_H

#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif
#include "spdlog/spdlog.h"

class qvi_logger {
    // Convenience internal logger type alias.
    using logger_t = decltype(spdlog::get(""));
    // Log sinks.
    // console (stdout)
    logger_t m_console_info;
    logger_t m_console_error;
    logger_t m_console_warn;
    logger_t m_console_debug;
    // syslog
    logger_t m_syslog_info;
    logger_t m_syslog_error;
    logger_t m_syslog_warn;
    logger_t m_syslog_debug;
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
    static logger_t
    console_info(void);
    //
    static logger_t
    console_warn(void);
    //
    static logger_t
    console_error(void);
    //
    static logger_t
    console_debug(void);
    //
    // syslog
    //
    static logger_t
    syslog_info(void);
    //
    static logger_t
    syslog_warn(void);
    //
    static logger_t
    syslog_error(void);
    //
    static logger_t
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
