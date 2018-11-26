/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * powertest.h - definitions for code to run continuously, making omar burn as much power as possible
 */

#pragma once

#define POWERTEST_SSID      ("Stringer 2.4")
#define POWERTEST_PASSWORD  ("stringertest")

#define ECHOSERVER_NAME     ("jenkins.currant.com")
#define ECHOSERVER_PORT     (7)


void tcpip_echo_task_start_up(void);
void tcpip_echo_task_shut_down(void);

