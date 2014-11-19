
#ifndef MODSOCKET_H
#define MODSOCKET_H

#ifndef __arm__ // unix

//#define  SOCKOPT_ACCEPT_NONBLOCK        2   // accept non block mode, set SOCK_ON or SOCK_OFF (default block mode)
//#define  SOCK_ON                0           // socket non-blocking mode is enabled
//#define  SOCK_OFF               1           // socket blocking mode is enabled

#else
/*
 * Address families.
 */
#define AF_UNSPEC   0       /* unspecified */
#define AF_UNIX     1       /* local to host (pipes, portals) */
//#define AF_INET     2       /* internetwork: UDP, TCP, etc. */
#define AF_IMPLINK  3       /* arpanet imp addresses */
#define AF_PUP      4       /* pup protocols: e.g. BSP */
#define AF_CHAOS    5       /* mit CHAOS protocols */
#define AF_NS       6       /* XEROX NS protocols */
#define AF_ISO      7       /* ISO protocols */
#define AF_OSI      AF_ISO
#define AF_ECMA     8       /* european computer manufacturers */
#define AF_DATAKIT  9       /* datakit protocols */
#define AF_CCITT    10      /* CCITT protocols, X.25 etc */
#define AF_SNA      11      /* IBM SNA */
#define AF_DECnet   12      /* DECnet */
#define AF_DLI      13      /* DEC Direct data link interface */
#define AF_LAT      14      /* LAT */
#define AF_HYLINK   15      /* NSC Hyperchannel */
#define AF_APPLETALK    16      /* Apple Talk */
#define AF_ROUTE    17      /* Internal Routing Protocol */
#define AF_LINK     18      /* Link layer interface */
#define pseudo_AF_XTP   19      /* eXpress Transfer Protocol (no AF) */

#define AF_MAX      20

/*
 * Option flags per-socket.
 */
#define SO_DEBUG    0x0001      /* turn on debugging info recording */
#define SO_ACCEPTCONN   0x0002      /* socket has had listen() */
#define SO_REUSEADDR    0x0004      /* allow local address reuse */
//#define SO_KEEPALIVE    0x0008      /* keep connections alive */
#define SO_DONTROUTE    0x0010      /* just use interface addresses */
#define SO_BROADCAST    0x0020      /* permit sending of broadcast msgs */
#define SO_USELOOPBACK  0x0040      /* bypass hardware when possible */
#define SO_LINGER   0x0080      /* linger on close if data present */
#define SO_OOBINLINE    0x0100      /* leave received OOB data in line */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF   0x1001      /* send buffer size */
#define SO_RCVBUF   0x1002      /* receive buffer size */
#define SO_SNDLOWAT 0x1003      /* send low-water mark */
#define SO_RCVLOWAT 0x1004      /* receive low-water mark */
#define SO_SNDTIMEO 0x1005      /* send timeout */
//#define SO_RCVTIMEO 0x1006      /* receive timeout */
#define SO_ERROR    0x1007      /* get error status and clear */
#define SO_TYPE     0x1008      /* get socket type */


#define MSG_OOB     0x1     /* process out-of-band data */
#define MSG_PEEK    0x2     /* peek at incoming message */
#define MSG_DONTROUTE   0x4     /* send without using routing tables */
#define MSG_EOR     0x8     /* data completes record */
#define MSG_TRUNC   0x10        /* data discarded before delivery */
#define MSG_CTRUNC  0x20        /* control data lost before delivery */
#define MSG_WAITALL 0x40        /* wait for full request or error */
// REF: http://www.opensource.apple.com/source/xnu/xnu-1456.1.26/bsd/sys/socket.h
//#define MSG_DONTWAIT    0x80        /* this message should be nonblocking */

// http://lxr.free-electrons.com/source/include/linux/inet.h?v=2.6.30#L51
#define INET_ADDRSTRLEN         (16)

#define MSG_NOSIGNAL 0x4000 /* don't raise SIGPIPE */ // IGNORED ANYWAY!

/*
 * Protocols (RFC 1700)
 */
//#define IPPROTO_IP              0               /* dummy for IP */
//#define IPPROTO_UDP             17              /* user datagram protocol */
//#define IPPROTO_TCP             6               /* tcp */

//#define INADDR_ANY              (0x00000000)
#define INADDR_BROADCAST        (0xffffffff) /* must be masked */

/*struct addrinfo
{
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};*/

#endif

#endif // MODSOCKET_H
