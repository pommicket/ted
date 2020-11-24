// Text buffers - These store the contents of a file.
// To make inserting characters faster, the file contents are split up into
// blocks of a few thousand bytes. Block boundaries are not necessarily
// on character boundaries, so one block might have part of a UTF-8 character
// with the next block having the rest.
#include "util.c"

#define TEXT_BLOCK_MAX_SIZE 400
// Once two adjacent blocks are at most this big, combine them into
// one bigger text block.
#define TEXT_BLOCK_COMBINE_SIZE (3*(TEXT_BLOCK_MAX_SIZE / 4))
// A good size for text blocks to be. When a file is loaded, there
// shouldn't be just full blocks, because they will all immediately split.
// Instead, we use blocks of this size.
#define TEXT_BLOCK_DEFAULT_SIZE (3*(TEXT_BLOCK_MAX_SIZE / 4))
typedef struct {
	u16 len;
	char *contents;
} TextBlock;

typedef struct {
	u32 nblocks; // number of text blocks
	TextBlock *blocks;
} TextBuffer;

// Returns a new block added at index `where`,
// or NULL if there's not enough memory.
static TextBlock *text_buffer_add_block(TextBuffer *buffer, u32 where) {
	// make sure we can actually allocate enough memory for the contents of the block
	// before doing anything
	char *block_contents = calloc(1, TEXT_BLOCK_MAX_SIZE);
	if (!block_contents) {
		return NULL;
	}

	u32 nblocks_before = buffer->nblocks;
	u32 nblocks_after = buffer->nblocks + 1;
	if (util_is_power_of_2(nblocks_after)) {
		// whenever the number of blocks is a power of 2, we need to grow the blocks array.
		buffer->blocks = realloc(buffer->blocks, 2 * nblocks_after * sizeof *buffer->blocks);
		if (!buffer->blocks) {
			// out of memory
			free(block_contents);
			return NULL;
		}
	}
	if (where != nblocks_before) { // if we aren't just inserting the block at the end,
		// make space for the new block
		memmove(buffer->blocks + (where + 1), buffer->blocks + where, nblocks_before - where);
	}
	
	TextBlock *block = buffer->blocks + where;
	util_zero_memory(block, sizeof *buffer->blocks); // zero everything
	block->contents = block_contents;

	
	buffer->nblocks = nblocks_after;
	return block;
}

Status text_buffer_load_file(TextBuffer *buffer, FILE *fp) {
	char block_contents[TEXT_BLOCK_DEFAULT_SIZE];
	size_t bytes_read;

	util_zero_memory(buffer, sizeof *buffer);
	
	// read file one block at a time
	do {
		bytes_read = fread(block_contents, 1, sizeof block_contents, fp);
		if (bytes_read > 0) {
			TextBlock *block = text_buffer_add_block(buffer, buffer->nblocks);
			if (!block) {
				return false;
			}
			memcpy(block->contents, block_contents, bytes_read);
			block->len = (u16)bytes_read;
		}
	} while (bytes_read == sizeof block_contents);
	if (ferror(fp))	
		return false;
	return true;
}

static void text_buffer_print_internal(TextBuffer *buffer, bool debug) {
	printf("\033[2J\033[;H");
	TextBlock *blocks = buffer->blocks;
	u32 nblocks = buffer->nblocks;
	
	for (u32 i = 0; i < nblocks; ++i) {
		TextBlock *block = &blocks[i];
		fwrite(block->contents, 1, block->len, stdout);
		if (debug)
			printf("\n\n------\n\n"); // NOTE: could be bad with UTF-8
	}
	fflush(stdout);
}

// print the contents of a buffer to stdout
void text_buffer_print(TextBuffer *buffer) {
	text_buffer_print_internal(buffer, false);
}

// print the contents of a buffer to stdout, along with debugging information
void text_buffer_print_debug(TextBuffer *buffer) {
	text_buffer_print_internal(buffer, true);
}

// Does not free the pointer `buffer` (buffer might not have even been allocated with malloc)
void text_buffer_free(TextBuffer *buffer) {
	TextBlock *blocks = buffer->blocks;
	u32 nblocks = buffer->nblocks;
	for (u32 i = 0; i < nblocks; ++i) {
		free(blocks[i].contents);
	}
	free(blocks);
}
