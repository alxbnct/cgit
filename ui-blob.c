/* ui-blob.c: show blob content
 *
 * Copyright (C) 2008 Lars Hjemli
 * Copyright (C) 2010 Jason A. Donenfeld <Jason@zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#include "cgit.h"
#include "ui-blob.h"
#include "html.h"
#include "ui-shared.h"

struct walk_tree_context {
	char *match_path;
	unsigned char *matched_sha1;
	int found_path;
};

static int walk_tree(const unsigned char *sha1, const char *base, int baselen,
	const char *pathname, unsigned mode, int stage, void *cbdata)
{
	struct walk_tree_context *walk_tree_ctx = cbdata;

	if (strncmp(base, walk_tree_ctx->match_path, baselen)
		|| strcmp(walk_tree_ctx->match_path + baselen, pathname))
		return READ_TREE_RECURSIVE;
	memmove(walk_tree_ctx->matched_sha1, sha1, 20);
	walk_tree_ctx->found_path = 1;
	return 0;
}

int cgit_print_file(char *path, const char *head)
{
	unsigned char sha1[20];
	enum object_type type;
	char *buf;
	unsigned long size;
	struct commit *commit;
	struct pathspec_item path_items = {
		.match = path,
		.len = strlen(path)
	};
	struct pathspec paths = {
		.nr = 1,
		.items = &path_items
	};
	struct walk_tree_context walk_tree_ctx = {
		.match_path = path,
		.matched_sha1 = sha1,
		.found_path = 0
	};

	if (get_sha1(head, sha1))
		return -1;
	type = sha1_object_info(sha1, &size);
	if (type == OBJ_COMMIT && path) {
		commit = lookup_commit_reference(sha1);
		read_tree_recursive(commit->tree, "", 0, 0, &paths, walk_tree, &walk_tree_ctx);
		if (!walk_tree_ctx.found_path)
			return -1;
		type = sha1_object_info(sha1, &size);
	}
	if (type == OBJ_BAD)
		return -1;
	buf = read_sha1_file(sha1, &type, &size);
	if (!buf)
		return -1;
	buf[size] = '\0';
	html_raw(buf, size);
	return 0;
}

void cgit_print_blob(const char *hex, char *path, const char *head)
{
	unsigned char sha1[20];
	enum object_type type;
	char *buf;
	unsigned long size;
	struct commit *commit;
	struct pathspec_item path_items = {
		.match = path,
		.len = path ? strlen(path) : 0
	};
	struct pathspec paths = {
		.nr = 1,
		.items = &path_items
	};
	struct walk_tree_context walk_tree_ctx = {
		.match_path = path,
		.matched_sha1 = sha1,
	};

	if (hex) {
		if (get_sha1_hex(hex, sha1)) {
			cgit_print_error(fmt("Bad hex value: %s", hex));
			return;
		}
	} else {
		if (get_sha1(head, sha1)) {
			cgit_print_error(fmt("Bad ref: %s", head));
			return;
		}
	}

	type = sha1_object_info(sha1, &size);

	if ((!hex) && type == OBJ_COMMIT && path) {
		commit = lookup_commit_reference(sha1);
		read_tree_recursive(commit->tree, "", 0, 0, &paths, walk_tree, &walk_tree_ctx);
		type = sha1_object_info(sha1,&size);
	}

	if (type == OBJ_BAD) {
		cgit_print_error(fmt("Bad object name: %s", hex));
		return;
	}

	buf = read_sha1_file(sha1, &type, &size);
	if (!buf) {
		cgit_print_error(fmt("Error reading object %s", hex));
		return;
	}

	buf[size] = '\0';
	ctx.page.mimetype = ctx.qry.mimetype;
	if (!ctx.page.mimetype) {
		if (buffer_is_binary(buf, size))
			ctx.page.mimetype = "application/octet-stream";
		else
			ctx.page.mimetype = "text/plain";
	}
	ctx.page.filename = path;
	cgit_print_http_headers(&ctx);
	html_raw(buf, size);
}
