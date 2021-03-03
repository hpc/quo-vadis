/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-log.cc
 */

#include "qvi-common.h"
#include "qvi-log.h"

#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/syslog_sink.h"
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

qvi_logger::qvi_logger(void)
{
    // Formatting applied globally to all registered loggers
    spdlog::set_pattern("[" PACKAGE_NAME " %l at (%s::%!::%#)] %v");
    //
    // Console
    //
    m_console_info = spdlog::stderr_logger_mt("console_info");
    m_console_info->set_level(spdlog::level::info);
    m_console_info->set_pattern("%v");
    m_console_info->flush_on(m_console_info->level());

    m_console_error = spdlog::stderr_logger_mt("console_error");
    m_console_error->set_level(spdlog::level::err);
    m_console_error->flush_on(m_console_error->level());

    m_console_warn = spdlog::stderr_logger_mt("console_warn");
    m_console_warn->set_level(spdlog::level::warn);

    m_console_debug = spdlog::stderr_logger_mt("console_debug");
    m_console_debug->set_level(spdlog::level::debug);
    m_console_debug->set_pattern("[%H:%M:%S.%e pid=%P tid=%t] %v");
    m_console_debug->flush_on(m_console_debug->level());
    //
    // Syslog
    //
    m_syslog_info = spdlog::syslog_logger_mt("syslog_info");
    m_syslog_info->set_level(spdlog::level::info);
    m_syslog_info->flush_on(m_syslog_info->level());

    m_syslog_error = spdlog::syslog_logger_mt("syslog_error");
    m_syslog_error->set_level(spdlog::level::err);
    m_syslog_error->flush_on(m_syslog_error->level());

    m_syslog_warn = spdlog::syslog_logger_mt("syslog_warn");
    m_syslog_warn->set_level(spdlog::level::warn);
    m_syslog_warn->flush_on(m_syslog_warn->level());

    m_syslog_debug = spdlog::syslog_logger_mt("syslog_debug");
    m_syslog_debug->set_level(spdlog::level::debug);
    m_syslog_debug->flush_on(m_syslog_debug->level());
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
// Console
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
// Syslog
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

void
qvi_logger::console_to_syslog(void)
{
    auto &logger = qvi_logger::the_qvi_logger();

    spdlog::drop(logger.m_console_info->name());
    spdlog::drop(logger.m_console_error->name());
    spdlog::drop(logger.m_console_warn->name());
    spdlog::drop(logger.m_console_debug->name());

    logger.m_console_info = spdlog::syslog_logger_mt("consys_info");
    logger.m_console_info->set_level(spdlog::level::info);
    logger.m_console_info->set_pattern("%v");
    logger.m_console_info->flush_on(logger.m_console_info->level());

    logger.m_console_error = spdlog::syslog_logger_mt("consys_error");
    logger.m_console_error->set_level(spdlog::level::err);
    logger.m_console_error->flush_on(logger.m_console_error->level());

    logger.m_console_warn = spdlog::syslog_logger_mt("consys_warn");
    logger.m_console_warn->set_level(spdlog::level::warn);
    logger.m_console_warn->flush_on(logger.m_console_warn->level());

    logger.m_console_debug = spdlog::syslog_logger_mt("consys_debug");
    logger.m_console_debug->set_level(spdlog::level::debug);
    logger.m_console_debug->flush_on(logger.m_console_debug->level());
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
