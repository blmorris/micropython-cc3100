// --------------------------------------------------------------------------------------
// Module     : MODSOCK
// Author     : N. El-Fata
//
// Description: This module is the high level python socket interface which implements
//              most standard socket functionality. It uses the cc3k module as a layer
//              for socket management.
//
// --------------------------------------------------------------------------------------
// Pulse Innovation, Inc. www.pulseinnovation.com
// Copyright (c) 2014, Pulse Innovation, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
//#include <sys/time.h>

#include "mpconfig.h"
#include "nlr.h"
#include "misc.h"
#include "qstr.h"
#include "obj.h"
#include "objtuple.h"
#include "objarray.h"
#include "stream.h"

#ifdef __arm__
#include "inet_pton.h"
#include "inet_ntop.h"
#else // Linux
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "log.h"
#include "utils.h"

//#include "cc3k.h"
#include "modwifi.h"
#include "../stmhal/modsock.h"

//
// Global functions
//

STATIC mp_obj_t sock_gethostbyname(const mp_obj_t str)
{
    int rc;
    uint32_t addr = 0;
    char ip_str[INET_ADDRSTRLEN];
    const char *rs;

    RAISE_EXCEP(str, mp_type_RuntimeError, "null pointer");
    RAISE_EXCEP(MP_OBJ_IS_STR(str), mp_type_TypeError, "invalid type");

    char *s = (char *)mp_obj_str_get_str(str);
    RAISE_EXCEP(s, mp_type_TypeError, "null pointer");
    RAISE_EXCEP(s[0], mp_type_TypeError, "empty string");

#ifdef __arm__
    rc = wifi_dev_gethostbyname(s, strlen(s), &addr); // addr is in network order
#else
    rc = 0;
    strcpy(s,"10.0.0.1");
    return(mp_obj_new_str(s, strlen(s), false));
#endif

    RAISE_EXCEP(rc>=0, mp_type_BaseException, "gethostbyname failed");

    addr = ntohl(addr);
    rs = inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
    RAISE_EXCEP(rs, mp_type_BaseException, "inet_ntop failed");

    return(mp_obj_new_str(ip_str, strlen(ip_str), false));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sock_gethostbyname_obj, sock_gethostbyname);

//
// Local
//

typedef struct _mp_obj_mod_t
{   // linked list
    mp_obj_base_t base;
    int fd;
} mp_obj_mod_t;

STATIC const mp_obj_type_t mod_type;

STATIC mp_obj_t sock_close(mp_obj_t self_in)
{
    mp_obj_mod_t *self = self_in;

#ifdef __arm__
    if(self->fd < 0)
    {
        //printf("socket already closed\n");
        return(mp_const_none);
    }

    int rc = wifi_dev_sock_close(self->fd);
    if (rc < 0) printf("rc %d\n", rc);

    // mark it as invalid, meaning it was closed, irrelevant of rc
    self->fd = -1;

    //RAISE_EXCEP(rc==0, mp_type_BaseException, "closesocket failed");
#else
    close(self->fd);
#endif

    return(mp_const_none);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sock_close_obj, sock_close);

// Checks to see if data is available for reading
STATIC int _sock_isavailable(int fd, int r_or_w, int timeout_ms)
{
    int rc;

    if(fd < 0)
    {
        return(-1);
    }
#if MICROPY_HW_ENABLE_CC3K
    if(wifi_dev_socket_isclosed(fd))
    {
        return(-1);
    }
#endif
    // do a select call on this socket
    struct timeval timeout, *ptimeout;
    fd_set *fd_read, *fd_write;
    fd_set fd_fd;

    FD_ZERO(&fd_fd);
    FD_SET(fd, &fd_fd);

    if(timeout_ms < 0)
    {   // infinite timeout
        ptimeout = 0;
    }
    else
    {
        ptimeout        = &timeout;
        timeout.tv_sec  = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms - (timeout.tv_sec * 1000)) * 1000;
        //printf("s %ld us %ld\n", timeout.tv_sec, timeout.tv_usec);
    }

    if(r_or_w)
    {   // check data to read
        fd_read  = &fd_fd;
        fd_write = 0;
    }
    else
    {   // check data to write
        fd_read = 0;
        fd_write = &fd_fd;
    }

    rc = wifi_dev_sock_select(fd+1, fd_read, fd_write, NULL, ptimeout);
    if (rc < 0) printf("rc %d\n", rc);
    if(rc == 0)
    {   // select timed out, no data available
        printf("select timedout\n");
    }
    else
    {
        if(rc < 0)
        {   // select failed, socket was closed, or disconnected
            printf("select failed\n");
            rc = -1;
        }
        else
        {   // data is available to read or write
            rc = FD_ISSET(fd, &fd_fd);
            if(rc <= 0)
            {
                printf("FD_ISSET not ready\n");
                rc = -2;
            }
            else
            {   // all is good, socket is available for read or write
                rc = 1;
            }
        }
    }
    return(rc);
}

STATIC mp_obj_t sock_isavailable(mp_obj_t self_in, mp_obj_t read_or_write)
{
    int rc;
    mp_obj_mod_t *self = self_in;
    int r_or_w = MP_OBJ_SMALL_INT_VALUE(read_or_write);

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    rc = _sock_isavailable(self->fd, r_or_w, 10);
    if(rc < 0)
    {
        rc = 0;
    }

    return(mp_obj_new_int(rc));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(sock_isavailable_obj, sock_isavailable);

// Checks to see if the socket is valid, not closed, and there is no data pending for reading
STATIC mp_obj_t sock_isconnected(mp_obj_t self_in)
{
    int          rc = 1; // assume connected
    mp_obj_mod_t *self = self_in;

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    // check if the socket closed event occured
#if MICROPY_HW_ENABLE_CC3K
    if(wifi_dev_socket_isclosed(self->fd))
    {   // event socket closed was set, close the socket
        // sock_close(self_in);
        rc = 0;
    }
#endif
#if 0
    else
    {   // no closed event detected, but we need to see if the socket is still accessible/available
        if(_sock_isavailable(self_in) < 0)
        {   // failed to read from the socket, close the socket
            sock_close(self_in);
            rc = 0;
        }
    }
#endif

    return(mp_obj_new_int(rc));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sock_isconnected_obj, sock_isconnected);

STATIC void sock_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t self_in, mp_print_kind_t kind)
{
    mp_obj_mod_t *self = self_in;
    print(env, "%d", self->fd);
}

STATIC mp_obj_mod_t *sock_new(int fd)
{
    mp_obj_mod_t *o = m_new_obj(mp_obj_mod_t);
    o->base.type = &mod_type;
    o->fd = fd;
    //printf("new sock fd %d\n", fd);
    return(o);
}

//
// Constructor
//
STATIC mp_obj_t sock_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args)
{
    int family = AF_INET;
    int type   = SOCK_STREAM;
    int proto  = 0;//IPPROTO_TCP;
    int fd;

    if(n_args > 0)
    {
        assert(MP_OBJ_IS_SMALL_INT(args[0]));
        family = MP_OBJ_SMALL_INT_VALUE(args[0]);
        if(n_args > 1)
        {
            assert(MP_OBJ_IS_SMALL_INT(args[1]));
            type = MP_OBJ_SMALL_INT_VALUE(args[1]);
            if(n_args > 2)
            {
                assert(MP_OBJ_IS_SMALL_INT(args[2]));
                proto = MP_OBJ_SMALL_INT_VALUE(args[2]);
            }
        }
    }

    fd = wifi_dev_socket(family, type, proto);
    if (fd < 0) printf("fd %d\n", fd);

    RAISE_EXCEP(fd>=0, mp_type_RuntimeError, "socket failed");

    return(sock_new(fd));
}

//
// Class members
//

STATIC mp_obj_t sock_connect(uint n_args, const mp_obj_t *args)
{
    int rc;
    mp_obj_mod_t *self;
#ifdef __arm__
    sockaddr_in addr;
#else
    struct sockaddr_in addr;
#endif
    const char *host;
    short port = 0;

    // Validate args
    RAISE_EXCEP(n_args==3, mp_type_TypeError, "no arguments");
    RAISE_EXCEP(args, mp_type_RuntimeError, "null argument");
    // Validate arg0
    RAISE_EXCEP(args[0], mp_type_RuntimeError, "null argument");
    self = args[0];

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    // Validate arg1: IP address (IP format xxx.xxx.xxx.xxx)
    RAISE_EXCEP(args[1], mp_type_RuntimeError, "null argument");
    RAISE_EXCEP(MP_OBJ_IS_STR(args[1]), mp_type_TypeError, "invalid type");
    // Get the host name
    host = mp_obj_str_get_str(args[1]);
    RAISE_EXCEP(host, mp_type_TypeError, "host: null pointer");
    RAISE_EXCEP(host[0], mp_type_TypeError, "host: empty string");

    // Validate arg2: Port number
    RAISE_EXCEP(args[2], mp_type_RuntimeError, "port: null argument");
    RAISE_EXCEP(MP_OBJ_IS_SMALL_INT(args[2]), mp_type_TypeError, "port: invalid type");
    // Get the port number
    port = (short)mp_obj_int_get(args[2]);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    rc = inet_pton(AF_INET, host, &addr.sin_addr); // converts to network order
    RAISE_EXCEP(rc, mp_type_ValueError, "invalid IP address");

    printf("Connecting %s:%d %08lX\n", host, port, (unsigned long)addr.sin_addr.s_addr);

#ifdef __arm__
    rc = wifi_dev_sock_connect(self->fd, (const sockaddr *)&addr, sizeof(addr));
#else
    rc = connect(self->fd, (const struct sockaddr *)&addr, sizeof(addr));
#endif
    if (rc < 0) printf("rc %d\n", rc);
    RAISE_EXCEP(rc==0, mp_type_BaseException, "connect failed");

    return(mp_obj_new_int(rc));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(sock_connect_obj, 3, sock_connect);

STATIC mp_obj_t sock_send(uint n_args, const mp_obj_t *args)
{
    mp_obj_mod_t *self = args[0];
    int flags = 0;

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    if(n_args > 2)
    {
        flags = MP_OBJ_SMALL_INT_VALUE(args[2]);
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);

    int out_sz = wifi_dev_sock_send(self->fd, bufinfo.buf, bufinfo.len, flags);

    RAISE_EXCEP(out_sz>0, mp_type_BaseException, "send failed");

    return(MP_OBJ_NEW_SMALL_INT((mp_int_t)out_sz));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sock_send_obj, 2, 3, sock_send);

STATIC mp_obj_t sock_write(uint n_args, const mp_obj_t *args)
{
    int rc = 0;
    int len;
    mp_obj_mod_t *self = args[0];
    int flags = MSG_NOSIGNAL;

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");
#if MICROPY_HW_ENABLE_CC3K
    RAISE_EXCEP(!wifi_dev_socket_isclosed(self->fd), mp_type_BaseException, "sock_send socket has closed unexpectedly");
#endif
    if(n_args > 2)
    {
        RAISE_EXCEP(args[2], mp_type_RuntimeError, "null argument");
        RAISE_EXCEP(MP_OBJ_IS_SMALL_INT(args[2]), mp_type_TypeError, "invalid type");
        flags = MP_OBJ_SMALL_INT_VALUE(args[2]);
    }
    RAISE_EXCEP(args[1], mp_type_RuntimeError, "null argument");
    // get the length
    len = MP_OBJ_SMALL_INT_VALUE(args[1]);

    // get the buffer
    mp_buffer_info_t bufinfo;

    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);

    // check to see if we can send
    rc = _sock_isavailable(self->fd, 0, -1);
    if(rc <= 0)
    {
        printf("cannot write to socket\n");
        rc = -1;
    }
    else
    {   // send the data
        rc = wifi_dev_sock_send(self->fd, bufinfo.buf, len, flags);
        if(rc < 0)
        {
            printf("wifi_sock_send failed\n");
        }
    }
#if MICROPY_HW_ENABLE_CC3K
    if(wifi_dev_socket_isclosed(self->fd))
    {
        rc = -1;
    }
#endif
    RAISE_EXCEP(rc>0, mp_type_BaseException, "send failed");

    return(MP_OBJ_NEW_SMALL_INT((mp_int_t)rc));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sock_write_obj, 2, 3, sock_write);

STATIC mp_obj_t sock_recv(uint n_args, const mp_obj_t *args)
{
    mp_obj_mod_t *self = args[0];
    mp_obj_t ret = 0;
    int len = MP_OBJ_SMALL_INT_VALUE(args[1]);
    int flags = 0;

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    if(n_args > 2)
    {
        flags = MP_OBJ_SMALL_INT_VALUE(args[2]);
    }

    // allocate buffer
    char *buf = m_new(char, len);
    RAISE_EXCEP(buf, mp_type_BaseException, "buf m_new failed");

    int out_len = wifi_dev_sock_recv(self->fd, buf, len, flags);
    if(out_len >= 0)
    {
        ret = MP_OBJ_NEW_QSTR(qstr_from_strn(buf, out_len));
    }

    // free the buffer
    m_del(char, buf, len);

    // raise the exception last as we need to make sure the buffer is freed before
    RAISE_EXCEP(out_len>=0, mp_type_BaseException, "recv failed");

    return(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sock_recv_obj, 2, 3, sock_recv);

STATIC mp_obj_t sock_read(uint n_args, const mp_obj_t *args)
{
    int rc = -1;
    mp_obj_mod_t *self = args[0];
    int len, out_len = 0;
    int flags = 0;
    int timeout = -1;   // infinite timeout by default
    char *buf = 0;

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    len = MP_OBJ_SMALL_INT_VALUE(args[1]);

    if(n_args > 2)
    {
        flags = MP_OBJ_SMALL_INT_VALUE(args[2]);
        if(n_args > 3)
        {
            timeout = MP_OBJ_SMALL_INT_VALUE(args[3]);
        }
    }

    // check for any client data
    rc = _sock_isavailable(self->fd, 1, timeout);
    RAISE_EXCEP(rc>=0, mp_type_ReadError, "_sock_isavailable failed");
    RAISE_EXCEP(rc!=0, mp_type_NoDataError, "no data");

    // data available to read, allocate buffer
    buf = m_new(char, len);
    RAISE_EXCEP(buf, mp_type_BaseException, "buf m_new failed");

    // read data
    out_len = wifi_dev_sock_recv(self->fd, buf, len, flags);
    if(out_len <= 0)
    {
        RAISE_EXCEP(out_len>=0, mp_type_BaseException, "recv failed");
        RAISE_EXCEP(out_len!=0, mp_type_BaseException, "recv failed with 0 data");
        // Yes, this really does happen
        RAISE_EXCEP(out_len<=len, mp_type_BaseException, "recv overflow");
    }

    //printf("recv: %d bytes\n", out_len);

    mp_obj_t ret = MP_OBJ_NEW_QSTR(qstr_from_strn(buf, out_len));
    m_del(char, buf, len);

    return(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sock_read_obj, 2, 4, sock_read);

STATIC mp_obj_t sock_setsockopt(uint n_args, const mp_obj_t *args)
{
    int rc;
    mp_obj_mod_t *self;
    int level;
    int option;
    const void *optval;
    socklen_t optlen;

    // Validate args
    RAISE_EXCEP(n_args==4, mp_type_RuntimeError, "no arguments");
    RAISE_EXCEP(args, mp_type_RuntimeError, "null argument");
    // Validate arg0
    RAISE_EXCEP(args[0], mp_type_RuntimeError, "null argument");
    self = args[0];

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    // Validate arg1:
    RAISE_EXCEP(args[1], mp_type_RuntimeError, "null argument");
    RAISE_EXCEP(MP_OBJ_IS_INT(args[1]), mp_type_TypeError, "invalid type");
    level = MP_OBJ_SMALL_INT_VALUE(args[1]);
    // Validate arg2:
    RAISE_EXCEP(args[2], mp_type_RuntimeError, "null argument");
    RAISE_EXCEP(MP_OBJ_IS_INT(args[2]), mp_type_TypeError, "invalid type");
    option = mp_obj_get_int(args[2]);

    // Validate arg3:
    RAISE_EXCEP(args[3], mp_type_RuntimeError, "null argument");
    int val;
    mp_buffer_info_t bufinfo;
    if(MP_OBJ_IS_INT(args[3]))
    {
        val    = mp_obj_int_get(args[3]);
        optval = &val;
        optlen = sizeof(val);
        //printf("level %d option %d optval %d optlen %d\n", level,option,val,(int)optlen);
    }
    else
    {
        mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);
        optval = bufinfo.buf;
        optlen = bufinfo.len;
    }

    rc = wifi_dev_sock_setsockopt(self->fd, level, option, optval, optlen);
    if (rc < 0) printf("rc %d\n", rc);
    RAISE_EXCEP(rc>=0, mp_type_BaseException, "setsockopt failed");

    return(mp_const_none);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sock_setsockopt_obj, 4, 4, sock_setsockopt);

STATIC mp_obj_t sock_bind(uint n_args, const mp_obj_t *args)
{
    int rc;
    mp_obj_mod_t *self;
    const char *host;
    short port = 0;
#ifdef __arm__
    sockaddr_in addr;
#else
    struct sockaddr_in addr;
#endif

    // Validate args
    RAISE_EXCEP(n_args==3, mp_type_TypeError, "no arguments");
    RAISE_EXCEP(args, mp_type_RuntimeError, "null argument");
    // Validate arg0
    RAISE_EXCEP(args[0], mp_type_RuntimeError, "null argument");
    self = args[0];

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    // Validate arg1: IP address (IP format xxx.xxx.xxx.xxx)
    RAISE_EXCEP(args[1], mp_type_RuntimeError, "IP: null argument");
    RAISE_EXCEP(MP_OBJ_IS_STR(args[1]), mp_type_TypeError, "IP: invalid type");
    // Get the host name
    host = mp_obj_str_get_str(args[1]);
    RAISE_EXCEP(host, mp_type_RuntimeError, "host: null pointer");
    RAISE_EXCEP(host[0], mp_type_TypeError, "host: empty string");

    // Validate arg2: Port number
    RAISE_EXCEP(args[2], mp_type_RuntimeError, "port: null argument");
    RAISE_EXCEP(MP_OBJ_IS_SMALL_INT(args[2]), mp_type_TypeError, "port: invalid type");
    // Get the port number
    port = (short)mp_obj_int_get(args[2]);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    rc = inet_pton(AF_INET, host, &addr.sin_addr); // converts to network order
    RAISE_EXCEP(rc, mp_type_ValueError, "invalid IP address");

    //printf("Binding %s:%d %08lX\n", host, port, (unsigned long)addr.sin_addr.s_addr);

#ifdef __arm__
    rc = wifi_dev_sock_bind(self->fd, (const sockaddr *)&addr, sizeof(addr));
#else
    rc = bind(self->fd, (const struct sockaddr *)&addr, sizeof(addr));
#endif
    if (rc < 0) printf("rc %d\n", rc);

    RAISE_EXCEP(rc==0, mp_type_BaseException, "bind failed");

    return(mp_obj_new_int(rc));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(sock_bind_obj, 3, sock_bind);

STATIC mp_obj_t sock_listen(mp_obj_t self_in, mp_obj_t backlog_in)
{
    mp_obj_mod_t *self = self_in;

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    int rc = wifi_dev_sock_listen(self->fd, MP_OBJ_SMALL_INT_VALUE(backlog_in));
    if (rc < 0) printf("rc %d\n", rc);
    RAISE_EXCEP(rc>=0, mp_type_BaseException, "listen failed");

    return(mp_const_none);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(sock_listen_obj, sock_listen);

STATIC mp_obj_t sock_accept(mp_obj_t self_in)
{
    mp_obj_mod_t *self = self_in;
#ifdef __arm__
    sockaddr addr;
#else
    struct sockaddr addr;
#endif
    socklen_t addr_len = sizeof(addr);

    RAISE_EXCEP(self->fd >= 0, mp_type_BaseException, "bad file descriptor");

    int fd = wifi_dev_sock_accept(self->fd, &addr, &addr_len);

    switch(fd)
    {
        case SOC_ERROR:         // case non-blocking
            // failure, socket must be closed and reopened, this might be due to Wifi disconnected
            // so before reopening the socket, isonline() should be checked
            break;
        case SL_EAGAIN:         // case non-blocking
        case SOC_IN_PROGRESS:   // case non-blocking
            // no connection yet
            return(mp_const_none);
        case 0:
            printf("WARN: fd is 0\n");
            break;
        default:
            break;
    }

    // error if fd is negative
    RAISE_EXCEP(fd >= 0, mp_type_RuntimeError, "accept failed");

    mp_obj_tuple_t *t = mp_obj_new_tuple(2, NULL);
    RAISE_EXCEP(t, mp_type_BaseException, "mp_obj_new_tuple failed");

    t->items[0] = sock_new(fd);
    RAISE_EXCEP(t->items[0], mp_type_BaseException, "t->items[0] failed");
    t->items[1] = mp_obj_new_bytearray(addr_len, &addr);
    RAISE_EXCEP(t->items[1], mp_type_BaseException, "t->items[1] failed");

    return(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sock_accept_obj, sock_accept);


STATIC mp_obj_t sock_tcpserver(mp_obj_t self_in, mp_obj_t port)
{
#define BUF_SIZE        1400
#define NO_OF_PACKETS   1000
union
{
    UINT8 BsdBuf[BUF_SIZE];
    UINT32 demobuf[BUF_SIZE/4];
} uBuf;

    mp_obj_mod_t *self = self_in;
    int _port = MP_OBJ_SMALL_INT_VALUE(port);

    SlSockAddrIn_t  Addr;
    SlSockAddrIn_t  LocalAddr;

    uint16_t          idx = 0;
    uint16_t          AddrSize = 0;
    int16_t           SockID = 0;
    int16_t           Status = 0;
    int16_t           newSockID = 0;
    uint16_t          LoopCount = 0;
    int32_t           nonBlocking = 1;

    self = self;

    for(idx=0 ; idx<BUF_SIZE ; idx++)
    {
        uBuf.BsdBuf[idx] = (char)(idx % 10);
    }

    LocalAddr.sin_family = SL_AF_INET;
    LocalAddr.sin_port = sl_Htons((uint16_t)_port);
    LocalAddr.sin_addr.s_addr = 0;

    SockID = wifi_dev_socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if(SockID < 0)
    {
        LOG_ERR("");
        return(mp_const_none);
    }

    AddrSize = sizeof(SlSockAddrIn_t);
    Status = wifi_dev_sock_bind(SockID, (SlSockAddr_t *)&LocalAddr, AddrSize);
    if(Status < 0)
    {
        LOG_ERR("");
        wifi_dev_sock_close(SockID);
        return(mp_const_none);
    }

    Status = wifi_dev_sock_listen(SockID, 0);
    if(Status < 0)
    {
        LOG_ERR("");
        wifi_dev_sock_close(SockID);
        return(mp_const_none);
    }

    Status = wifi_dev_sock_setsockopt(SockID, SL_SOL_SOCKET, SL_SO_NONBLOCKING, &nonBlocking, sizeof(nonBlocking));
    if(Status < 0)
    {
        LOG_ERR("");
        wifi_dev_sock_close(SockID);
        return(mp_const_none);
    }

    printf(" Started TCP server\r\n");

    newSockID = SL_EAGAIN;
    while(newSockID < 0)
    {
        newSockID = wifi_dev_sock_accept(SockID, ( struct SlSockAddr_t *)&Addr, (SlSocklen_t*)&AddrSize);
        if(newSockID == SL_EAGAIN)
        {
            /* Wait for 1 ms */
            HAL_Delay(1);
        }
        else if(newSockID < 0)
        {
            LOG_ERR("");
            wifi_dev_sock_close(SockID);
            return(mp_const_none);
        }
    }

    printf(" TCP client connected\r\n");

    while(LoopCount < NO_OF_PACKETS)
    {
        Status = wifi_dev_sock_recv(newSockID, uBuf.BsdBuf, BUF_SIZE, 0);
        if(Status <= 0)
        {
            LOG_ERR("");
            wifi_dev_sock_close(newSockID);
            wifi_dev_sock_close(SockID);
            return(mp_const_none);
        }

        LoopCount++;
    }

    wifi_dev_sock_close(newSockID);
    wifi_dev_sock_close(SockID);

    return(mp_const_none);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(sock_tcpserver_obj, sock_tcpserver);

// Table of class members
STATIC const mp_map_elem_t locals_dict_table[] =
{
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),     (mp_obj_t)&sock_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected), (mp_obj_t)&sock_isconnected_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_isavailable), (mp_obj_t)&sock_isavailable_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_send),        (mp_obj_t)&sock_send_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),       (mp_obj_t)&sock_write_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_recv),        (mp_obj_t)&sock_recv_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),        (mp_obj_t)&sock_read_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_setsockopt),  (mp_obj_t)&sock_setsockopt_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_bind),        (mp_obj_t)&sock_bind_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_listen),      (mp_obj_t)&sock_listen_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_accept),      (mp_obj_t)&sock_accept_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_close),       (mp_obj_t)&sock_close_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_tcpserver),   (mp_obj_t)&sock_tcpserver_obj},


#if 0
    { MP_OBJ_NEW_QSTR(MP_QSTR_fileno),      (mp_obj_t)&socket_fileno_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_makefile),    (mp_obj_t)&mp_identity_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),        (mp_obj_t)&mp_stream_read_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_readall),     (mp_obj_t)&mp_stream_readall_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_readline),    (mp_obj_t)&mp_stream_unbuffered_readline_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),       (mp_obj_t)&mp_stream_write_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_setblocking), (mp_obj_t)&socket_setblocking_obj},
#endif
};
STATIC MP_DEFINE_CONST_DICT(mod_locals_dict, locals_dict_table);

STATIC const mp_obj_type_t mod_type =
{
    { &mp_type_type },
    .name           = MP_QSTR_sock,
    .print          = sock_print,
    .make_new       = sock_make_new,
    .getiter        = NULL,
    .iternext       = NULL,
    .stream_p       = NULL,
    .locals_dict    = (mp_obj_t)&mod_locals_dict,
};

// Table of global class members
STATIC const mp_map_elem_t module_globals_table[] =
{
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),        MP_OBJ_NEW_QSTR(MP_QSTR_sock) },
    // Class object
    { MP_OBJ_NEW_QSTR(MP_QSTR_sock),            (mp_obj_t)&mod_type},
    // Global functions
    { MP_OBJ_NEW_QSTR(MP_QSTR_gethostbyname),   (mp_obj_t)&sock_gethostbyname_obj},

#if 0
    { MP_OBJ_NEW_QSTR(MP_QSTR_htons),           (mp_obj_t)&mod_socket_htons_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_inet_aton),       (mp_obj_t)&mod_socket_inet_aton_obj},
#endif

#define C(name) { MP_OBJ_NEW_QSTR(MP_QSTR_ ## name), MP_OBJ_NEW_SMALL_INT(name) }
    C(AF_UNIX),
    C(AF_INET),
    C(AF_INET6),
    C(SOCK_STREAM),
    C(SOCK_DGRAM),
    C(SOCK_RAW),

    C(MSG_DONTROUTE),
    C(MSG_DONTWAIT),

    // For setsockopt
    C(SOL_SOCKET),
    C(SO_BROADCAST),
    C(SO_ERROR),
    C(SO_KEEPALIVE),
    C(SO_LINGER),
    C(SO_REUSEADDR),
    // For setsockopt (custom for CC3000)
    C(SOCKOPT_ACCEPT_NONBLOCK),
    C(SOCK_ON),
    C(SOCK_OFF),

    C(INADDR_ANY),
#undef C
};

STATIC const mp_obj_dict_t module_globals =
{
    .base = {&mp_type_dict},
    .map =
    {
        .all_keys_are_qstrs     = 1,
        .table_is_fixed_array   = 1,
        .used                   = MP_ARRAY_SIZE(module_globals_table),
        .alloc                  = MP_ARRAY_SIZE(module_globals_table),
        .table                  = (mp_map_elem_t *)module_globals_table,
    },
};

const mp_obj_module_t sock_module =
{
    .base       = { &mp_type_module },
    .name       = MP_QSTR_sock,
    .globals    = (mp_obj_dict_t*)&module_globals,
};


