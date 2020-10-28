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
 * @file qvi-logger.cc
 */

#include "quo-vadis/config.h"

#include "private/qvi-logger.h"

#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/syslog_sink.h"

qvi_logger::qvi_logger(void)
{
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

qvi_logger::~qvi_logger(void)
{
    spdlog::shutdown();
}

qvi_logger &
qvi_logger::the_qvi_logger(void)
{
    static qvi_logger singleton;
    return singleton;
}

qvi_logger &
qvi_logger::operator=(const qvi_logger &)
{
    // Just return the singleton.
    return qvi_logger::the_qvi_logger();
}

//
// console
//
qvi_logger::logger_t
qvi_logger::console_info(void)
{
    return qvi_logger::the_qvi_logger().m_console_info;
}

qvi_logger::logger_t
qvi_logger::console_warn(void)
{
    return qvi_logger::the_qvi_logger().m_console_warn;
}

qvi_logger::logger_t
qvi_logger::console_error(void)
{
    return qvi_logger::the_qvi_logger().m_console_error;
}

qvi_logger::logger_t
qvi_logger::console_debug(void)
{
    return qvi_logger::the_qvi_logger().m_console_debug;
}

//
// syslog
//
qvi_logger::logger_t
qvi_logger::syslog_info(void)
{
    return qvi_logger::the_qvi_logger().m_syslog_info;
}

qvi_logger::logger_t
qvi_logger::syslog_warn(void)
{
    return qvi_logger::the_qvi_logger().m_syslog_warn;
}

qvi_logger::logger_t
qvi_logger::syslog_error(void)
{
    return qvi_logger::the_qvi_logger().m_syslog_error;
}

qvi_logger::logger_t
qvi_logger::syslog_debug(void)
{
    return qvi_logger::the_qvi_logger().m_syslog_debug;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
