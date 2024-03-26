#ifndef _QX_ERRNO_H_
#define _QX_ERRNO_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "string.h"

typedef int QX_ERR_T;

#define QX_SUCCESS              0
#define QX_EPERM                EPERM           /* Operation not permitted */
#define QX_ENOENT               ENOENT          /* No such file or directory */
#define QX_ESRCH                ESRCH           /* No such process */
#define QX_EINTR                EINTR           /* Interrupted system call */
#define QX_EIO                  EIO             /* I/O error */
#define QX_ENXIO                ENXIO           /* No such device or address */
#define QX_E2BIG                E2BIG           /* Argument list too long */
#define QX_ENOEXEC              ENOEXEC         /* Exec format error */
#define QX_EBADF                EBADF           /* Bad file number */
#define QX_ECHILD               ECHILD          /* No child processes */
#define QX_EAGAIN               EAGAIN          /* Try again */
#define QX_ENOMEM               ENOMEM          /* Out of memory */
#define QX_EACCES               EACCES          /* Permission denied */
#define QX_EFAULT               EFAULT          /* Bad address */
#define QX_ENOTBLK              ENOTBLK         /* Block device required */
#define QX_EBUSY                EBUSY           /* Device or resource busy */
#define QX_EEXIST               EEXIST          /* File exists */
#define QX_EXDEV                EXDEV           /* Cross-device link */
#define QX_ENODEV               ENODEV          /* No such device */
#define QX_ENOTDIR              ENOTDIR         /* Not a directory */
#define QX_EISDIR               EISDIR          /* Is a directory */
#define QX_EINVAL               EINVAL          /* Invalid argument */
#define QX_ENFILE               ENFILE          /* File table overflow */
#define QX_EMFILE               EMFILE          /* Too many open files */
#define QX_ENOTTY               ENOTTY          /* Not a typewriter */
#define QX_ETXTBSY              ETXTBSY         /* Text file busy */
#define QX_EFBIG                EFBIG           /* File too large */
#define QX_ENOSPC               ENOSPC          /* No space left on device */
#define QX_ESPIPE               ESPIPE          /* Illegal seek */
#define QX_EROFS                EROFS           /* Read-only file system */
#define QX_EMLINK               EMLINK          /* Too many links */
#define QX_EPIPE                EPIPE           /* Broken pipe */
#define QX_EDOM                 EDOM            /* Math argument out of domain of func */
#define QX_ERANGE               ERANGE          /* Math result not representable */
#define QX_EDEADLK              EDEADLK         /* Resource deadlock would occur */
#define QX_ENAMETOOLONG         ENAMETOOLONG    /* File name too long */
#define QX_ENOLCK               ENOLCK          /* No record locks available */
#define QX_ENOSYS               ENOSYS          /* Function not implemented */
#define QX_ENOTEMPTY            ENOTEMPTY       /* Directory not empty */
#define QX_ELOOP                ELOOP           /* Too many symbolic links encountered */
#define QX_EWOULDBLOCK          EWOULDBLOCK     /* Operation would block */
#define QX_ENOMSG               ENOMSG          /* No message of desired type */
#define QX_EIDRM                EIDRM           /* Identifier removed */
#define QX_ECHRNG               ECHRNG          /* Channel number out of range */
#define QX_EL2NSYNC             EL2NSYNC        /* Level 2 not synchronized */
#define QX_EL3HLT               EL3HLT          /* Level 3 halted */
#define QX_EL3RST               EL3RST          /* Level 3 reset */
#define QX_ELNRNG               ELNRNG          /* Link number out of range */
#define QX_EUNATCH              EUNATCH         /* Protocol driver not attached */
#define QX_ENOCSI               ENOCSI          /* No CSI structure available */
#define QX_EL2HLT               EL2HLT          /* Level 2 halted */
#define QX_EBADE                EBADE           /* Invalid exchange */
#define QX_EBADR                EBADR           /* Invalid request descriptor */
#define QX_EXFULL               EXFULL          /* Exchange full */
#define QX_ENOANO               ENOANO          /* No anode */
#define QX_EBADRQC              EBADRQC         /* Invalid request code */
#define QX_EBADSLT              EBADSLT         /* Invalid slot */
#define QX_EDEADLOCK            EDEADLOCK
#define QX_EBFONT               EBFONT          /* Bad font file format */
#define QX_ENOSTR               ENOSTR          /* Device not a stream */
#define QX_ENODATA              ENODATA         /* No data available */
#define QX_ETIME                ETIME           /* Timer expired */
#define QX_ENOSR                ENOSR           /* Out of streams resources */
#define QX_ENONET               ENONET          /* Machine is not on the network */
#define QX_ENOPKG               ENOPKG          /* Package not installed */
#define QX_EREMOTE              EREMOTE         /* Object is remote */
#define QX_ENOLINK              ENOLINK         /* Link has been severed */
#define QX_EADV                 EADV            /* Advertise error */
#define QX_ESRMNT               ESRMNT          /* Srmount error */
#define QX_ECOMM                ECOMM           /* Communication error on send */
#define QX_EPROTO               EPROTO          /* Protocol error */
#define QX_EMULTIHOP            EMULTIHOP       /* Multihop attempted */
#define QX_EDOTDOT              EDOTDOT         /* RFS specific error */
#define QX_EBADMSG              EBADMSG         /* Not a data message */
#define QX_EOVERFLOW            EOVERFLOW       /* Value too large for defined data type */
#define QX_ENOTUNIQ             ENOTUNIQ        /* Name not unique on network */
#define QX_EBADFD               EBADFD          /* File descriptor in bad state */
#define QX_EREMCHG              EREMCHG         /* Remote address changed */
#define QX_ELIBACC              ELIBACC         /* Can not access a needed shared library */
#define QX_ELIBBAD              ELIBBAD         /* Accessing a corrupted shared library */
#define QX_ELIBSCN              ELIBSCN         /* .lib section in a.out corrupted */
#define QX_ELIBMAX              ELIBMAX         /* Attempting to link in too many shared libraries */
#define QX_ELIBEXEC             ELIBEXEC        /* Cannot exec a shared library directly */
#define QX_EILSEQ               EILSEQ          /* Illegal byte sequence */
#define QX_ERESTART             ERESTART        /* Interrupted system call should be restarted */
#define QX_ESTRPIPE             ESTRPIPE        /* Streams pipe error */
#define QX_EUSERS               EUSERS          /* Too many users */
#define QX_ENOTSOCK             ENOTSOCK        /* Socket operation on non-socket */
#define QX_EDESTADDRREQ         EDESTADDRREQ    /* Destination address required */
#define QX_EMSGSIZE             EMSGSIZE        /* Message too long */
#define QX_EPROTOTYPE           EPROTOTYPE      /* Protocol wrong type for socket */
#define QX_ENOPROTOOPT          ENOPROTOOPT     /* Protocol not available */
#define QX_EPROTONOSUPPORT      EPROTONOSUPPORT /* Protocol not supported */
#define QX_ESOCKTNOSUPPORT      ESOCKTNOSUPPORT /* Socket type not supported */
#define QX_EOPNOTSUPP           EOPNOTSUPP      /* Operation not supported on transport endpoint */
#define QX_EPFNOSUPPORT         EPFNOSUPPORT    /* Protocol family not supported */
#define QX_EAFNOSUPPORT         EAFNOSUPPORT    /* Address family not supported by protocol */
#define QX_EADDRINUSE           EADDRINUSE      /* Address already in use */
#define QX_EADDRNOTAVAIL        EADDRNOTAVAIL   /* Cannot assign requested address */
#define QX_ENETDOWN             ENETDOWN        /* Network is down */
#define QX_ENETUNREACH          ENETUNREACH     /* Network is unreachable */
#define QX_ENETRESET            ENETRESET       /* Network dropped connection because of reset */
#define QX_ECONNABORTED         ECONNABORTED    /* Software caused connection abort */
#define QX_ECONNRESET           ECONNRESET      /* Connection reset by peer */
#define QX_ENOBUFS              ENOBUFS         /* No buffer space available */
#define QX_EISCONN              EISCONN         /* Transport endpoint is already connected */
#define QX_ENOTCONN             ENOTCONN        /* Transport endpoint is not connected */
#define QX_ESHUTDOWN            ESHUTDOWN       /* Cannot send after transport endpoint shutdown */
#define QX_ETOOMANYREFS         ETOOMANYREFS    /* Too many references: cannot splice */
#define QX_ETIMEDOUT            ETIMEDOUT       /* Connection timed out */
#define QX_ECONNREFUSED         ECONNREFUSED    /* Connection refused */
#define QX_EHOSTDOWN            EHOSTDOWN       /* Host is down */
#define QX_EHOSTUNREACH         EHOSTUNREACH    /* No route to host */
#define QX_EALREADY             EALREADY        /* Operation already in progress */
#define QX_EINPROGRESS          EINPROGRESS     /* Operation now in progress */
#define QX_ESTALE               ESTALE          /* Stale file handle */
#define QX_EUCLEAN              EUCLEAN         /* Structure needs cleaning */
#define QX_ENOTNAM              ENOTNAM         /* Not a XENIX named type file */
#define QX_ENAVAIL              ENAVAIL         /* No XENIX semaphores available */
#define QX_EISNAM               EISNAM          /* Is a named type file */
#define QX_EREMOTEIO            EREMOTEIO       /* Remote I/O error */
#define QX_EDQUOT               EDQUOT          /* Quota exceeded */
#define QX_ENOMEDIUM            ENOMEDIUM       /* No medium found */
#define QX_EMEDIUMTYPE          EMEDIUMTYPE     /* Wrong medium type */
#define QX_ECANCELED            ECANCELED       /* Operation Canceled */
#define QX_ENOKEY               ENOKEY          /* Required key not available */
#define QX_EKEYEXPIRED          EKEYEXPIRED     /* Key has expired */
#define QX_EKEYREVOKED          EKEYREVOKED     /* Key has been revoked */
#define QX_EKEYREJECTED         EKEYREJECTED    /* Key was rejected by service */
/* for  mutexes */
#define QX_EOWNERDEAD           EOWNERDEAD      /* Owner died */
#define QX_ENOTRECOVERABLE      ENOTRECOVERABLE /* State not recoverable */
#define QX_ERFKILL              ERFKILL         /* Operation not possible due to RF-kill */
#define QX_EHWPOISON            EHWPOISON       /* Memory page has hardware error */

typedef enum _QX_ERR_NO_ENUM
{
    QX_ERR_NO_START             = 255,
    QX_ERR_EXIT_WITH_SUCCESS    = 256,
    QX_ERR_PEER_CLOSED          = 257,
    QX_ERR_MEM_LEAK             = 258,
    
    QX_ERR_NO_ENUM_MAX 
}
QX_ERR_NO_ENUM;

static const char* sg_QXTestErrnoStr[QX_ERR_NO_ENUM_MAX - QX_ERR_NO_START] = 
{
    [QX_ERR_NO_START - QX_ERR_NO_START]             = "unused error: errno start",
    [QX_ERR_EXIT_WITH_SUCCESS - QX_ERR_NO_START]    = "exit with success: not an error",
    [QX_ERR_PEER_CLOSED - QX_ERR_NO_START]          = "peer closed",
    [QX_ERR_MEM_LEAK - QX_ERR_NO_START]             = "mem leak"
};

static inline const char*
QX_StrErr(
    int Errno
    )
{
    int errno;
    errno = Errno > 0 ? Errno : -Errno;
    
    return errno < QX_ERR_NO_START ? strerror(errno) : 
            (errno < QX_ERR_NO_ENUM_MAX ? sg_QXTestErrnoStr[errno - QX_ERR_NO_START] : "UnknownErr");
}

#ifdef __cplusplus
 }
#endif

#endif /* _MY_ERRNO_H_ */
