/*
 * Minimal gzopen/gzgets/gzclose for AmigaOS3 vbcc.
 *
 * z.library does not export gzip file I/O. SAS/C zlibstubs.lib cannot be
 * linked by vbcc (@ vs _ symbol names), so inflate* come from a vbcc build
 * of those stubs; these gz* helpers cover Messages loading in hashtable.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

/* zlib.h already defines a minimal struct gzFile_s for gzgetc; keep ours private */
struct ami_gzFile {
	FILE *fp;
	unsigned char *data;
	size_t len;
	size_t pos;
};

static int
read_whole_file(FILE *fp, unsigned char **out, size_t *out_len)
{
	unsigned char *buf;
	unsigned char *nbuf;
	size_t cap;
	size_t len;
	size_t n;

	cap = 4096;
	len = 0;
	buf = (unsigned char *)malloc(cap);
	if (buf == NULL) {
		return -1;
	}

	while ((n = fread(buf + len, 1, cap - len, fp)) > 0) {
		len += n;
		if (len == cap) {
			cap *= 2;
			nbuf = (unsigned char *)realloc(buf, cap);
			if (nbuf == NULL) {
				free(buf);
				return -1;
			}
			buf = nbuf;
		}
	}

	if (ferror(fp)) {
		free(buf);
		return -1;
	}

	*out = buf;
	*out_len = len;
	return 0;
}

static int
gunzip_buffer(const unsigned char *src, size_t src_len,
	unsigned char **out, size_t *out_len)
{
	z_stream strm;
	unsigned char *buf;
	unsigned char *nbuf;
	size_t cap;
	int ret;

	memset(&strm, 0, sizeof(strm));
	strm.next_in = (Bytef *)src;
	strm.avail_in = (uInt)src_len;

	ret = inflateInit2(&strm, 32 + MAX_WBITS);
	if (ret != Z_OK) {
		return -1;
	}

	cap = src_len * 4;
	if (cap < 4096) {
		cap = 4096;
	}
	buf = (unsigned char *)malloc(cap);
	if (buf == NULL) {
		inflateEnd(&strm);
		return -1;
	}

	strm.next_out = buf;
	strm.avail_out = (uInt)cap;

	for (;;) {
		ret = inflate(&strm, Z_NO_FLUSH);
		if (ret == Z_STREAM_END) {
			break;
		}
		if (ret != Z_OK) {
			free(buf);
			inflateEnd(&strm);
			return -1;
		}
		if (strm.avail_out == 0) {
			size_t used;

			used = (size_t)strm.total_out;
			cap *= 2;
			nbuf = (unsigned char *)realloc(buf, cap);
			if (nbuf == NULL) {
				free(buf);
				inflateEnd(&strm);
				return -1;
			}
			buf = nbuf;
			strm.next_out = buf + used;
			strm.avail_out = (uInt)(cap - used);
		}
	}

	*out_len = (size_t)strm.total_out;
	*out = buf;
	inflateEnd(&strm);
	return 0;
}

gzFile gzopen(const char *path, const char *mode)
{
	struct ami_gzFile *gz;
	FILE *fp;
	unsigned char hdr[2];
	unsigned char *raw;
	size_t raw_len;
	size_t n;

	(void)mode;

	gz = (struct ami_gzFile *)calloc(1, sizeof(*gz));
	if (gz == NULL) {
		return NULL;
	}

	fp = fopen(path, "rb");
	if (fp == NULL) {
		free(gz);
		return NULL;
	}

	n = fread(hdr, 1, 2, fp);
	if (n == 2 && hdr[0] == 0x1f && hdr[1] == 0x8b) {
		rewind(fp);
		if (read_whole_file(fp, &raw, &raw_len) != 0) {
			fclose(fp);
			free(gz);
			return NULL;
		}
		fclose(fp);
		if (gunzip_buffer(raw, raw_len, &gz->data, &gz->len) != 0) {
			free(raw);
			free(gz);
			return NULL;
		}
		free(raw);
		gz->pos = 0;
		return (gzFile)gz;
	}

	/* Plain text Messages (typical Amiga install) */
	rewind(fp);
	gz->fp = fp;
	return (gzFile)gz;
}

char *gzgets(gzFile file, char *buf, int len)
{
	struct ami_gzFile *gz;
	int i;
	unsigned char c;

	if (file == NULL || buf == NULL || len <= 0) {
		return NULL;
	}

	gz = (struct ami_gzFile *)file;
	if (gz->fp != NULL) {
		return fgets(buf, len, gz->fp);
	}

	if (gz->data == NULL || gz->pos >= gz->len) {
		return NULL;
	}

	i = 0;
	while (i < len - 1 && gz->pos < gz->len) {
		c = gz->data[gz->pos++];
		buf[i++] = (char)c;
		if (c == '\n') {
			break;
		}
	}
	buf[i] = '\0';
	return buf;
}

int gzclose(gzFile file)
{
	struct ami_gzFile *gz;
	int rc;

	if (file == NULL) {
		return Z_STREAM_ERROR;
	}

	gz = (struct ami_gzFile *)file;
	rc = Z_OK;
	if (gz->fp != NULL) {
		if (fclose(gz->fp) != 0) {
			rc = Z_ERRNO;
		}
	}
	free(gz->data);
	free(gz);
	return rc;
}
