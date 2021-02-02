import ast
keywords = ["CHAR_BIT", "CHAR_MAX", "CHAR_MIN", "DBL_DIG", "DBL_EPSILON", "DBL_HAS_SUBNORM", "DBL_MANT_DIG", "DBL_MAX", 
    "DBL_MAX_10_EXP", "DBL_MAX_EXP", "DBL_MIN", "DBL_MIN_EXP", "DBL_TRUE_MIN", "DECIMAL_DIG", "EXIT_FAILURE", "EXIT_SUCCESS", 
    "FLT_DECIMAL_DIG", "FLT_DIG", "FLT_EVAL_METHOD", "FLT_HAS_SUBNORM", "FLT_MANT_DIG", "FLT_MAX", "FLT_MAX_10_EXP", "FLT_MAX_EXP", 
    "FLT_MIN", "FLT_MIN_10_EXP", "FLT_MIN_EXP", "FLT_RADIX", "FLT_ROUNDS", "FLT_TRUE_MIN", "INT16_MAX", "INT16_MIN", 
    "INT32_MAX", "INT32_MIN", "INT64_MAX", "INT64_MIN", "INT8_MAX", "INT8_MIN", "INTMAX_MAX", "INTMAX_MIN", 
    "INTPTR_MAX", "INTPTR_MIN", "INT_FAST16_MAX", "INT_FAST16_MIN", "INT_FAST32_MAX", "INT_FAST32_MIN", "INT_FAST64_MAX", "INT_FAST64_MIN", 
    "INT_FAST8_MAX", "INT_FAST8_MIN", "INT_LEAST16_MAX", "INT_LEAST16_MIN", "INT_LEAST32_MAX", "INT_LEAST32_MIN", "INT_LEAST64_MAX", "INT_LEAST64_MIN", 
    "INT_LEAST8_MAX", "INT_LEAST8_MIN", "INT_MAX", "INT_MIN", "LDBL_DECIMAL_DIG", "LDBL_DIG", "LDBL_EPSILON", "LDBL_MANT_DIG", 
    "LDBL_MAX", "LDBL_MAX_10_EXP", "LDBL_MAX_EXP", "LDBL_MIN", "LDBL_MIN_10_EXP", "LDBL_MIN_EXP", "LDBL_TRUE_MIN", "LLONG_MAX", 
    "LLONG_MIN", "LONG_MAX", "LONG_MIN", "MB_LEN_MAX", "NULL", "SCHAR_MAX", "SCHAR_MIN", "SHRT_MAX", 
    "SHRT_MIN", "UCHAR_MAX", "UINT16_MAX", "UINT32_MAX", "UINT64_MAX", "UINT8_MAX", "UINTMAX_MAX", "UINTPTR_MAX", 
    "UINT_FAST16_MAX", "UINT_FAST32_MAX", "UINT_FAST64_MAX", "UINT_FAST8_MAX", "UINT_LEAST16_MAX", "UINT_LEAST32_MAX", "UINT_LEAST64_MAX", "UINT_LEAST8_MAX", 
    "UINT_MAX", "ULLONG_MAX", "ULONG_MAX", "USHRT_MAX", "_Alignas", "_Alignof", "_Atomic", "_Bool", 
    "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local", "auto", "bool", 
    "break", "case", "char", "char16_t", "char32_t", "char8_t", "const", "continue", 
    "default", "do", "double", "else", "enum", "extern", "false", "float", 
    "for", "goto", "if", "inline", "int", "int16_t", "int32_t", "int64_t", 
    "int8_t", "long", "register", "restrict", "return", "short", "signed", "sizeof", 
    "static", "struct", "switch", "true", "typedef", "uint16_t", "uint32_t", "uint64_t", 
    "uint8_t", "union", "unsigned", "void", "volatile", "wchar_t", "while", "wint_t",
    "size_t", "rsize_t", "uintptr_t", "intptr_t", "intmax_t", "uintmax_t",
    "PTRDIFF_MIN", "PTRDIFF_MAX", 'SIG_ATOMIC_MIN', 'SIG_ATOMIC_MAX',
    'SIZE_MAX', 'WCHAR_MIN', 'WCHAR_MAX', 'WINT_MIN', 'WINT_MAX',
    'FILE', 'fpos_t', '_IOFBF', '_IOLBF', '_IONBF', 'BUFSIZ',
    'EOF', 'FOPEN_MAX', 'FILENAME_MAX', 'L_tmpnam',
    'SEEK_CUR', 'SEEK_END', 'SEEK_SET',
    'TMP_MAX', 'stderr', 'stdin', 'stdout',
    'mbstate_t', 'TIME_UTC', 'static_assert', 'noreturn', 'alignof', 'alignas', 'complex',
    'TMP_MAX_S',
    'SIG_DFL', 'SIG_ERR', 'SIG_IGN',
    "SIGABRT", "SIGALRM", "SIGBUS", "SIGCHLD", "SIGCLD", "SIGCONT",
    "SIGEMT", "SIGFPE", "SIGHUP", "SIGILL", "SIGINFO", "SIGINT",
    "SIGIO", "SIGIOT", "SIGKILL", "SIGLOST", "SIGPIPE",
    "SIGPOLL", "SIGPROF", "SIGPWR", "SIGQUIT", "SIGSEGV", "SIGSTKFLT",
    "SIGSTOP", "SIGTSTP", "SIGSYS", "SIGTERM", "SIGTRAP", "SIGTTIN",
    "SIGTTOU", "SIGUNUSED", "SIGURG", "SIGUSR1", "SIGUSR2", "SIGVTALRM",
    "SIGXCPU", "SIGXFSZ", "SIGWINCH", 'imaxdiv_t',
    'NDEBUG', 'I',
    'EPERM', 'ENOENT', 'ESRCH', 'EINTR', 'EIO', 'ENXIO',
    'E2BIG', 'ENOEXEC', 'EBADF', 'ECHILD', 'EAGAIN', 'ENOMEM', 'EACCES',
    'EFAULT', 'ENOTBLK', 'EBUSY', 'EEXIST', 'EXDEV', 'ENODEV', 'ENOTDIR',
    'EISDIR', 'EINVAL', 'ENFILE', 'EMFILE', 'ENOTTY', 'ETXTBSY', 'EFBIG',
    'ENOSPC', 'ESPIPE', 'EROFS', 'EMLINK', 'EPIPE', 'EDOM', 'ERANGE', 'EDEADLK',
    'ENAMETOOLONG', 'ENOLCK', 'ENOSYS', 'ENOTEMPTY', 'ELOOP', 'EWOULDBLOCK',
    'ENOMSG', 'EIDRM', 'ECHRNG', 'EL2NSYNC', 'EL3HLT', 'EL3RST', 'ELNRNG',
    'EUNATCH', 'ENOCSI', 'EL2HLT', 'EBADE', 'EBADR', 'EXFULL', 'ENOANO',
    'EBADRQC', 'EBADSLT', 'EDEADLOCK', 'EBFONT', 'ENOSTR', 'ENODATA',
    'ETIME', 'ENOSR', 'ENONET', 'ENOPKG', 'EREMOTE', 'ENOLINK', 'EADV',
    'ESRMNT', 'ECOMM', 'EPROTO', 'EMULTIHOP', 'EDOTDOT', 'EBADMSG',
    'EOVERFLOW', 'ENOTUNIQ', 'EBADFD', 'EREMCHG', 'ELIBACC', 'ELIBBAD',
    'ELIBSCN', 'ELIBMAX', 'ELIBEXEC', 'EILSEQ', 'ERESTART', 'ESTRPIPE',
    'EUSERS', 'ENOTSOCK', 'EDESTADDRREQ', 'EMSGSIZE', 'EPROTOTYPE',
    'ENOPROTOOPT', 'EPROTONOSUPPORT', 'ESOCKTNOSUPPORT', 'EOPNOTSUPP',
    'EPFNOSUPPORT', 'EAFNOSUPPORT', 'EADDRINUSE', 'EADDRNOTAVAIL', 'ENETDOWN',
    'ENETUNREACH', 'ENETRESET', 'ECONNABORTED', 'ECONNRESET', 'ENOBUFS',
    'EISCONN', 'ENOTCONN', 'ESHUTDOWN', 'ETOOMANYREFS', 'ETIMEDOUT',
    'ECONNREFUSED', 'EHOSTDOWN', 'EHOSTUNREACH', 'EALREADY', 'EINPROGRESS',
    'ESTALE', 'EUCLEAN', 'ENOTNAM',
    'ENAVAIL', 'EISNAM', 'EREMOTEIO', 'EDQUOT', 'ENOMEDIUM', 'EMEDIUMTYPE',
    'errno_t', 'fenv_t', 'fexcept_t', 'FE_DIVBYZERO', 'FE_INEXACT',
    'FE_INVALID', 'FE_OVERFLOW', 'FE_UNDERFLOW', 'FE_ALL_EXCEPT', 'FE_DOWNWARD',
    'FE_TONEAREST', 'FE_TOWARDZERO', 'FE_UPWARD', 'FE_DFL_ENV',
    'LC_ALL', 'LC_CTYPE', 'LC_NUMERIC', 'LC_COLLATE', 'LC_MONETARY', 'LC_TIME',
    'float_t', 'double_t', 'HUGE_VAL', 'HUGE_VALF', 'HUGE_VALL', 'INFINITY',
    'NAN', 'FP_INFINITE', 'FP_NAN', 'FP_NORMAL', 'FP_SUBNORMAL', 'FP_ZERO',
    'FP_FAST_FMA', 'FP_FAST_FMAF', 'FP_FAST_FMAL', 'FP_ILOGB0', 'FP_ILOGBNAN',
    'MATH_ERRNO', 'MATH_ERREXCEPT', 'math_errhandling', 'jmp_buf', 'va_list',
    'sig_atomic_t', 'memory_order', 'memory_order_relaxed', 'memory_order_consume',
    'memory_order_acquire', 'memory_order_release', 'memory_order_acq_rel', 'memory_order_seq_cst',
    'ptrdiff_t', 'max_align_t', 'offsetof', 'RSIZE_MAX', 'MB_CUR_MAX', 'div_t', 'ldiv_t', 'lldiv_t',
    'constraint_handler_t', 'ONCE_FLAG_INIT', 'TSS_DTOR_ITERATIONS', 'cnd_t', 'thrd_t', 'tss_t', 'mtx_t',
    'tss_dtor_t', 'thrd_start_t', 'once_flag', 'xtime', 'mtx_plain', 'mtx_recursive', 'mtx_timed',
    'mtx_try', 'thrd_timeout', 'thrd_success', 'thrd_busy', 'thrd_error', 'thrd_nomem',
    'CLOCKS_PER_SEC', 'clock_t', 'time_t', 'WEOF', 'wctrans_t', 'wctype_t',
]

for x in ['char', 'char16_t', 'char32_t', 'wchar_t', 'short', 'int',
    'long', 'llong', 'address', 'flag']:
    keywords.append('ATOMIC_{}_LOCK_FREE'.format(x.upper()))
for x in ['flag', 'bool', 'address', 'char', 'schar', 'uchar', 'short',
    'ushort', 'int', 'uint', 'long', 'ulong', 'llong', 'ullong', 'char16_t',
    'char32_t', 'wchar_t',
    'int_least8_t', 'int_least16_t', 'int_least32_t', 'int_least64_t',
    'uint_least8_t', 'uint_least16_t', 'uint_least32_t', 'uint_least64_t',
    'int_fast8_t', 'int_fast16_t', 'int_fast32_t', 'int_fast64_t',
    'uint_fast8_t', 'uint_fast16_t', 'uint_fast32_t', 'uint_fast64_t',
    'intptr_t', 'uintptr_t', 'size_t', 'ptrdiff_t', 'intmax_t', 'uintmax_t']:
    keywords.append('atomic_{}'.format(x))

for c in 'diouxX':
    for thing in ['', 'LEAST', 'FAST']:
        for N in [8, 16, 32, 64]:
            keywords.append('PRI{}{}{}'.format(c, thing, N))
            if c != 'X': keywords.append('SCN{}{}{}'.format(c, thing, N))
            
    keywords.append('PRI{}PTR'.format(c))
    keywords.append('PRI{}MAX'.format(c))
    if c != 'X':
        keywords.append('SCN{}PTR'.format(c))
        keywords.append('SCN{}MAX'.format(c))

assert len(set(keywords)) == len(keywords)

keywords_by_c = {}
for kwd in keywords:
    c = kwd[0]
    if c in keywords_by_c:
        keywords_by_c[c].append(kwd)
    else:
        keywords_by_c[c] = [kwd]

for (c, kwds) in sorted(keywords_by_c.items()):
    kwds = list(sorted(kwds))
    print('static char const *const keywords_{}[{}] = {{'.format(c, len(kwds)+1), end='')
    print(','.join(map(lambda x: '"'+x+'"', kwds)), end='};\n')
print('static char const *const *const all_keywords[] = {')
print('\t'+', '.join(["['{}'] = keywords_{}".format(c, c) for c in sorted(keywords_by_c.keys())]))
print('};')