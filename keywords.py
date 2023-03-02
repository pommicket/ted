#!/usr/bin/python3

# creates lists of keywords for all languages
# to make keyword lookup more efficient, the lists are split up by their first letter
# (or by their first codepoint, modulo 128, although no language I know of has non-ASCII keywords)
#       (probably APL but i hope no one is using APL)

import ast

types = [
	'SYNTAX_KEYWORD', 'SYNTAX_CONSTANT', 'SYNTAX_BUILTIN'
]
exec('\n'.join(['{} = {}'.format(type, i) for (i, type) in enumerate(types)]))

def process_keywords(keywords):
    keyword_types = {}
    for (type,kwd) in keywords:
    	if kwd in keyword_types:
    		print('repeated keyword:', kwd)
    	keyword_types[kwd] = type
    
    keywords_by_c = {}
    for (type,kwd) in keywords:
        c = kwd[0]
        if c in keywords_by_c:
            keywords_by_c[c].append((kwd, type))
        else:
            keywords_by_c[c] = [(kwd, type)]
    return keywords_by_c
   
def output_keywords(file, keywords, language):
    keywords = process_keywords(keywords)
    def escape(c):
    	if c.isalpha():
    		return c
    	else:
    		return 'x{:x}'.format(ord(c))
    for (c, kwds) in sorted(keywords.items()):
        kwds = list(sorted(kwds))
        file.write('static const Keyword syntax_keywords_{}_{}[{}] = {{'.format(language, escape(c), len(kwds)))
        file.write(','.join(map(lambda kwd: '{"'+kwd[0]+'", ' + types[kwd[1]] + '}', kwds)) + '};\n')
    file.write('static const KeywordList syntax_all_keywords_{}[128] = {{\n'.format(language))
    file.write('\t'+', '.join(["['{}'] = {{syntax_keywords_{}_{}, arr_count(syntax_keywords_{}_{})}}".format(
    	c, language, escape(c), language, escape(c)) for c in sorted(keywords.keys())]) + '\n')
    file.write('};\n\n')

def cant_overlap(*args):
	for i in range(len(args)):
		for j in range(i):
			intersection = set(args[i]).intersection(args[j])
			if intersection:
				raise ValueError("Argument {} intersects with {}: {}".format(i, j, intersection))

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

# see https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#keywords
keywords_glsl = [
	'const', 'uniform', 'buffer', 'shared', 'attribute', 'varying', 'coherent',
	'volatile', 'restrict', 'readonly', 'writeonly', 'atomic_uint', 'layout',
	'centroid', 'flat', 'smooth', 'noperspective', 'patch', 'sample', 'invariant',
	'precise', 'break', 'continue', 'do', 'for', 'while', 'switch', 'case', 'default',
	'if', 'else', 'subroutine', 'in', 'out', 'inout', 'int', 'void', 'bool', 'true',
	'false', 'float', 'double', 'discard', 'return', 'vec2', 'vec3', 'vec4', 'ivec2',
	'ivec3', 'ivec4', 'bvec2', 'bvec3', 'bvec4', 'uint', 'uvec2', 'uvec3', 'uvec4',
	'dvec2', 'dvec3', 'dvec4', 'mat2', 'mat3', 'mat4', 'mat2x2', 'mat2x3', 'mat2x4',
	'mat3x2', 'mat3x3', 'mat3x4', 'mat4x2', 'mat4x3', 'mat4x4', 'dmat2', 'dmat3', 'dmat4',
	'dmat2x2', 'dmat2x3', 'dmat2x4', 'dmat3x2', 'dmat3x3', 'dmat3x4', 'dmat4x2', 'dmat4x3',
	'dmat4x4', 'lowp', 'mediump', 'highp', 'precision', 'sampler1D', 'sampler1DShadow',
	'sampler1DArray', 'sampler1DArrayShadow', 'isampler1D', 'isampler1DArray', 'usampler1D',
	'usampler1DArray', 'sampler2D', 'sampler2DShadow', 'sampler2DArray', 'sampler2DArrayShadow',
	'isampler2D', 'isampler2DArray', 'usampler2D', 'usampler2DArray', 'sampler2DRect',
	'sampler2DRectShadow', 'isampler2DRect', 'usampler2DRect', 'sampler2DMS', 'isampler2DMS',
	'usampler2DMS', 'sampler2DMSArray', 'isampler2DMSArray', 'usampler2DMSArray', 'sampler3D',
	'isampler3D', 'usampler3D', 'samplerCube', 'samplerCubeShadow', 'isamplerCube', 'usamplerCube',
	'samplerCubeArray', 'samplerCubeArrayShadow', 'isamplerCubeArray', 'usamplerCubeArray', 'samplerBuffer',
	'isamplerBuffer', 'usamplerBuffer', 'image1D', 'iimage1D', 'uimage1D', 'image1DArray', 'iimage1DArray',
	'uimage1DArray', 'image2D', 'iimage2D', 'uimage2D', 'image2DArray', 'iimage2DArray', 'uimage2DArray',
	'image2DRect', 'iimage2DRect', 'uimage2DRect', 'image2DMS', 'iimage2DMS', 'uimage2DMS',
	'image2DMSArray', 'iimage2DMSArray', 'uimage2DMSArray', 'image3D', 'iimage3D', 'uimage3D',
	'imageCube', 'iimageCube', 'uimageCube', 'imageCubeArray', 'iimageCubeArray', 'uimageCubeArray',
	'imageBuffer', 'iimageBuffer', 'uimageBuffer', 'struct'
]

# extracted from https://registry.khronos.org/OpenGL-Refpages/gl4/index.php
builtins_glsl = ['abs', 'acos', 'acosh', 'all', 'any', 'asin', 'asinh', 'atan', 'atanh',
	'atomicAdd', 'atomicAnd', 'atomicCompSwap', 'atomicCounter', 'atomicCounterDecrement',
	'atomicCounterIncrement', 'atomicExchange', 'atomicMax', 'atomicMin', 'atomicOr',
	'atomicXor', 'barrier', 'bitCount', 'bitfieldExtract', 'bitfieldInsert', 'bitfieldReverse',
	'ceil', 'clamp', 'cos', 'cosh', 'cross', 'degrees', 'determinant', 'dFdx', 'dFdxCoarse',
	'dFdxFine', 'dFdy', 'dFdyCoarse', 'dFdyFine', 'distance', 'dot', 'EmitStreamVertex', 'EmitVertex',
	'EndPrimitive', 'EndStreamPrimitive', 'equal', 'exp', 'exp2', 'faceforward', 'findLSB', 'findMSB',
	'floatBitsToInt', 'floatBitsToUint', 'floor', 'fma', 'fract', 'frexp', 'fwidth', 'fwidthCoarse',
	'fwidthFine', 'gl_ClipDistance', 'gl_CullDistance', 'gl_FragCoord', 'gl_FragDepth', 'gl_FrontFacing',
	'gl_GlobalInvocationID', 'gl_HelperInvocation', 'gl_InstanceID', 'gl_InvocationID', 'gl_Layer',
	'gl_LocalInvocationID', 'gl_LocalInvocationIndex', 'gl_NumSamples', 'gl_NumWorkGroups',
	'gl_PatchVerticesIn', 'gl_PointCoord', 'gl_PointSize', 'gl_Position', 'gl_PrimitiveID',
	'gl_PrimitiveIDIn', 'gl_SampleID', 'gl_SampleMask', 'gl_SampleMaskIn', 'gl_SamplePosition',
	'gl_TessCoord', 'gl_TessLevelInner', 'gl_TessLevelOuter', 'gl_VertexID', 'gl_ViewportIndex',
	'gl_WorkGroupID', 'gl_WorkGroupSize', 'greaterThan', 'greaterThanEqual', 'groupMemoryBarrier',
	'imageAtomicAdd', 'imageAtomicAnd', 'imageAtomicCompSwap', 'imageAtomicExchange', 'imageAtomicMax',
	'imageAtomicMin', 'imageAtomicOr', 'imageAtomicXor', 'imageLoad', 'imageSamples', 'imageSize',
	'imageStore', 'imulExtended', 'intBitsToFloat', 'interpolateAtCentroid', 'interpolateAtOffset',
	'interpolateAtSample', 'inverse', 'inversesqrt', 'isinf', 'isnan', 'ldexp', 'length', 'lessThan',
	'lessThanEqual', 'log', 'log2', 'matrixCompMult', 'max', 'memoryBarrier', 'memoryBarrierAtomicCounter',
	'memoryBarrierBuffer', 'memoryBarrierImage', 'memoryBarrierShared', 'min', 'mix', 'mod', 'modf',
	'noise', 'noise1', 'noise2', 'noise3', 'noise4', 'normalize', 'not', 'notEqual', 'outerProduct',
	'packDouble2x32', 'packHalf2x16', 'packSnorm2x16', 'packSnorm4x8', 'packUnorm', 'packUnorm2x16',
	'packUnorm4x8', 'pow', 'radians', 'reflect', 'refract', 'removedTypes', 'round', 'roundEven',
	'sign', 'sin', 'sinh', 'smoothstep', 'sqrt', 'step', 'tan', 'tanh', 'texelFetch', 'texelFetchOffset',
	'texture', 'textureGather', 'textureGatherOffset', 'textureGatherOffsets', 'textureGrad',
	'textureGradOffset', 'textureLod', 'textureLodOffset', 'textureOffset', 'textureProj',
	'textureProjGrad', 'textureProjGradOffset', 'textureProjLod', 'textureProjLodOffset',
	'textureProjOffset', 'textureQueryLevels', 'textureQueryLod', 'textureSamples', 'textureSize',
	'transpose', 'trunc', 'uaddCarry', 'uintBitsToFloat', 'umulExtended', 'unpackDouble2x32',
	'unpackHalf2x16', 'unpackSnorm2x16', 'unpackSnorm4x8', 'unpackUnorm', 'unpackUnorm2x16',
	'unpackUnorm4x8', 'usubBorrow']
constants_glsl = []

	
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

attributes_html = [
	'accept','accept-charset','accesskey','action','align','alt','async',
	'autocomplete','autofocus','autoplay','bgcolor','border','charset',
	'checked','cite','class','color','cols','colspan','content',
	'contenteditable','controls','coords','data','datetime',
	'default','defer','dir','dirname','disabled','download','draggable',
	'enctype','for','form','formaction','headers','height','hidden','high',
	'href','hreflang','http-equiv','id','ismap','kind','label','lang','list',
	'loop','low','max','maxlength','media','method','min','multiple','muted',
	'name','novalidate','onabort','onafterprint','onbeforeprint',
	'onbeforeunload','onblur','oncanplay','oncanplaythrough','onchange',
	'onclick','oncontextmenu','oncopy','oncuechange','oncut','ondblclick',
	'ondrag','ondragend','ondragenter','ondragleave','ondragover','ondragstart',
	'ondrop','ondurationchange','onemptied','onended','onerror','onfocus',
	'onhashchange','oninput','oninvalid','onkeydown','onkeypress','onkeyup',
	'onload','onloadeddata','onloadedmetadata','onloadstart','onmousedown',
	'onmousemove','onmouseout','onmouseover','onmouseup','onmousewheel','onoffline',
	'ononline','onpagehide','onpageshow','onpaste','onpause','onplay','onplaying',
	'onpopstate','onprogress','onratechange','onreset','onresize','onscroll',
	'onsearch','onseeked','onseeking','onselect','onstalled','onstorage',
	'onsubmit','onsuspend','ontimeupdate','ontoggle','onunload','onvolumechange',
	'onwaiting','onwheel','open','optimum','pattern','placeholder','poster',
	'preload','readonly','rel','required','reversed','rows','rowspan','sandbox',
	'scope','selected','shape','size','sizes','span','spellcheck','src','srcdoc',
	'srclang','srcset','start','step','style','tabindex','target',
	'title','translate','type','usemap','value','width','wrap'
]
constants_config = [
	'on', 'off', 'yes', 'no', 'true', 'false'
]

assert len(attributes_html) == len(set(attributes_html))

builtins_html = []
for attr in attributes_html:
	builtins_html.append(attr + '=')


keywords_javascript = [
	'break', 'case', 'catch', 'class', 'const',
	'continue', 'debugger', 'default', 'delete',
	'do', 'else', 'export', 'extends', 'finally',
	'for', 'function', 'if', 'import', 'in',
	'instanceof', 'new', 'return', 'super',
	'switch', 'this', 'throw', 'try', 'typeof',
	'var', 'void', 'while', 'with', 'yield',
	'let', 'await'
]

constants_json = [
	'true', 'false', 'null'
]
constants_javascript = constants_json + ['undefined']

builtins_javascript = [
	'AggregateError','Array','ArrayBuffer','AsyncFunction','AsyncGenerator','AsyncGeneratorFunction',
	'Atomics','BigInt','BigInt64Array','BigUint64Array','Boolean','DataView','Date','decodeURI',
	'decodeURIComponent','encodeURI','encodeURIComponent','Error','eval','EvalError','FinalizationRegistry',
	'Float32Array','Float64Array','Function','Generator','GeneratorFunction','globalThis','Infinity',
	'Int16Array','Int32Array','Int8Array','InternalError','Intl','isFinite','isNaN','JSON','Map','Math',
	'NaN','Number','Object','parseFloat','parseInt','Promise','Proxy','RangeError','ReferenceError',
	'Reflect','RegExp','Set','SharedArrayBuffer','String','Symbol','SyntaxError','TypedArray',
	'TypeError','Uint16Array','Uint32Array','Uint8Array','Uint8ClampedArray',
	'URIError','WeakMap','WeakRef','WeakSet','WebAssembly'
]

keywords_java = [
	'abstract', 'continue', 'for', 'new', 'switch',
	'assert', 'default', 'goto', 'package', 'synchronized',
	'boolean', 'do', 'if', 'private', 'this',
	'break', 'double', 'implements', 'protected', 'throw',
	'byte', 'else', 'import', 'public', 'throws',
	'case', 'enum', 'instanceof', 'return', 'transient',
	'catch', 'extends', 'int', 'short', 'try',
	'char', 'final', 'interface', 'static', 'var',
	'class', 'finally', 'long', 'strictfp', 'void',
	'const', 'float', 'native', 'super', 'volatile', 'while'
]


keywords_typescript = keywords_javascript + [
	'public', 'any', 'as', 'module',
	'static', 'interface', 'enum', 'type',
	'implements', 'private', 'package'
]

builtins_typescript = builtins_javascript + [
	'string', 'number', 'get', 'set'
]

constants_typescript = constants_javascript

constants_java = [
	'true', 'false', 'null'
]

keywords_go = [
	'break', 'default', 'func', 'interface', 'select',
	'case', 'defer', 'go', 'map', 'struct',
	'chan', 'else', 'goto', 'package', 'switch',
	'const', 'fallthrough', 'if', 'range', 'type',
	'continue', 'for', 'import', 'return', 'var'
]

constants_go = [
	'true', 'false', 'iota', 'nil'
]

builtins_go = [
	'new', 'make', 'cap', 'len', 'close', 'append', 'copy', 'delete',
	'complex', 'real', 'imag', 'panic', 'recover', 'print', 'println',
	'bool', 'uint8', 'uint16', 'uint32', 'uint64',
	'int8', 'int16', 'int32', 'int64',
	'float32', 'float64', 'complex64', 'complex128',
	'byte', 'rune', 'uint', 'int', 'uintptr', 'string', 'error',
	'comparable'
]

builtins_css = [
	'-webkit-line-clamp:', 'abs', 'accent-color:', 'acos', 
	'additive-symbols:', 'align-content:', 'align-items:', 'align-self:', 'align-tracks:', 'all:', 'animation:', 
	'animation-composition:', 'animation-delay:', 'animation-direction:', 'animation-duration:', 'animation-fill-mode:', 
	'animation-iteration-count:', 'animation-name:', 'animation-play-state:', 'animation-timeline:', 
	'animation-timing-function:', 'annotation', 'appearance:', 'ascent-override:', 'asin', 'aspect-ratio:', 'atan', 
	'atan2', 'attr', 'backdrop-filter:', 'backface-visibility:', 'background:', 'background-attachment:', 
	'background-blend-mode:', 'background-clip:', 'background-color:', 'background-image:', 'background-origin:', 
	'background-position:', 'background-position-x:', 'background-position-y:', 'background-repeat:', 'background-size:', 
	'bleed:', 'block-overflow:', 'block-size:', 'blur', 'border:', 'border-block:', 'border-block-color:', 
	'border-block-end:', 'border-block-end-color:', 'border-block-end-style:', 'border-block-end-width:', 
	'border-block-start:', 'border-block-start-color:', 'border-block-start-style:', 'border-block-start-width:', 
	'border-block-style:', 'border-block-width:', 'border-bottom:', 'border-bottom-color:', 'border-bottom-left-radius:', 
	'border-bottom-right-radius:', 'border-bottom-style:', 'border-bottom-width:', 'border-collapse:', 'border-color:', 
	'border-end-end-radius:', 'border-end-start-radius:', 'border-image:', 'border-image-outset:', 'border-image-repeat:', 
	'border-image-slice:', 'border-image-source:', 'border-image-width:', 'border-inline:', 'border-inline-color:', 
	'border-inline-end:', 'border-inline-end-color:', 'border-inline-end-style:', 'border-inline-end-width:', 
	'border-inline-start:', 'border-inline-start-color:', 'border-inline-start-style:', 'border-inline-start-width:', 
	'border-inline-style:', 'border-inline-width:', 'border-left:', 'border-left-color:', 'border-left-style:', 
	'border-left-width:', 'border-radius:', 'border-right:', 'border-right-color:', 'border-right-style:', 
	'border-right-width:', 'border-spacing:', 'border-start-end-radius:', 'border-start-start-radius:', 'border-style:', 
	'border-top:', 'border-top-color:', 'border-top-left-radius:', 'border-top-right-radius:', 'border-top-style:', 
	'border-top-width:', 'border-width:', 'box-decoration-break:', 'box-shadow:', 'box-sizing:', 'break-after:', 
	'break-before:', 'break-inside:', 'brightness', 'calc', 'caption-side:', 'caret:', 'caret-color:', 'caret-shape:', 
	'character-variant', 'circle', 'clamp', 'clear:', 'clip:', 'clip-path:', 'color:', 'color-scheme:', 'column-count:', 
	'column-fill:', 'column-gap:', 'column-rule:', 'column-rule-color:', 'column-rule-style:', 'column-rule-width:', 
	'column-span:', 'column-width:', 'columns:', 'conic-gradient', 'contain:', 'contain-intrinsic-block-size:', 
	'contain-intrinsic-height:', 'contain-intrinsic-inline-size:', 'contain-intrinsic-size:', 'contain-intrinsic-width:', 
	'content:', 'content-visibility:', 'contrast', 'cos', 'counter-increment:', 'counter-reset:', 'counter-set:', 
	'counters', 'cross-fade', 'cubic-bezier', 'cursor:', 'descent-override:', 'direction:', 'display:', 'drop-shadow', 
	'element', 'ellipse', 'empty-cells:', 'env', 'exp', 'fallback:', 'filter:', 'flex:', 'flex-basis:', 'flex-direction:', 
	'flex-flow:', 'flex-grow:', 'flex-shrink:', 'flex-wrap:', 'float:', 'font:', 'font-display:', 'font-family:', 
	'font-feature-settings:', 'font-kerning:', 'font-language-override:', 'font-optical-sizing:', 'font-size:', 
	'font-size-adjust:', 'font-stretch:', 'font-style:', 'font-synthesis:', 'font-variant:', 'font-variant-alternates:', 
	'font-variant-caps:', 'font-variant-east-asian:', 'font-variant-ligatures:', 'font-variant-numeric:', 
	'font-variant-position:', 'font-variation-settings:', 'font-weight:', 'forced-color-adjust:', 'format', 'gap:', 
	'grayscale', 'grid:', 'grid-area:', 'grid-auto-columns:', 'grid-auto-flow:', 'grid-auto-rows:', 'grid-column:', 
	'grid-column-end:', 'grid-column-start:', 'grid-row:', 'grid-row-end:', 'grid-row-start:', 'grid-template:', 
	'grid-template-areas:', 'grid-template-columns:', 'grid-template-rows:', 'hanging-punctuation:', 'height:', 'hsl', 
	'hsla', 'hue-rotate', 'hwb', 'hyphenate-character:', 'hyphenate-limit-chars:', 'hyphens:', 'hypot', 'image', 
	'image-orientation:', 'image-rendering:', 'image-resolution:', 'image-set', 'inherits:', 'initial-letter:', 
	'initial-letter-align:', 'initial-value:', 'inline-size:', 'input-security:', 'inset', 'inset-block:', 
	'inset-block-end:', 'inset-block-start:', 'inset-inline:', 'inset-inline-end:', 'inset-inline-start:', 'invert', 
	'isolation:', 'justify-content:', 'justify-items:', 'justify-self:', 'justify-tracks:', 'lab', 'layer', 'lch', 
	'leader', 'letter-spacing:', 'line-break:', 'line-clamp:', 'line-gap-override:', 'line-height:', 'line-height-step:', 
	'linear-gradient', 'list-style:', 'list-style-image:', 'list-style-position:', 'list-style-type:', 'local', 'log', 
	'margin:', 'margin-block:', 'margin-block-end:', 'margin-block-start:', 'margin-bottom:', 'margin-inline:', 
	'margin-inline-end:', 'margin-inline-start:', 'margin-left:', 'margin-right:', 'margin-top:', 'margin-trim:', 'marks:', 
	'mask:', 'mask-border:', 'mask-border-mode:', 'mask-border-outset:', 'mask-border-repeat:', 'mask-border-slice:', 
	'mask-border-source:', 'mask-border-width:', 'mask-clip:', 'mask-composite:', 'mask-image:', 'mask-mode:', 
	'mask-origin:', 'mask-position:', 'mask-repeat:', 'mask-size:', 'mask-type:', 'masonry-auto-flow:', 'math-depth:', 
	'math-shift:', 'math-style:', 'matrix', 'matrix3d', 'max', 'max-block-size:', 'max-height:', 'max-inline-size:', 
	'max-lines:', 'max-width:', 'max-zoom:', 'min', 'min-block-size:', 'min-height:', 'min-inline-size:', 'min-width:', 
	'min-zoom:', 'minmax', 'mix-blend-mode:', 'mod', 'negative:', 'object-fit:', 'object-position:', 'offset:', 
	'offset-anchor:', 'offset-distance:', 'offset-path:', 'offset-position:', 'offset-rotate:', 'opacity', 'order:', 
	'orientation:', 'ornaments', 'orphans:', 'outline:', 'outline-color:', 'outline-offset:', 'outline-style:', 
	'outline-width:', 'overflow:', 'overflow-anchor:', 'overflow-block:', 'overflow-clip-margin:', 'overflow-inline:', 
	'overflow-wrap:', 'overflow-x:', 'overflow-y:', 'overscroll-behavior:', 'overscroll-behavior-block:', 
	'overscroll-behavior-inline:', 'overscroll-behavior-x:', 'overscroll-behavior-y:', 'pad:', 'padding:', 
	'padding-block:', 'padding-block-end:', 'padding-block-start:', 'padding-bottom:', 'padding-inline:', 
	'padding-inline-end:', 'padding-inline-start:', 'padding-left:', 'padding-right:', 'padding-top:', 'page-break-after:', 
	'page-break-before:', 'page-break-inside:', 'paint', 'paint-order:', 'path', 'perspective', 'perspective-origin:', 
	'place-content:', 'place-items:', 'place-self:', 'pointer-events:', 'polygon', 'position:', 'pow', 'prefix:', 
	'print-color-adjust:', 'quotes:', 'radial-gradient', 'range:', 'rect', 'rem', 'repeat', 'repeating-conic-gradient', 
	'repeating-linear-gradient', 'repeating-radial-gradient', 'resize:', 'reversed', 'rgb', 'rgba', 'rotate', 'rotate3d', 
	'rotateX', 'rotateY', 'rotateZ', 'row-gap:', 'ruby-align:', 'ruby-merge:', 'ruby-position:', 'saturate', 'scale', 
	'scale3d', 'scaleX', 'scaleY', 'scaleZ', 'scroll', 'scroll-behavior:', 'scroll-margin:', 'scroll-margin-block:', 
	'scroll-margin-block-end:', 'scroll-margin-block-start:', 'scroll-margin-bottom:', 'scroll-margin-inline:', 
	'scroll-margin-inline-end:', 'scroll-margin-inline-start:', 'scroll-margin-left:', 'scroll-margin-right:', 
	'scroll-margin-top:', 'scroll-padding:', 'scroll-padding-block:', 'scroll-padding-block-end:', 
	'scroll-padding-block-start:', 'scroll-padding-bottom:', 'scroll-padding-inline:', 'scroll-padding-inline-end:', 
	'scroll-padding-inline-start:', 'scroll-padding-left:', 'scroll-padding-right:', 'scroll-padding-top:', 
	'scroll-snap-align:', 'scroll-snap-stop:', 'scroll-snap-type:', 'scroll-timeline:', 'scroll-timeline-axis:', 
	'scroll-timeline-name:', 'scrollbar-color:', 'scrollbar-gutter:', 'scrollbar-width:', 'selector', 'sepia', 
	'shape-image-threshold:', 'shape-margin:', 'shape-outside:', 'sign', 'sin', 'size:', 'size-adjust:', 'skew', 'skewX', 
	'skewY', 'speak-as:', 'sqrt', 'src:', 'steps', 'styleset', 'stylistic', 'suffix:', 'supports', 'swash', 'symbols', 
	'syntax:', 'system:', 'tab-size:', 'table-layout:', 'tan', 'target-counter', 'target-counters', 'target-text', 
	'text-align:', 'text-align-last:', 'text-combine-upright:', 'text-decoration:', 'text-decoration-color:', 
	'text-decoration-line:', 'text-decoration-skip:', 'text-decoration-skip-ink:', 'text-decoration-style:', 
	'text-decoration-thickness:', 'text-emphasis:', 'text-emphasis-color:', 'text-emphasis-position:', 
	'text-emphasis-style:', 'text-indent:', 'text-justify:', 'text-orientation:', 'text-overflow:', 'text-rendering:', 
	'text-shadow:', 'text-size-adjust:', 'text-transform:', 'text-underline-offset:', 'text-underline-position:', 
	'touch-action:', 'transform:', 'transform-box:', 'transform-origin:', 'transform-style:', 'transition:', 
	'transition-delay:', 'transition-duration:', 'transition-property:', 'transition-timing-function:', 'translate', 
	'translate3d', 'translateX', 'translateY', 'translateZ', 'type', 'unicode-bidi:', 'unicode-range:', 'url', 
	'user-select:', 'user-zoom:', 'var', 'vertical-align:', 'viewport-fit:', 'visibility:', 'white-space:', 'widows:', 
	'width:', 'will-change:', 'word-break:', 'word-spacing:', 'word-wrap:', 'writing-mode:', 'z-index:', 'zoom:',
	'left:','right:','top:','bottom:',
]

constants_css = [
	'aliceblue', 'antiquewhite', 'aqua', 'aquamarine', 'azure', 'beige', 'bisque', 'black', 'blanchedalmond', 'blue', 
	'blueviolet', 'brown', 'burlywood', 'cadetblue', 'chartreuse', 'chocolate', 'coral', 'cornflowerblue', 'cornsilk', 
	'crimson', 'cyan', 'darkblue', 'darkcyan', 'darkgoldenrod', 'darkgray', 'darkgreen', 'darkgrey', 'darkkhaki', 
	'darkmagenta', 'darkolivegreen', 'darkorange', 'darkorchid', 'darkred', 'darksalmon', 'darkseagreen', 'darkslateblue', 
	'darkslategray', 'darkslategrey', 'darkturquoise', 'darkviolet', 'deeppink', 'deepskyblue', 'dimgray', 'dimgrey', 
	'dodgerblue', 'firebrick', 'floralwhite', 'forestgreen', 'fuchsia', 'gainsboro', 'ghostwhite', 'gold', 'goldenrod', 
	'gray', 'green', 'greenyellow', 'grey', 'honeydew', 'hotpink', 'indianred', 'indigo', 'ivory', 'khaki', 'lavender', 
	'lavenderblush', 'lawngreen', 'lemonchiffon', 'lightblue', 'lightcoral', 'lightcyan', 'lightgoldenrodyellow', 
	'lightgray', 'lightgreen', 'lightgrey', 'lightpink', 'lightsalmon', 'lightseagreen', 'lightskyblue', 'lightslategray', 
	'lightslategrey', 'lightsteelblue', 'lightyellow', 'lime', 'limegreen', 'linen', 'magenta', 'maroon', 
	'mediumaquamarine', 'mediumblue', 'mediumorchid', 'mediumpurple', 'mediumseagreen', 'mediumslateblue', 
	'mediumspringgreen', 'mediumturquoise', 'mediumvioletred', 'midnightblue', 'mintcream', 'mistyrose', 'moccasin', 
	'navajowhite', 'navy', 'oldlace', 'olive', 'olivedrab', 'orange', 'orangered', 'orchid', 'palegoldenrod', 'palegreen', 
	'paleturquoise', 'palevioletred', 'papayawhip', 'peachpuff', 'peru', 'pink', 'plum', 'powderblue', 'purple', 'red', 
	'rosybrown', 'royalblue', 'saddlebrown', 'salmon', 'sandybrown', 'seagreen', 'seashell', 'sienna', 'silver', 'skyblue', 
	'slateblue', 'slategray', 'slategrey', 'snow', 'springgreen', 'steelblue', 'teal', 'thistle', 'tomato', 
	'turquoise', 'violet', 'wheat', 'white', 'whitesmoke', 'yellow', 'yellowgreen',
	'above', 'absolute', 'active', 'add', 'additive', 'after-edge', 'alias', 'all-petite-caps', 'all-scroll', 
	'all-small-caps', 'alpha', 'alphabetic', 'alternate', 'alternate-reverse', 'always', 'antialiased', 'auto', 'auto-pos', 
	'available', 'avoid', 'avoid-column', 'avoid-page', 'avoid-region', 'backwards', 'balance', 'baseline', 'before-edge', 
	'below', 'bevel', 'bidi-override', 'block', 'block-axis', 'block-start', 'block-end', 'bold', 'bolder', 
	'border-box', 'both', 'bottom', 'bottom-outside', 'break-all', 'break-word', 'bullets', 'butt', 'capitalize', 
	'cell', 'center', 'central', 'char', 'clone', 'close-quote', 'closest-corner', 
	'closest-side', 'col-resize', 'collapse', 'color-burn', 'color-dodge', 'column', 'column-reverse', 
	'common-ligatures', 'compact', 'condensed', 'content-box', 'contents', 'context-menu', 
	'contextual', 'copy', 'cover', 'crisp-edges', 'crispEdges', 'crosshair', 'cyclic', 'dark', 'darken', 'dashed', 
	'decimal', 'default', 'dense', 'diagonal-fractions', 'difference', 'digits', 'disabled', 'disc', 
	'discretionary-ligatures', 'distribute', 'distribute-all-lines', 'distribute-letter', 'distribute-space', 'dot', 
	'dotted', 'double', 'double-circle', 'downleft', 'downright', 'e-resize', 'each-line', 'ease', 'ease-in', 
	'ease-in-out', 'ease-out', 'economy', 'ellipsis', 'end', 'evenodd', 'ew-resize', 'exact', 
	'exclude', 'exclusion', 'expanded', 'extends', 'extra-condensed', 'extra-expanded', 'farthest-corner', 
	'farthest-side', 'fill', 'fill-available', 'fill-box', 'filled', 'fit-content', 'fixed', 'flat',
	'flip', 'flow-root', 'forwards', 'freeze', 'from-image', 'full-width', 'geometricPrecision', 'georgian', 
	'grab', 'grabbing', 'groove', 'hand', 'hanging', 'hard-light', 'help', 'hidden', 'hide', 
	'historical-forms', 'historical-ligatures', 'horizontal', 'horizontal-tb', 'hue', 'icon', 'ideograph-alpha', 
	'ideograph-numeric', 'ideograph-parenthesis', 'ideograph-space', 'ideographic', 'inactive', 'infinite', 'inherit', 
	'initial', 'inline', 'inline-axis', 'inline-block', 'inline-end', 'inline-flex', 'inline-grid', 'inline-list-item', 
	'inline-start', 'inline-table', 'inside', 'inter-character', 'inter-ideograph', 'inter-word', 'intersect', 
	'isolate', 'isolate-override', 'italic', 'jis04', 'jis78', 'jis83', 'jis90', 'justify', 'justify-all', 
	'kannada', 'keep-all', 'landscape', 'large', 'larger', 'left', 'light', 'lighten', 'lighter', 'line', 'line-edge', 
	'line-through', 'linear', 'linearRGB', 'lining-nums', 'list-item', 'loose', 'lowercase', 'lr', 'lr-tb', 'ltr', 
	'luminance', 'luminosity', 'main-size', 'mandatory', 'manipulation', 'manual', 'margin-box', 'match-parent', 
	'match-source', 'mathematical', 'max-content', 'medium', 'message-box', 'middle', 'min-content', 'miter', 
	'mixed', 'move', 'multiply', 'n-resize', 'narrower', 'ne-resize', 'nearest-neighbor', 'nesw-resize', 'newspaper', 
	'no-change', 'no-clip', 'no-close-quote', 'no-common-ligatures', 'no-contextual', 'no-discretionary-ligatures', 
	'no-drop', 'no-historical-ligatures', 'no-open-quote', 'no-repeat', 'none', 'nonzero', 'normal', 'not-allowed', 
	'nowrap', 'ns-resize', 'numbers', 'numeric', 'nw-resize', 'nwse-resize', 'oblique', 'oldstyle-nums', 'open', 
	'open-quote', 'optimizeLegibility', 'optimizeQuality', 'optimizeSpeed', 'optional', 'ordinal', 'outset', 'outside', 
	'over', 'overlay', 'overline', 'page', 'painted', 'pan-down', 'pan-left', 'pan-right', 
	'pan-up', 'pan-x', 'pan-y', 'paused', 'petite-caps', 'pixelated', 'plaintext', 'pointer', 'portrait', 
	'pre-line', 'pre-wrap', 'preserve-3d', 'progressive', 'proportional-nums', 'proportional-width', 
	'proximity', 'radial', 'recto', 'region', 'relative', 'remove', 'reset-size', 'reverse', 
	'revert', 'ridge', 'right', 'rl', 'rl-tb', 'round', 'row', 'row-resize', 'row-reverse', 'row-severse', 'rtl', 
	'ruby-base', 'ruby-base-container', 'ruby-text', 'ruby-text-container', 'run-in', 'running', 's-resize', 'saturation', 
	'scale-down', 'screen', 'se-resize', 'semi-condensed', 'semi-expanded', 'separate', 
	'sesame', 'show', 'sideways', 'sideways-left', 'sideways-lr', 'sideways-right', 'sideways-rl', 'simplified', 
	'slashed-zero', 'slice', 'small', 'small-caps', 'small-caption', 'smaller', 'smooth', 'soft-light', 'solid', 'space', 
	'space-around', 'space-between', 'space-evenly', 'spell-out', 'square', 'sRGB', 'stacked-fractions', 'start', 'static', 
	'status-bar', 'swap', 'step-end', 'step-start', 'sticky', 'stretch', 'strict', 'stroke', 'stroke-box', 'style', 
	'subgrid', 'subpixel-antialiased', 'subtract', 'super', 'sw-resize', 'symbolic', 'table-caption', 
	'table-cell', 'table-column', 'table-column-group', 'table-footer-group', 'table-header-group', 'table-row', 
	'table-row-group', 'tabular-nums', 'tb', 'tb-rl', 'text', 'text-after-edge', 'text-before-edge', 'text-bottom', 
	'text-top', 'thick', 'thin', 'titling-caps', 'top', 'top-outside', 'touch', 'traditional', 'transparent', 'triangle', 
	'ultra-condensed', 'ultra-expanded', 'under', 'underline', 'unicase', 'unset', 'upleft', 'uppercase', 'upright', 
	'use-glyph-orientation', 'use-script', 'verso', 'vertical', 'vertical-ideographic', 'vertical-lr', 'vertical-rl', 
	'vertical-text', 'view-box', 'visible', 'visibleFill', 'visiblePainted', 'visibleStroke', 'w-resize', 'wait', 'wavy', 
	'weight', 'whitespace', 'wider', 'words', 'wrap', 'wrap-reverse', 'x', 'x-large', 'x-small', 'xx-large', 'xx-small', 
	'y', 'zero', 'zoom-in', 'zoom-out', '!important',
]
keywords_css =  [
	 '@annotation', '@bottom-center', '@character-variant', '@charset', '@counter-style', 
	'@font-face', '@font-feature-values', '@historical-forms', '@import', '@keyframes', '@layer', '@left-bottom', 
	'@media', '@namespace', '@ornaments', '@page', '@property', '@right-bottom', '@scroll-timeline', '@styleset', 
	'@stylistic', '@supports', '@swash', '@top-center', '@viewport',
	'a', 'abbr', 'acronym', 'address', 'applet', 'area', 'article', 'aside', 'audio', 'b', 'base', 'bdi', 'bdo', 
	'bgsound', 'big', 'blink', 'blockquote', 'body', 'br', 'button', 'canvas', 'caption', 'cite', 'code', 'col', 
	'colgroup', 'content', 'data', 'datalist', 'dd', 'del', 'details', 'dfn', 'dialog', 'dir', 'div', 'dl', 'dt', 'em', 
	'embed', 'fieldset', 'figcaption', 'figure', 'font', 'footer', 'form', 'frame', 'frameset', 'head', 'header', 
	'hgroup', 'hr', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'html', 'i', 'iframe', 'img', 'input', 'ins', 'kbd', 'keygen', 'label', 'legend', 'li', 
	'link', 'main', 'map', 'mark', 'marquee', 'menu', 'menuitem', 'meta', 'meter', 'nav', 'nobr', 'noembed', 'noframes', 
	'noscript', 'object', 'ol', 'optgroup', 'option', 'output', 'p', 'param', 'picture', 'portal', 'pre', 
	'progress', 'q', 'rb', 'rp', 'rt', 'rtc', 'ruby', 's', 'samp', 'script', 'section', 'select', 'shadow', 'slot', 
	'source', 'spacer', 'span', 'strike', 'strong', 'sub', 'summary', 'sup', 'table', 'tbody', 'td', 
	'template', 'textarea', 'tfoot', 'th', 'thead', 'time', 'title', 'tr', 'track', 'tt', 'u', 'ul', 'video', 'wbr', 
	'xmp', 'svg'
]



file = open('keywords.h', 'w')
file.write('''// keywords for all languages ted supports
// This file was auto-generated by keywords.py
''')
file.write('''typedef struct {
	const char *str;
	SyntaxCharType type;
} Keyword;
typedef struct {
	const Keyword *keywords;
	size_t len;
} KeywordList;\n\n''')

def label(kwds, l):
	return [(l, kwd) for kwd in kwds]

c_things = label(keywords_c, SYNTAX_KEYWORD) + label(constants_c, SYNTAX_CONSTANT) + label(builtins_c, SYNTAX_BUILTIN)
output_keywords(file, c_things, 'c')
cpp_things = c_things + label(keywords_cpp, SYNTAX_KEYWORD)
cpp_things.remove((SYNTAX_BUILTIN, 'bool'))
cpp_things.remove((SYNTAX_BUILTIN, 'wchar_t'))
output_keywords(file, cpp_things, 'cpp')
output_keywords(file, label(keywords_rust, SYNTAX_KEYWORD) + label(builtins_rust, SYNTAX_BUILTIN) + label(constants_rust, SYNTAX_CONSTANT), 'rust')
output_keywords(file, label(keywords_javascript, SYNTAX_KEYWORD) + label(builtins_javascript, SYNTAX_BUILTIN) +
	label(constants_javascript, SYNTAX_CONSTANT), 'javascript')
output_keywords(file, label(constants_json, SYNTAX_CONSTANT), 'json')
output_keywords(file, label(keywords_typescript, SYNTAX_KEYWORD) + label(builtins_typescript, SYNTAX_BUILTIN) +
	label(constants_typescript, SYNTAX_CONSTANT), 'typescript')
output_keywords(file, label(keywords_go, SYNTAX_KEYWORD) + label(builtins_go, SYNTAX_BUILTIN) +
	label(constants_go, SYNTAX_CONSTANT), 'go')
output_keywords(file, label(keywords_java, SYNTAX_KEYWORD) + label(constants_java, SYNTAX_CONSTANT), 'java')
output_keywords(file, label(keywords_python, SYNTAX_KEYWORD) + label(builtins_python, SYNTAX_BUILTIN), 'python')
output_keywords(file, label(builtins_html, SYNTAX_BUILTIN), 'html')
output_keywords(file, label(constants_config, SYNTAX_CONSTANT), 'config')
output_keywords(file, label(keywords_glsl, SYNTAX_KEYWORD) + label(constants_glsl, SYNTAX_CONSTANT)
	+ label(builtins_glsl, SYNTAX_BUILTIN), 'glsl')
output_keywords(file, label(builtins_css, SYNTAX_BUILTIN) + label(constants_css, SYNTAX_CONSTANT) + label(keywords_css, SYNTAX_KEYWORD), 'css')
file.close()
