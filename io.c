static void write_u8(FILE *fp, u8 x) {
	putc(x, fp);
}

static void write_u16(FILE *fp, u16 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_u32(FILE *fp, u32 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_u64(FILE *fp, u64 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_i8(FILE *fp, i8 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_i16(FILE *fp, i16 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_i32(FILE *fp, i32 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_i64(FILE *fp, i64 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_float(FILE *fp, float x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_double(FILE *fp, double x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_char(FILE *fp, char x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_bool(FILE *fp, bool x) {
	putc(x, fp);
}

static u8 read_u8(FILE *fp) {
	return (u8)getc(fp);
}

static u16 read_u16(FILE *fp) {
	u16 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static u32 read_u32(FILE *fp) {
	u32 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static u64 read_u64(FILE *fp) {
	u64 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static i8 read_i8(FILE *fp) {
	i8 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static i16 read_i16(FILE *fp) {
	i16 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static i32 read_i32(FILE *fp) {
	i32 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static i64 read_i64(FILE *fp) {
	i64 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static float read_float(FILE *fp) {
	float x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static double read_double(FILE *fp) {
	double x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static char read_char(FILE *fp) {
	char x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static bool read_bool(FILE *fp) {
	return (bool)getc(fp);
}

static void write_cstr(FILE *fp, char const *cstr) {
	fwrite(cstr, 1, strlen(cstr) + 1, fp);
}

static void read_cstr(FILE *fp, char *out, size_t out_sz) {
	char *p = out, *end = out + out_sz;
	while (1) {
		if (p >= end - 1) {
			*p = '\0';
			break;
		}
		int c = getc(fp);
		if (c == 0 || c == EOF) {
			*p = '\0';
			break;
		}
		*p++ = (char)c;
	}
}
