#ifdef HAVE_BFD_H
#include <bfd.h>

struct output_buffer
{
	char * buf;
	size_t sz;
	size_t ptr;
};

struct bfd_ctx
{
	bfd * handle;
	asymbol ** symbol;
};

struct bfd_set
{
	char * name;
	struct bfd_ctx * bc;
	struct bfd_set *next;
};

struct find_info
{
	asymbol **symbol;
	bfd_vma counter;
	const char *file;
	const char *func;
	unsigned line;
};

void output_init(struct output_buffer *ob, char * buf, size_t sz);
void find(struct bfd_ctx * b, size_t offset, const char **file, const char **func, unsigned *line);
struct bfd_ctx *get_bc(struct output_buffer *ob, struct bfd_set *set, const char *procname);
void release_set(struct bfd_set *set);
void output_print(struct output_buffer *ob, const char * format, ...);

#endif
