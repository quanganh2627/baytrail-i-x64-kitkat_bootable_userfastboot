/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (C) 2011 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>

#include <cutils/properties.h>
#include <fs_mgr.h>

#include <inttypes.h>
#include "fastboot.h"
#include "userfastboot_fstab.h"
#include "userfastboot.h"
#include "userfastboot_ui.h"
#include "userfastboot_util.h"

static struct fstab *fstab = NULL;

void load_volume_table()
{
	int i;
	int ret;

	fstab = fs_mgr_read_fstab("/etc/recovery.fstab");
	if (!fstab) {
		pr_error("failed to read /etc/recovery.fstab\n");
		return;
	}

	ret = fs_mgr_add_entry(fstab, "/tmp", "ramdisk", "ramdisk", 0);
	if (ret < 0) {
		pr_error("failed to add /tmp entry to fstab\n");
		fs_mgr_free_fstab(fstab);
		fstab = NULL;
		return;
	}

	pr_debug("recovery filesystem table\n");
	pr_debug("=========================\n");
	for (i = 0; i < fstab->num_entries; ++i) {
		struct fstab_rec *v = &fstab->recs[i];
		pr_debug("  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
			 v->blk_device, v->length);
	}
	printf("\n");
}

struct fstab_rec *volume_for_path(const char *path)
{
	return fs_mgr_get_entry_for_mount_point(fstab, path);
}

struct fstab_rec *volume_for_name(const char *name)
{
	char *pat;
	struct fstab_rec *vol;
	/* Historical: it's /data in recovery.fstab, but some fastboot
	 * options (such as -w) expect it to be called userdata */
	if (!strcmp("userdata", name))
		name = "data";
	/* recovery.fstab entries are all prefixed with '/' */
	pat = xasprintf("/%s", name);
	vol = volume_for_path(pat);
	free(pat);
	return vol;
}
static void publish_part_data(bool wait, struct fstab_rec *v, char *name)
{
	uint64_t size;
	struct stat sb;
	int ctr = 15;

	/* Keep trying for ctr seconds for the device node to show up.
	 * ueventd may be busy creating the node */
	while (wait && ctr-- && stat(v->blk_device, &sb)) {
		pr_debug("waiting for %s\n", v->blk_device);
		sleep(1);
	}


	if (get_volume_size(v, &size)) {
		if (wait)
			pr_error("Couldn't get %s volume size\n",
				xasprintf("partition-size:%s", name));
		fastboot_publish(xasprintf("partition-size:%s", name),
				xstrdup("0x0"));
	} else {
		fastboot_publish(xasprintf("partition-size:%s", name),
				xasprintf("0x%" PRIx64, size));
	}
}

void publish_all_part_data(bool wait, char * part)
{
	int i;
	for (i = 0; i < fstab->num_entries; i++) {
		struct fstab_rec *v = &fstab->recs[i];

		if (!strcmp(part , v->mount_point))
			{
			if(!strcmp(part,"/data"))
			publish_part_data(wait, v, "userdata");
			else if(!strcmp(part,"/cache"))
			publish_part_data(wait, v, "cache");
			}
	}
}
