#include <common.h>
#include <fs.h>

#define OVERLAY_PART "1:3"

int read_file(const char* filename, char *buf, int size)
{
	loff_t filesize = 0;
	loff_t len;
	int ret;

	/* If consoldev is set take this as productive conosle instead of default console */
	if (fs_set_blk_dev("mmc", OVERLAY_PART, FS_TYPE_EXT) != 0) {
		puts("Error, can not set blk device\n");
		return -1;
	}


	/* File does not exist, do not print an error message */
	if (fs_size(filename, &filesize)) {
		return -1;
	}

	if (filesize < size)
		size = filesize;

	/* If consoldev is set take this as productive conosle instead of default console */
	if (fs_set_blk_dev("mmc", OVERLAY_PART, FS_TYPE_EXT) != 0) {
		puts("Error, can not set blk device\n");
		return -1;
	}


	if ((ret = fs_read(filename, (ulong)buf, 0, size, &len))) {
		printf("Can't read file %s (size %d, len %lld, ret %d)\n", filename, size, len, ret);
		return -1;
	}

	buf[len] = 0;

	return len;
}

