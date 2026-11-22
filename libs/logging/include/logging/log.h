#pragma once

#include <stdio.h>

// уровни журнала
#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_INF 2
#define LOG_LEVEL_DBG 3

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INF
#endif

#if LOG_LEVEL >= LOG_LEVEL_ERR
#define LOG_ERR(fmt, ...) printf("err %s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_ERR(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INF
#define LOG_INF(fmt, ...) printf("inf %s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_INF(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DBG
#define LOG_DBG(fmt, ...) printf("dbg %s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...) ((void)0)
#endif
