/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TD_OS_SOCKET_H_
#define _TD_OS_SOCKET_H_

// If the error is in a third-party library, place this header file under the third-party library header file.
#ifndef ALLOW_FORBID_FUNC
    #define socket SOCKET_FUNC_TAOS_FORBID
    #define bind   BIND_FUNC_TAOS_FORBID
    #define listen LISTEN_FUNC_TAOS_FORBID
    // #define accept ACCEPT_FUNC_TAOS_FORBID
#endif

#if defined(_TD_WINDOWS_64) || defined(_TD_WINDOWS_32)
  #include "winsock2.h"
  #include <WS2tcpip.h>
  #include <winbase.h>
  #include <Winsock2.h>
#else
  #include <netinet/in.h>
  #include <sys/epoll.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TAOS_EPOLL_WAIT_TIME 500
typedef int32_t SOCKET;
typedef SOCKET  EpollFd;
#define EpollClose(pollFd) taosCloseSocket(pollFd)

#if defined(_TD_WINDOWS_64) || defined(_TD_WINDOWS_32)
typedef SOCKET SocketFd;
#else
typedef int32_t SocketFd;
#endif

int32_t taosSendto(SocketFd fd, void * msg, int len, unsigned int flags, const struct sockaddr * to, int tolen);
int32_t taosWriteSocket(SocketFd fd, void *msg, int len);
int32_t taosReadSocket(SocketFd fd, void *msg, int len);
int32_t taosCloseSocketNoCheck(SocketFd fd);
int32_t taosCloseSocket(SocketFd fd);
void    taosShutDownSocketRD(SOCKET fd);
void    taosShutDownSocketWR(SOCKET fd);
int32_t taosSetNonblocking(SOCKET sock, int32_t on);
int32_t taosSetSockOpt(SOCKET socketfd, int32_t level, int32_t optname, void *optval, int32_t optlen);
int32_t taosGetSockOpt(SOCKET socketfd, int32_t level, int32_t optname, void *optval, int32_t *optlen);

uint32_t    taosInetAddr(const char *ipAddr);
const char *taosInetNtoa(struct in_addr ipInt);

#if (defined(_TD_WINDOWS_64) || defined(_TD_WINDOWS_32)) 
  #define htobe64 htonll
  #if defined(_TD_GO_DLL_)
    uint64_t htonll(uint64_t val);
  #endif
#endif

#if defined(_TD_DARWIN_64)
  #define htobe64 htonll
#endif

int32_t taosReadn(SOCKET sock, char *buffer, int32_t len);
int32_t taosWriteMsg(SOCKET fd, void *ptr, int32_t nbytes);
int32_t taosReadMsg(SOCKET fd, void *ptr, int32_t nbytes);
int32_t taosNonblockwrite(SOCKET fd, char *ptr, int32_t nbytes);
int64_t taosCopyFds(SOCKET sfd, int32_t dfd, int64_t len);
int32_t taosSetNonblocking(SOCKET sock, int32_t on);

SOCKET  taosOpenUdpSocket(uint32_t localIp, uint16_t localPort);
SOCKET  taosOpenTcpClientSocket(uint32_t ip, uint16_t port, uint32_t localIp);
SOCKET  taosOpenTcpServerSocket(uint32_t ip, uint16_t port);
int32_t taosKeepTcpAlive(SOCKET sockFd);

void    taosBlockSIGPIPE();
uint32_t taosGetIpv4FromFqdn(const char *);
int32_t  taosGetFqdn(char *);
void     tinet_ntoa(char *ipstr, uint32_t ip);
uint32_t ip2uint(const char *const ip_addr);
void    taosIgnSIGPIPE();
void    taosSetMaskSIGPIPE();

#ifdef __cplusplus
}
#endif

#endif /*_TD_OS_SOCKET_H_*/
