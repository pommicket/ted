// returns the language this string is referring to, or LANG_NONE if it's invalid.
Language language_from_str(char const *str) {
	for (int i = 0; i < LANG_COUNT; ++i) {
		if (streq(language_names[i].name, str))
			return language_names[i].lang;
	}
	return LANG_NONE;
}

// NOTE: returns the color setting, not the color
ColorSetting syntax_char_type_to_color(SyntaxCharType t) {
	switch (t) {
	case SYNTAX_NORMAL: return COLOR_TEXT;
	case SYNTAX_KEYWORD: return COLOR_KEYWORD;
	case SYNTAX_COMMENT: return COLOR_COMMENT;
	case SYNTAX_PREPROCESSOR: return COLOR_PREPROCESSOR;
	case SYNTAX_STRING: return COLOR_STRING;
	case SYNTAX_CHARACTER: return COLOR_CHARACTER;
	case SYNTAX_CONSTANT: return COLOR_CONSTANT;
	}
	return COLOR_TEXT;
}

static void syntax_highlight_c(SyntaxState *state_ptr, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	SyntaxState state = *state_ptr;
	bool in_preprocessor = (state & SYNTAX_STATE_PREPROCESSOR) != 0;
	bool in_string = (state & SYNTAX_STATE_STRING) != 0;
	bool in_single_line_comment = (state & SYNTAX_STATE_SINGLE_LINE_COMMENT) != 0;
	bool in_multi_line_comment = (state & SYNTAX_STATE_MULTI_LINE_COMMENT) != 0;
	bool in_char = false;
	bool in_number = false;
	
	int backslashes = 0;
	for (u32 i = 0; i < line_len; ++i) {
		SyntaxCharType type = SYNTAX_NORMAL;
		// necessary for the final " of a string to be highlighted
		bool in_string_now = in_string;
		bool in_char_now = in_char;
		bool in_multi_line_comment_now = in_multi_line_comment;

		// are there 1/2 characters left in the line?
		bool has_1_char = i + 1 < line_len;

		bool dealt_with = false;

		switch (line[i]) {
		case '#':
			if (!in_single_line_comment && !in_multi_line_comment && !in_char && !in_string)
				in_preprocessor = true;
			break;
		case '\\':
			++backslashes;
			break;
		case '/':
			if (!in_multi_line_comment && !in_single_line_comment && !in_string && !in_char && has_1_char) {
				if (line[i + 1] == '/')
					in_single_line_comment = true; // //
				else if (line[i + 1] == '*')
					in_multi_line_comment = in_multi_line_comment_now = true; // /*
			} else if (in_multi_line_comment) {
				if (i && line[i - 1] == '*') {
					// */
					in_multi_line_comment = false;
				}
			}
			break;
		case '"':
			if (in_string && backslashes % 2 == 0)
				in_string = false;
			else if (!in_multi_line_comment && !in_single_line_comment && !in_char)
				in_string = in_string_now = true;
			break;
		case '\'':
			if (in_char && backslashes % 2 == 0)
				in_char = false;
			else if (!in_multi_line_comment && !in_single_line_comment && !in_string)
				in_char = in_char_now = true;
			break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': // don't you wish C had case ranges...
			// a number!
			if (!in_single_line_comment && !in_multi_line_comment && !in_string && !in_number && !in_char) {
				in_number = true;
				if (i) {
					if (line[i - 1] == '.') {
						// support .6, for example
						char_types[i - 1] = SYNTAX_CONSTANT;
					} else if (is32_ident(line[i - 1])) {
						// actually, this isn't a number. it's something like a*6* or u3*2*.
						in_number = false;
					}
				}
			}
			break;
		default: {
			if ((i && is32_ident(line[i - 1])) || !is32_ident(line[i]))
				break; // can't be a keyword on its own.
			
			// here are all the keywords!
			static char const *const keywords_A[11] = {"ATOMIC_ADDRESS_LOCK_FREE","ATOMIC_CHAR16_T_LOCK_FREE","ATOMIC_CHAR32_T_LOCK_FREE","ATOMIC_CHAR_LOCK_FREE","ATOMIC_FLAG_LOCK_FREE","ATOMIC_INT_LOCK_FREE","ATOMIC_LLONG_LOCK_FREE","ATOMIC_LONG_LOCK_FREE","ATOMIC_SHORT_LOCK_FREE","ATOMIC_WCHAR_T_LOCK_FREE"};
			static char const *const keywords_B[2] = {"BUFSIZ"};
			static char const *const keywords_C[5] = {"CHAR_BIT","CHAR_MAX","CHAR_MIN","CLOCKS_PER_SEC"};
			static char const *const keywords_D[12] = {"DBL_DIG","DBL_EPSILON","DBL_HAS_SUBNORM","DBL_MANT_DIG","DBL_MAX","DBL_MAX_10_EXP","DBL_MAX_EXP","DBL_MIN","DBL_MIN_EXP","DBL_TRUE_MIN","DECIMAL_DIG"};
			static char const *const keywords_E[128] = {"E2BIG","EACCES","EADDRINUSE","EADDRNOTAVAIL","EADV","EAFNOSUPPORT","EAGAIN","EALREADY","EBADE","EBADF","EBADFD","EBADMSG","EBADR","EBADRQC","EBADSLT","EBFONT","EBUSY","ECHILD","ECHRNG","ECOMM","ECONNABORTED","ECONNREFUSED","ECONNRESET","EDEADLK","EDEADLOCK","EDESTADDRREQ","EDOM","EDOTDOT","EDQUOT","EEXIST","EFAULT","EFBIG","EHOSTDOWN","EHOSTUNREACH","EIDRM","EILSEQ","EINPROGRESS","EINTR","EINVAL","EIO","EISCONN","EISDIR","EISNAM","EL2HLT","EL2NSYNC","EL3HLT","EL3RST","ELIBACC","ELIBBAD","ELIBEXEC","ELIBMAX","ELIBSCN","ELNRNG","ELOOP","EMEDIUMTYPE","EMFILE","EMLINK","EMSGSIZE","EMULTIHOP","ENAMETOOLONG","ENAVAIL","ENETDOWN","ENETRESET","ENETUNREACH","ENFILE","ENOANO","ENOBUFS","ENOCSI","ENODATA","ENODEV","ENOENT","ENOEXEC","ENOLCK","ENOLINK","ENOMEDIUM","ENOMEM","ENOMSG","ENONET","ENOPKG","ENOPROTOOPT","ENOSPC","ENOSR","ENOSTR","ENOSYS","ENOTBLK","ENOTCONN","ENOTDIR","ENOTEMPTY","ENOTNAM","ENOTSOCK","ENOTTY","ENOTUNIQ","ENXIO","EOF","EOPNOTSUPP","EOVERFLOW","EPERM","EPFNOSUPPORT","EPIPE","EPROTO","EPROTONOSUPPORT","EPROTOTYPE","ERANGE","EREMCHG","EREMOTE","EREMOTEIO","ERESTART","EROFS","ESHUTDOWN","ESOCKTNOSUPPORT","ESPIPE","ESRCH","ESRMNT","ESTALE","ESTRPIPE","ETIME","ETIMEDOUT","ETOOMANYREFS","ETXTBSY","EUCLEAN","EUNATCH","EUSERS","EWOULDBLOCK","EXDEV","EXFULL","EXIT_FAILURE","EXIT_SUCCESS"};
			static char const *const keywords_F[39] = {"FE_ALL_EXCEPT","FE_DFL_ENV","FE_DIVBYZERO","FE_DOWNWARD","FE_INEXACT","FE_INVALID","FE_OVERFLOW","FE_TONEAREST","FE_TOWARDZERO","FE_UNDERFLOW","FE_UPWARD","FILE","FILENAME_MAX","FLT_DECIMAL_DIG","FLT_DIG","FLT_EVAL_METHOD","FLT_HAS_SUBNORM","FLT_MANT_DIG","FLT_MAX","FLT_MAX_10_EXP","FLT_MAX_EXP","FLT_MIN","FLT_MIN_10_EXP","FLT_MIN_EXP","FLT_RADIX","FLT_ROUNDS","FLT_TRUE_MIN","FOPEN_MAX","FP_FAST_FMA","FP_FAST_FMAF","FP_FAST_FMAL","FP_ILOGB0","FP_ILOGBNAN","FP_INFINITE","FP_NAN","FP_NORMAL","FP_SUBNORMAL","FP_ZERO"};
			static char const *const keywords_H[4] = {"HUGE_VAL","HUGE_VALF","HUGE_VALL"};
			static char const *const keywords_I[33] = {"I","INFINITY","INT16_MAX","INT16_MIN","INT32_MAX","INT32_MIN","INT64_MAX","INT64_MIN","INT8_MAX","INT8_MIN","INTMAX_MAX","INTMAX_MIN","INTPTR_MAX","INTPTR_MIN","INT_FAST16_MAX","INT_FAST16_MIN","INT_FAST32_MAX","INT_FAST32_MIN","INT_FAST64_MAX","INT_FAST64_MIN","INT_FAST8_MAX","INT_FAST8_MIN","INT_LEAST16_MAX","INT_LEAST16_MIN","INT_LEAST32_MAX","INT_LEAST32_MIN","INT_LEAST64_MAX","INT_LEAST64_MIN","INT_LEAST8_MAX","INT_LEAST8_MIN","INT_MAX","INT_MIN"};
			static char const *const keywords_L[23] = {"LC_ALL","LC_COLLATE","LC_CTYPE","LC_MONETARY","LC_NUMERIC","LC_TIME","LDBL_DECIMAL_DIG","LDBL_DIG","LDBL_EPSILON","LDBL_MANT_DIG","LDBL_MAX","LDBL_MAX_10_EXP","LDBL_MAX_EXP","LDBL_MIN","LDBL_MIN_10_EXP","LDBL_MIN_EXP","LDBL_TRUE_MIN","LLONG_MAX","LLONG_MIN","LONG_MAX","LONG_MIN","L_tmpnam"};
			static char const *const keywords_M[5] = {"MATH_ERREXCEPT","MATH_ERRNO","MB_CUR_MAX","MB_LEN_MAX"};
			static char const *const keywords_N[4] = {"NAN","NDEBUG","NULL"};
			static char const *const keywords_O[2] = {"ONCE_FLAG_INIT"};
			static char const *const keywords_P[87] = {"PRIX16","PRIX32","PRIX64","PRIX8","PRIXFAST16","PRIXFAST32","PRIXFAST64","PRIXFAST8","PRIXLEAST16","PRIXLEAST32","PRIXLEAST64","PRIXLEAST8","PRIXMAX","PRIXPTR","PRId16","PRId32","PRId64","PRId8","PRIdFAST16","PRIdFAST32","PRIdFAST64","PRIdFAST8","PRIdLEAST16","PRIdLEAST32","PRIdLEAST64","PRIdLEAST8","PRIdMAX","PRIdPTR","PRIi16","PRIi32","PRIi64","PRIi8","PRIiFAST16","PRIiFAST32","PRIiFAST64","PRIiFAST8","PRIiLEAST16","PRIiLEAST32","PRIiLEAST64","PRIiLEAST8","PRIiMAX","PRIiPTR","PRIo16","PRIo32","PRIo64","PRIo8","PRIoFAST16","PRIoFAST32","PRIoFAST64","PRIoFAST8","PRIoLEAST16","PRIoLEAST32","PRIoLEAST64","PRIoLEAST8","PRIoMAX","PRIoPTR","PRIu16","PRIu32","PRIu64","PRIu8","PRIuFAST16","PRIuFAST32","PRIuFAST64","PRIuFAST8","PRIuLEAST16","PRIuLEAST32","PRIuLEAST64","PRIuLEAST8","PRIuMAX","PRIuPTR","PRIx16","PRIx32","PRIx64","PRIx8","PRIxFAST16","PRIxFAST32","PRIxFAST64","PRIxFAST8","PRIxLEAST16","PRIxLEAST32","PRIxLEAST64","PRIxLEAST8","PRIxMAX","PRIxPTR","PTRDIFF_MAX","PTRDIFF_MIN"};
			static char const *const keywords_R[2] = {"RSIZE_MAX"};
			static char const *const keywords_S[122] = {"SCHAR_MAX","SCHAR_MIN","SCNd16","SCNd32","SCNd64","SCNd8","SCNdFAST16","SCNdFAST32","SCNdFAST64","SCNdFAST8","SCNdLEAST16","SCNdLEAST32","SCNdLEAST64","SCNdLEAST8","SCNdMAX","SCNdPTR","SCNi16","SCNi32","SCNi64","SCNi8","SCNiFAST16","SCNiFAST32","SCNiFAST64","SCNiFAST8","SCNiLEAST16","SCNiLEAST32","SCNiLEAST64","SCNiLEAST8","SCNiMAX","SCNiPTR","SCNo16","SCNo32","SCNo64","SCNo8","SCNoFAST16","SCNoFAST32","SCNoFAST64","SCNoFAST8","SCNoLEAST16","SCNoLEAST32","SCNoLEAST64","SCNoLEAST8","SCNoMAX","SCNoPTR","SCNu16","SCNu32","SCNu64","SCNu8","SCNuFAST16","SCNuFAST32","SCNuFAST64","SCNuFAST8","SCNuLEAST16","SCNuLEAST32","SCNuLEAST64","SCNuLEAST8","SCNuMAX","SCNuPTR","SCNx16","SCNx32","SCNx64","SCNx8","SCNxFAST16","SCNxFAST32","SCNxFAST64","SCNxFAST8","SCNxLEAST16","SCNxLEAST32","SCNxLEAST64","SCNxLEAST8","SCNxMAX","SCNxPTR","SEEK_CUR","SEEK_END","SEEK_SET","SHRT_MAX","SHRT_MIN","SIGABRT","SIGALRM","SIGBUS","SIGCHLD","SIGCLD","SIGCONT","SIGEMT","SIGFPE","SIGHUP","SIGILL","SIGINFO","SIGINT","SIGIO","SIGIOT","SIGKILL","SIGLOST","SIGPIPE","SIGPOLL","SIGPROF","SIGPWR","SIGQUIT","SIGSEGV","SIGSTKFLT","SIGSTOP","SIGSYS","SIGTERM","SIGTRAP","SIGTSTP","SIGTTIN","SIGTTOU","SIGUNUSED","SIGURG","SIGUSR1","SIGUSR2","SIGVTALRM","SIGWINCH","SIGXCPU","SIGXFSZ","SIG_ATOMIC_MAX","SIG_ATOMIC_MIN","SIG_DFL","SIG_ERR","SIG_IGN","SIZE_MAX"};
			static char const *const keywords_T[5] = {"TIME_UTC","TMP_MAX","TMP_MAX_S","TSS_DTOR_ITERATIONS"};
			static char const *const keywords_U[20] = {"UCHAR_MAX","UINT16_MAX","UINT32_MAX","UINT64_MAX","UINT8_MAX","UINTMAX_MAX","UINTPTR_MAX","UINT_FAST16_MAX","UINT_FAST32_MAX","UINT_FAST64_MAX","UINT_FAST8_MAX","UINT_LEAST16_MAX","UINT_LEAST32_MAX","UINT_LEAST64_MAX","UINT_LEAST8_MAX","UINT_MAX","ULLONG_MAX","ULONG_MAX","USHRT_MAX"};
			static char const *const keywords_W[6] = {"WCHAR_MAX","WCHAR_MIN","WEOF","WINT_MAX","WINT_MIN"};
			static char const *const keywords__[14] = {"_Alignas","_Alignof","_Atomic","_Bool","_Complex","_Generic","_IOFBF","_IOLBF","_IONBF","_Imaginary","_Noreturn","_Static_assert","_Thread_local"};
			static char const *const keywords_a[43] = {"alignas","alignof","atomic_address","atomic_bool","atomic_char","atomic_char16_t","atomic_char32_t","atomic_flag","atomic_int","atomic_int_fast16_t","atomic_int_fast32_t","atomic_int_fast64_t","atomic_int_fast8_t","atomic_int_least16_t","atomic_int_least32_t","atomic_int_least64_t","atomic_int_least8_t","atomic_intmax_t","atomic_intptr_t","atomic_llong","atomic_long","atomic_ptrdiff_t","atomic_schar","atomic_short","atomic_size_t","atomic_uchar","atomic_uint","atomic_uint_fast16_t","atomic_uint_fast32_t","atomic_uint_fast64_t","atomic_uint_fast8_t","atomic_uint_least16_t","atomic_uint_least32_t","atomic_uint_least64_t","atomic_uint_least8_t","atomic_uintmax_t","atomic_uintptr_t","atomic_ullong","atomic_ulong","atomic_ushort","atomic_wchar_t","auto"};
			static char const *const keywords_b[3] = {"bool","break"};
			static char const *const keywords_c[12] = {"case","char","char16_t","char32_t","char8_t","clock_t","cnd_t","complex","const","constraint_handler_t","continue"};
			static char const *const keywords_d[6] = {"default","div_t","do","double","double_t"};
			static char const *const keywords_e[5] = {"else","enum","errno_t","extern"};
			static char const *const keywords_f[8] = {"false","fenv_t","fexcept_t","float","float_t","for","fpos_t"};
			static char const *const keywords_g[2] = {"goto"};
			static char const *const keywords_i[11] = {"if","imaxdiv_t","inline","int","int16_t","int32_t","int64_t","int8_t","intmax_t","intptr_t"};
			static char const *const keywords_j[2] = {"jmp_buf"};
			static char const *const keywords_l[4] = {"ldiv_t","lldiv_t","long"};
			static char const *const keywords_m[16] = {"math_errhandling","max_align_t","mbstate_t","memory_order","memory_order_acq_rel","memory_order_acquire","memory_order_consume","memory_order_relaxed","memory_order_release","memory_order_seq_cst","mtx_plain","mtx_recursive","mtx_t","mtx_timed","mtx_try"};
			static char const *const keywords_n[2] = {"noreturn"};
			static char const *const keywords_o[3] = {"offsetof","once_flag"};
			static char const *const keywords_p[2] = {"ptrdiff_t"};
			static char const *const keywords_r[5] = {"register","restrict","return","rsize_t"};
			static char const *const keywords_s[13] = {"short","sig_atomic_t","signed","size_t","sizeof","static","static_assert","stderr","stdin","stdout","struct","switch"};
			static char const *const keywords_t[13] = {"thrd_busy","thrd_error","thrd_nomem","thrd_start_t","thrd_success","thrd_t","thrd_timeout","time_t","true","tss_dtor_t","tss_t","typedef"};
			static char const *const keywords_u[9] = {"uint16_t","uint32_t","uint64_t","uint8_t","uintmax_t","uintptr_t","union","unsigned"};
			static char const *const keywords_v[4] = {"va_list","void","volatile"};
			static char const *const keywords_w[6] = {"wchar_t","wctrans_t","wctype_t","while","wint_t"};
			static char const *const keywords_x[2] = {"xtime"};
			static char const *const *const all_keywords[] = {
				['A'] = keywords_A, ['B'] = keywords_B, ['C'] = keywords_C, ['D'] = keywords_D, ['E'] = keywords_E, ['F'] = keywords_F, ['H'] = keywords_H, ['I'] = keywords_I, ['L'] = keywords_L, ['M'] = keywords_M, ['N'] = keywords_N, ['O'] = keywords_O, ['P'] = keywords_P, ['R'] = keywords_R, ['S'] = keywords_S, ['T'] = keywords_T, ['U'] = keywords_U, ['W'] = keywords_W, ['_'] = keywords__, ['a'] = keywords_a, ['b'] = keywords_b, ['c'] = keywords_c, ['d'] = keywords_d, ['e'] = keywords_e, ['f'] = keywords_f, ['g'] = keywords_g, ['i'] = keywords_i, ['j'] = keywords_j, ['l'] = keywords_l, ['m'] = keywords_m, ['n'] = keywords_n, ['o'] = keywords_o, ['p'] = keywords_p, ['r'] = keywords_r, ['s'] = keywords_s, ['t'] = keywords_t, ['u'] = keywords_u, ['v'] = keywords_v, ['w'] = keywords_w, ['x'] = keywords_x
			};
			// keywords don't matter for advancing the state
			if (char_types && !in_single_line_comment && !in_multi_line_comment && !in_string && !in_preprocessor && !in_char) {
				u32 keyword_end;
				// find where this keyword would end (if this is a keyword)
				for (keyword_end = i; keyword_end < line_len && is32_ident(line[keyword_end]); ++keyword_end);
				
				u32 keyword_len = keyword_end - i;
				char const *const *keywords = line[i] < arr_count(all_keywords) ? all_keywords[line[i]] : NULL;
				if (keywords) {
					for (size_t k = 0; keywords[k]; ++k) {
						bool matches = true;
						char const *keyword = keywords[k];
						if (keyword_len == strlen(keyword)) {
							char32_t *p = &line[i];
							// check if `p` starts with `keyword`
							for (char const *q = keyword; *q; ++p, ++q) {
								if (*p != (char32_t)*q) {
									matches = false;
									break;
								}
							}
							if (matches) {
								// it's a keyword
								// let's highlight all of it now
								type = SYNTAX_KEYWORD;
								if (isupper(keyword[0]) || 
									(keyword_len == 4 && streq(keyword, "true")) ||
									(keyword_len == 5 && streq(keyword, "false")) ||
									(keyword_len == 6 && (streq(keyword, "stderr") || streq(keyword, "stdout")))
									) {
									type = SYNTAX_CONSTANT; // these are constants, not keywords
									}
								for (size_t c = 0; keyword[c]; ++c) {
									char_types[i++] = type;
								}
								--i; // we'll increment i from the for loop
								dealt_with = true;
								break;
							}
						}
					}
				}
			}
		} break;
		}
		if (line[i] != '\\') backslashes = 0;
		if (in_number && !(is32_digit(line[i]) || line[i] == '.'
			|| (line[i] < CHAR_MAX && strchr("xXoObBlLuUabcdefABCDEF", (char)line[i]))
 			|| (i && line[i-1] == 'e' && (line[i] == '+' || line[i] == '-')))) {
			in_number = false;
		}

		if (char_types && !dealt_with) {
			if (in_single_line_comment || in_multi_line_comment_now)
				type = SYNTAX_COMMENT;
			else if (in_string_now)
				type = SYNTAX_STRING;
			else if (in_char_now)
				type = SYNTAX_CHARACTER;
			else if (in_number)
				type = SYNTAX_CONSTANT;
			else if (in_preprocessor)
				type = SYNTAX_PREPROCESSOR;

			char_types[i] = type;
		}
	}
	*state_ptr = (SyntaxState)(
		  (backslashes && in_single_line_comment) << SYNTAX_STATE_SINGLE_LINE_COMMENT_SHIFT
		| (backslashes && in_preprocessor) << SYNTAX_STATE_PREPROCESSOR_SHIFT
		| (backslashes && in_string) << SYNTAX_STATE_STRING_SHIFT
		| in_multi_line_comment << SYNTAX_STATE_MULTI_LINE_COMMENT_SHIFT);
}

// This is the main syntax highlighting function. It will determine which colors to use for each character.
// Rather than returning colors, it returns a character type (e.g. comment) which can be converted to a color.
// To highlight multiple lines, start out with a zeroed SyntaxState, and pass a pointer to it each time.
// You can set char_types to NULL if you just want to advance the state, and don't care about the character types.
void syntax_highlight(SyntaxState *state, Language lang, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	switch (lang) {
	case LANG_NONE:
		memset(char_types, 0, line_len * sizeof *char_types);
		break;
	case LANG_C:
		syntax_highlight_c(state, line, line_len, char_types);
		break;
	case LANG_COUNT: assert(0); break;
	}
}
