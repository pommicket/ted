#!/usr/bin/python3
import ast

types = [
	'SYNTAX_KEYWORD', 'SYNTAX_CONSTANT', 'SYNTAX_BUILTIN'
]
exec('\n'.join(['{} = {}'.format(type, i) for (i, type) in enumerate(types)]))

def process_keywords(keywords):
    assert len(set(keywords)) == len(keywords)
    keywords_by_c = {}
    for (type,kwd) in keywords:
        c = kwd[0]
        if c in keywords_by_c:
            keywords_by_c[c].append((type, kwd))
        else:
            keywords_by_c[c] = [(type, kwd)]
    return keywords_by_c
   
def output_keywords(file, keywords, language):
    keywords = process_keywords(keywords)
    for (c, kwds) in sorted(keywords.items()):
        kwds = list(sorted(kwds))
        file.write('static Keyword const syntax_keywords_{}_{}[{}] = {{'.format(language, c, len(kwds)+1))
        file.write(','.join(map(lambda kwd: '{"'+kwd[1]+'", ' + types[kwd[0]] + '}', kwds)) + '};\n')
    file.write('static Keyword const *const syntax_all_keywords_{}[] = {{\n'.format(language))
    file.write('\t'+', '.join(["['{}'] = syntax_keywords_{}_{}".format(c, language, c) for c in sorted(keywords.keys())]) + '\n')
    file.write('};\n\n')

constants_c = ['CHAR_BIT', 'CHAR_MAX', 'CHAR_MIN', 'DBL_DIG', 'DBL_EPSILON', 'DBL_HAS_SUBNORM', 'DBL_MANT_DIG', 'DBL_MAX', 
    'DBL_MAX_10_EXP', 'DBL_MAX_EXP', 'DBL_MIN', 'DBL_MIN_EXP', 'DBL_TRUE_MIN', 'DECIMAL_DIG', 'EXIT_FAILURE', 'EXIT_SUCCESS', 
    'FLT_DECIMAL_DIG', 'FLT_DIG', 'FLT_EVAL_METHOD', 'FLT_HAS_SUBNORM', 'FLT_MANT_DIG', 'FLT_MAX', 'FLT_MAX_10_EXP', 'FLT_MAX_EXP', 
    'FLT_MIN', 'FLT_MIN_10_EXP', 'FLT_MIN_EXP', 'FLT_RADIX', 'FLT_ROUNDS', 'FLT_TRUE_MIN', 'INT16_MAX', 'INT16_MIN', 
    'INT32_MAX', 'INT32_MIN', 'INT64_MAX', 'INT64_MIN', 'INT8_MAX', 'INT8_MIN', 'INTMAX_MAX', 'INTMAX_MIN', 
    'INTPTR_MAX', 'INTPTR_MIN', 'INT_FAST16_MAX', 'INT_FAST16_MIN', 'INT_FAST32_MAX', 'INT_FAST32_MIN', 'INT_FAST64_MAX', 'INT_FAST64_MIN', 
    'INT_FAST8_MAX', 'INT_FAST8_MIN', 'INT_LEAST16_MAX', 'INT_LEAST16_MIN', 'INT_LEAST32_MAX', 'INT_LEAST32_MIN', 'INT_LEAST64_MAX', 'INT_LEAST64_MIN', 
    'INT_LEAST8_MAX', 'INT_LEAST8_MIN', 'INT_MAX', 'INT_MIN', 'LDBL_DECIMAL_DIG', 'LDBL_DIG', 'LDBL_EPSILON', 'LDBL_MANT_DIG', 
    'LDBL_MAX', 'LDBL_MAX_10_EXP', 'LDBL_MAX_EXP', 'LDBL_MIN', 'LDBL_MIN_10_EXP', 'LDBL_MIN_EXP', 'LDBL_TRUE_MIN', 'LLONG_MAX', 
    'LLONG_MIN', 'LONG_MAX', 'LONG_MIN', 'MB_LEN_MAX', 'NULL', 'SCHAR_MAX', 'SCHAR_MIN', 'SHRT_MAX', 
    'SHRT_MIN', 'UCHAR_MAX', 'UINT16_MAX', 'UINT32_MAX', 'UINT64_MAX', 'UINT8_MAX', 'UINTMAX_MAX', 'UINTPTR_MAX', 
    'UINT_FAST16_MAX', 'UINT_FAST32_MAX', 'UINT_FAST64_MAX', 'UINT_FAST8_MAX', 'UINT_LEAST16_MAX', 'UINT_LEAST32_MAX', 'UINT_LEAST64_MAX', 'UINT_LEAST8_MAX', 
    'UINT_MAX', 'ULLONG_MAX', 'ULONG_MAX', 'USHRT_MAX', 'true', 'false',
    'PTRDIFF_MIN', 'PTRDIFF_MAX', 'SIG_ATOMIC_MIN', 'SIG_ATOMIC_MAX',
    'SIZE_MAX', 'WCHAR_MIN', 'WCHAR_MAX', 'WINT_MIN', 'WINT_MAX',
    'FILE', 'fpos_t', '_IOFBF', '_IOLBF', '_IONBF', 'BUFSIZ',
    'EOF', 'FOPEN_MAX', 'FILENAME_MAX', 'L_tmpnam',
    'SEEK_CUR', 'SEEK_END', 'SEEK_SET', 'TMP_MAX', 'stderr', 'stdin', 'stdout', 'TIME_UTC', 
    'TMP_MAX_S', 'SIG_DFL', 'SIG_ERR', 'SIG_IGN',
    'SIGABRT', 'SIGALRM', 'SIGBUS', 'SIGCHLD', 'SIGCLD', 'SIGCONT',
    'SIGEMT', 'SIGFPE', 'SIGHUP', 'SIGILL', 'SIGINFO', 'SIGINT',
    'SIGIO', 'SIGIOT', 'SIGKILL', 'SIGLOST', 'SIGPIPE',
    'SIGPOLL', 'SIGPROF', 'SIGPWR', 'SIGQUIT', 'SIGSEGV', 'SIGSTKFLT',
    'SIGSTOP', 'SIGTSTP', 'SIGSYS', 'SIGTERM', 'SIGTRAP', 'SIGTTIN',
    'SIGTTOU', 'SIGUNUSED', 'SIGURG', 'SIGUSR1', 'SIGUSR2', 'SIGVTALRM',
    'SIGXCPU', 'SIGXFSZ', 'SIGWINCH', 'NDEBUG', 'I',
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
    'ESTALE', 'EUCLEAN', 'ENOTNAM', 'ENAVAIL', 'EISNAM', 'EREMOTEIO', 'EDQUOT', 'ENOMEDIUM', 'EMEDIUMTYPE',
	'FE_DIVBYZERO', 'FE_INEXACT', 'FE_INVALID', 'FE_OVERFLOW', 'FE_UNDERFLOW', 'FE_ALL_EXCEPT', 'FE_DOWNWARD',
    'FE_TONEAREST', 'FE_TOWARDZERO', 'FE_UPWARD', 'FE_DFL_ENV',
    'LC_ALL', 'LC_CTYPE', 'LC_NUMERIC', 'LC_COLLATE', 'LC_MONETARY', 'LC_TIME',
	'HUGE_VAL', 'HUGE_VALF', 'HUGE_VALL', 'INFINITY',
    'NAN', 'FP_INFINITE', 'FP_NAN', 'FP_NORMAL', 'FP_SUBNORMAL', 'FP_ZERO',
    'FP_FAST_FMA', 'FP_FAST_FMAF', 'FP_FAST_FMAL', 'FP_ILOGB0', 'FP_ILOGBNAN',
    'MATH_ERRNO', 'MATH_ERREXCEPT', 'RSIZE_MAX', 'MB_CUR_MAX', 'CLOCKS_PER_SEC', 'WEOF',
	'ONCE_FLAG_INIT', 'TSS_DTOR_ITERATIONS',
	'thrd_timeout', 'thrd_success', 'thrd_busy', 'thrd_error', 'thrd_nomem',
	'mtx_plain', 'mtx_recursive', 'mtx_timed', 'mtx_try',
]
keywords_c = ['_Alignas', '_Alignof', '_Atomic', '_Bool', 
    '_Complex', '_Generic', '_Imaginary', '_Noreturn', '_Static_assert', '_Thread_local', 'auto', 
    'break', 'case', 'char', 'const', 'continue',
    'default', 'do', 'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if', 'inline', 'int', 
	'long', 'register', 'restrict', 'return', 'short', 'signed', 'sizeof', 
    'static', 'struct', 'switch', 'typedef', 'union', 'unsigned', 'void', 'volatile', 'while',
]
builtins_c = [
	'bool',
	'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t', 
	'int_least8_t', 'int_least16_t', 'int_least32_t', 'int_least64_t', 'uint_least8_t', 'uint_least16_t', 'uint_least32_t', 'uint_least64_t', 
	'int_fast8_t', 'int_fast16_t', 'int_fast32_t', 'int_fast64_t', 'uint_fast8_t', 'uint_fast16_t', 'uint_fast32_t', 'uint_fast64_t', 
	'char8_t', 'char16_t', 'char32_t',
	'wchar_t', 'wint_t',
    'size_t', 'rsize_t', 'uintptr_t', 'intptr_t', 'intmax_t', 'uintmax_t',
    'mbstate_t', 'static_assert', 'noreturn', 'alignof', 'alignas', 'complex',
	'imaxdiv_t',
	'math_errhandling', 'jmp_buf', 'va_list',
    'errno_t', 'fenv_t', 'fexcept_t',
	'float_t', 'double_t',
    'sig_atomic_t', 'memory_order', 'memory_order_relaxed', 'memory_order_consume',
    'memory_order_acquire', 'memory_order_release', 'memory_order_acq_rel', 'memory_order_seq_cst',
	'ptrdiff_t', 'max_align_t', 'offsetof', 
    'div_t', 'ldiv_t', 'lldiv_t', 'constraint_handler_t', 'cnd_t', 'thrd_t', 'tss_t', 'mtx_t',
    'tss_dtor_t', 'thrd_start_t', 'once_flag', 'xtime',
    'clock_t', 'time_t', 'wctrans_t', 'wctype_t',
]
	
for x in ['char', 'char16_t', 'char32_t', 'wchar_t', 'short', 'int',
    'long', 'llong', 'address', 'flag']:
    constants_c.append('ATOMIC_{}_LOCK_FREE'.format(x.upper()))
for x in ['flag', 'bool', 'address', 'char', 'schar', 'uchar', 'short',
    'ushort', 'int', 'uint', 'long', 'ulong', 'llong', 'ullong', 'char16_t',
    'char32_t', 'wchar_t',
    'int_least8_t', 'int_least16_t', 'int_least32_t', 'int_least64_t',
    'uint_least8_t', 'uint_least16_t', 'uint_least32_t', 'uint_least64_t',
    'int_fast8_t', 'int_fast16_t', 'int_fast32_t', 'int_fast64_t',
    'uint_fast8_t', 'uint_fast16_t', 'uint_fast32_t', 'uint_fast64_t',
    'intptr_t', 'uintptr_t', 'size_t', 'ptrdiff_t', 'intmax_t', 'uintmax_t']:
    builtins_c.append('atomic_{}'.format(x))

for c in 'diouxX':
    for thing in ['', 'LEAST', 'FAST']:
        for N in [8, 16, 32, 64]:
            constants_c.append('PRI{}{}{}'.format(c, thing, N))
            if c != 'X': constants_c.append('SCN{}{}{}'.format(c, thing, N))
            
    constants_c.append('PRI{}PTR'.format(c))
    constants_c.append('PRI{}MAX'.format(c))
    if c != 'X':
        constants_c.append('SCN{}PTR'.format(c))
        constants_c.append('SCN{}MAX'.format(c))
  
# keywords unique to C++ (not in C)
keywords_cpp = [
    'and', 'and_eq', 'asm', 'atomic_cancel', 'atomic_commit', 
    'atomic_noexcept', 'bitand', 'bitor', 'catch', 'class', 'compl',
    'concept', 'consteval', 'constexpr', 'constinit', 'const_cast',
    'co_await', 'co_return', 'co_yield', 'decltype', 'delete', 'thread_local',
    'dynamic_cast', 'explicit', 'export', 'friend', 'mutable', 'namespace',
    'new', 'noexcept', 'not', 'not_eq', 'nullptr', 'operator', 'or', 'or_eq',
    'private', 'protected', 'public', 'reflexpr', 'reinterpret_cast', 'requires',
    'static_cast', 'synchronized', 'template', 'this',
    'throw', 'try', 'typeid', 'typename', 'using', 'virtual',
    'xor', 'xor_eq',
	'bool', 'wchar_t',
]
def cant_overlap(*args):
	for i in range(len(args)):
		for j in range(i):
			intersection = set(args[i]).intersection(args[j])
			if intersection:
				raise ValueError("Argument {} intersects with {}: {}".format(i, j, intersection))
cant_overlap(keywords_c, keywords_cpp)

keywords_rust = [
    'as', 'break', 'const', 'continue', 'crate', 'else', 'enum', 'extern', 'fn', 'for',
    'if', 'impl', 'in', 'let', 'loop', 'match', 'mod', 'move', 'mut', 'pub', 'ref', 'return',
    'self', 'Self', 'static', 'struct', 'super', 'trait', 'type', 'unsafe', 'use',
    'where', 'while', 'async', 'await', 'dyn', 'abstract', 'become', 'box', 'do', 'final',
    'macro', 'override', 'priv', 'typeof', 'unsized', 'virtual', 'yield', 'try', 'union',
]
builtins_rust = [
    'asm!','concat_idents!','format_args_nl!','global_asm!','is_aarch64_feature_detected!',
    'is_arm_feature_detected!','is_mips64_feature_detected!','is_mips_feature_detected!',
    'is_powerpc64_feature_detected!','is_powerpc_feature_detected!','llvm_asm!','log_syntax!',
    'trace_macros!','assert!','assert_eq!','assert_ne!','cfg!','column!','compile_error!',
    'concat!','dbg!','debug_assert!','debug_assert_eq!','debug_assert_ne!','env!','eprint!',
    'eprintln!','file!','format!','format_args!','include!','include_bytes!','include_str!',
    'is_x86_feature_detected!','line!','matches!','module_path!','option_env!','panic!',
    'print!','println!','stringify!','thread_local!','todo!','try!','unimplemented!',
    'unreachable!','vec!','write!','writeln!',
	'Copy', 'Send', 'Sized', 'Sync', 'Unpin', 'Drop', 'Fn', 'FnMut', 'FnOnce',
	'drop', 'Box', 'ToOwned', 'Clone', 'PartialEq', 'PartialOrd', 'Eq', 'Ord',
	'AsRef', 'AsMut', 'Into', 'From', 'Default', 'Iterator', 'Extend', 'IntoIterator',
	'DoubleEndedIterator', 'ExactSizeIterator', 'Option', 'Some', 'None', 'Result', 'Ok',
	'Err', 'String', 'ToString', 'Vec',
	'u8', 'u16', 'u32', 'u64', 'u128', 'usize',
	'i8', 'i16', 'i32', 'i64', 'i128', 'isize',
	'f32', 'f64', 'bool', 'char', 'str',
]
constants_rust = ['false', 'true']



keywords_python = ['await', 'else', 'import', 'pass', 'break', 'except', 'in', 'raise', 'class', 'finally',
    'is', 'return', 'and', 'continue', 'for', 'lambda', 'try', 'as', 'def', 'from', 'nonlocal',
    'while', 'assert', 'del', 'global', 'not', 'with', 'async', 'elif', 'if', 'or', 'yield',
]
builtins_python = ['ArithmeticError', 'AssertionError', 'AttributeError', 'BaseException', 'BlockingIOError',
    'BrokenPipeError', 'BufferError', 'BytesWarning', 'ChildProcessError', 'ConnectionAbortedError',
    'ConnectionError', 'ConnectionRefusedError', 'ConnectionResetError', 'DeprecationWarning',
    'EOFError', 'Ellipsis', 'EnvironmentError', 'Exception', 'False', 'FileExistsError', 'FileNotFoundError',
    'FloatingPointError', 'FutureWarning', 'GeneratorExit', 'IOError', 'ImportError', 'ImportWarning',
    'IndentationError', 'IndexError', 'InterruptedError', 'IsADirectoryError', 'KeyError', 'KeyboardInterrupt',
    'LookupError', 'MemoryError', 'ModuleNotFoundError', 'NameError', 'None', 'NotADirectoryError',
    'NotImplemented', 'NotImplementedError', 'OSError', 'OverflowError', 'PendingDeprecationWarning',
    'PermissionError', 'ProcessLookupError', 'RecursionError', 'ReferenceError', 'ResourceWarning',
    'RuntimeError', 'RuntimeWarning', 'StopAsyncIteration', 'StopIteration', 'SyntaxError', 'SyntaxWarning',
    'SystemError', 'SystemExit', 'TabError', 'TimeoutError', 'True', 'TypeError', 'UnboundLocalError',
    'UnicodeDecodeError', 'UnicodeEncodeError', 'UnicodeError', 'UnicodeTranslateError', 'UnicodeWarning',
    'UserWarning', 'ValueError', 'Warning', 'WindowsError', 'ZeroDivisionError', '__build_class__', '__debug__',
    '__doc__', '__import__', '__loader__', '__name__', '__package__', '__spec__', 'abs', 'all', 'any', 'ascii',
    'bin', 'bool', 'breakpoint', 'bytearray', 'bytes', 'callable', 'chr', 'classmethod', 'compile', 'complex',
    'copyright', 'credits', 'delattr', 'dict', 'dir', 'divmod', 'enumerate', 'eval', 'exec', 'exit', 'filter',
    'float', 'format', 'frozenset', 'getattr', 'globals', 'hasattr', 'hash', 'help', 'hex', 'id', 'input', 'int',
    'isinstance', 'issubclass', 'iter', 'len', 'license', 'list', 'locals', 'map', 'max', 'memoryview', 'min',
    'next', 'object', 'oct', 'open', 'ord', 'pow', 'print', 'property', 'quit', 'range', 'repr', 'reversed',
    'round', 'set', 'setattr', 'slice', 'sorted', 'staticmethod', 'str', 'sum', 'super', 'tuple', 'type',
    'vars', 'zip',
]

file = open('keywords.h', 'w')
file.write('''// keywords for all languages ted supports
// This file was auto-generated by keywords.py
''')
file.write('''typedef struct {
	char const *str;
	SyntaxCharType type;
} Keyword;\n\n''')

def label(kwds, l):
	return [(l, kwd) for kwd in kwds]

cant_overlap(keywords_c, constants_c, builtins_c)
cant_overlap(keywords_rust, builtins_rust, constants_rust)
cant_overlap(keywords_python, builtins_python)
c_things = label(keywords_c, SYNTAX_KEYWORD) + label(constants_c, SYNTAX_CONSTANT) + label(builtins_c, SYNTAX_BUILTIN)
output_keywords(file, c_things, 'c')
cpp_things = c_things + label(keywords_cpp, SYNTAX_KEYWORD)
cpp_things.remove((SYNTAX_BUILTIN, 'bool'))
cpp_things.remove((SYNTAX_BUILTIN, 'wchar_t'))
output_keywords(file, cpp_things, 'cpp')
output_keywords(file, label(keywords_rust, SYNTAX_KEYWORD) + label(builtins_rust, SYNTAX_BUILTIN) + label(constants_rust, SYNTAX_CONSTANT), 'rust')
output_keywords(file, label(keywords_python, SYNTAX_KEYWORD) + label(builtins_python, SYNTAX_BUILTIN), 'python')
file.close()
