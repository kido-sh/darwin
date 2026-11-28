/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __XCSELECT_PRIVATE_H__
#define __XCSELECT_PRIVATE_H__

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>

#include <xcselect.h>

typedef struct xcselect_manpaths xcselect_manpaths;

__BEGIN_DECLS

const char *xcselect_get_version(void);

bool xcselect_get_developer_dir_path(char *buffer, int buffer_size,
                                     bool *was_environment,
                                     bool *is_command_line_tools,
                                     bool *was_default);

bool xcselect_developer_dir_matches_path(const char *developer_dir,
                                         const char *path);

bool xcselect_find_developer_contents_from_path(const char *path,
                                                char *buffer, int buffer_size,
                                                bool *is_command_line_tools);

bool xcselect_bundle_is_developer_tool(const char *bundle_id);

xcselect_manpaths *xcselect_get_manpaths(const char *sdkname);
uint32_t xcselect_manpaths_get_num_paths(xcselect_manpaths *manpaths);
const char *xcselect_manpaths_get_path(xcselect_manpaths *manpaths,
                                       uint32_t idx);
void xcselect_manpaths_free(xcselect_manpaths *manpaths);

void xcselect_invoke_xcrun(const char *tool_name, int argc, char *argv[],
                           bool requires_xcode);

bool xcselect_trigger_install_request(const char *tool_name);

__END_DECLS

#endif /* __XCSELECT_PRIVATE_H__ */
