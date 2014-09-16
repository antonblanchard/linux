/*
 * Support for Partition Mobility/Migration
 *
 * Copyright (C) 2010 Nathan Fontenot
 * Copyright (C) 2010 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/smp.h>
#include <linux/stat.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <asm/machdep.h>
#include <asm/rtas.h>
#include "pseries.h"

static struct kobject *mobility_kobj;

struct update_props_workarea {
	u32 phandle;
	u32 state;
	u64 reserved;
	u32 nprops;
} __packed;

#define NODE_ACTION_MASK	0xff000000
#define NODE_COUNT_MASK		0x00ffffff

#define DELETE_DT_NODE	0x01000000
#define UPDATE_DT_NODE	0x02000000
#define ADD_DT_NODE	0x03000000

#define MIGRATION_SCOPE	(1)

static int mobility_rtas_call(int token, __be32 *buf, s32 scope)
{
	int rc;

	spin_lock(&rtas_data_buf_lock);

	memcpy(rtas_data_buf, buf, RTAS_DATA_BUF_SIZE);
	rc = rtas_call(token, 2, 1, NULL, rtas_data_buf, scope);
	memcpy(buf, rtas_data_buf, RTAS_DATA_BUF_SIZE);

	spin_unlock(&rtas_data_buf_lock);
	return rc;
}

static int delete_dt_node(u32 phandle)
{
	struct device_node *dn;

	dn = of_find_node_by_phandle(phandle);
	if (!dn)
		return -ENOENT;

	dlpar_detach_node(dn);
	of_node_put(dn);
	return 0;
}

static int update_dt_property(struct device_node *dn, struct property **prop,
			      const char *name, int length, char *value)
{
	struct property *new_prop = *prop;

	if (new_prop) {
		/* partial property fixup */
		char *new_data = kzalloc(new_prop->length + length, GFP_KERNEL | __GFP_NOWARN);
		if (!new_data)
			return -ENOMEM;

		memcpy(new_data, new_prop->value, new_prop->length);
		memcpy(new_data + new_prop->length, value, length);

		kfree(new_prop->value);
		new_prop->value = new_data;
		new_prop->length += length;
	} else {
		new_prop = kzalloc(sizeof(*new_prop), GFP_KERNEL);
		if (!new_prop)
			return -ENOMEM;

		new_prop->name = kstrdup(name, GFP_KERNEL | __GFP_NOWARN);
		if (!new_prop->name) {
			kfree(new_prop);
			return -ENOMEM;
		}

		new_prop->length = length;
		new_prop->value = kzalloc(new_prop->length, GFP_KERNEL | __GFP_NOWARN);
		if (!new_prop->value) {
			kfree(new_prop->name);
			kfree(new_prop);
			return -ENOMEM;
		}

		memcpy(new_prop->value, value, length);
		*prop = new_prop;
	}

	return 0;
}

static int update_dt_node(u32 phandle, s32 scope)
{
	struct update_props_workarea upwa;
	struct device_node *dn;
	struct property *prop = NULL;
	int i, rc, rtas_rc;
	int update_properties_token;
	char *prop_data;
	u32 vd;
	__be32 *rtas_buf;

	update_properties_token = rtas_token("ibm,update-properties");
	if (update_properties_token == RTAS_UNKNOWN_SERVICE)
		return -EINVAL;

	rtas_buf = kzalloc(RTAS_DATA_BUF_SIZE, GFP_KERNEL);
	if (!rtas_buf)
		return -ENOMEM;

	dn = of_find_node_by_phandle(phandle);
	if (!dn) {
		kfree(rtas_buf);
		return -ENOENT;
	}

	*rtas_buf = cpu_to_be32(phandle);
	do {
		rtas_rc = mobility_rtas_call(update_properties_token, rtas_buf,
					scope);
		if (rtas_rc < 0)
			break;
		upwa.phandle = be32_to_cpu(*rtas_buf);
		upwa.state = be32_to_cpu(*(rtas_buf + 1));
		upwa.reserved = be64_to_cpu(*((__be64 *)(rtas_buf + 2)));
		upwa.nprops = be32_to_cpu(*(rtas_buf + 4));
		prop_data = ((char *)rtas_buf) + sizeof(upwa);

		/* On the first call to ibm,update-properties for a node the
		 * the first property value descriptor contains an empty
		 * property name, the property value length encoded as u32,
		 * and the property value is the node path being updated.
		 */
		if (*prop_data == 0) {
			prop_data += sizeof(u32);
			vd = be32_to_cpu(*(__be32 *)prop_data);
			prop_data += vd + sizeof(vd);
			upwa.nprops--;
		}

		for (i = 0; i < upwa.nprops; i++) {
			char *prop_name;

			prop_name = prop_data;
			prop_data += strlen(prop_name) + 1;
			vd = be32_to_cpu(*(__be32 *)prop_data);
			prop_data += sizeof(vd);

			switch (vd) {
			case 0x00000000:
				/* name only property, nothing to do */
				break;

			case 0x80000000:
				prop = of_find_property(dn, prop_name, NULL);
				of_remove_property(dn, prop);
				prop = NULL;
				break;

			default:
				/* A negative 'vd' value indicates that only part of the new property
				 * value is contained in the buffer and we need to call
				 * ibm,update-properties again to get the rest of the value.
				 *
				 * A negative value is also the two's compliment of the actual value.
				 */

				rc = update_dt_property(dn, &prop, prop_name,
							vd & 0x80000000 ? ~vd + 1 : vd, prop_data);
				if (rc) {
					printk(KERN_ERR "Could not update %s property\n",
					       prop_name);
					/* Could try to continue but if the failure was for a section
					 * of a node it gets too easy to mess up the device tree.
					 * Plus, ENOMEM likely means we have bigger problems than a
					 * failed device tree update */
					if (prop) {
						kfree(prop->name);
						kfree(prop->value);
						kfree(prop);
						prop = NULL;
					}
					i = upwa.nprops - 1; /* Break */
				}

				if (prop && !(vd & 0x80000000)) {
					of_update_property(dn, prop);
					prop = NULL;
				}
				prop_data += vd & 0x80000000 ? ~vd + 1 : vd;
			}

			if (prop_data - (char *)rtas_buf >= RTAS_DATA_BUF_SIZE) {
				printk(KERN_ERR "Device tree property"
				       " length exceeds rtas buffer\n");
				rc = -EOVERFLOW;
				goto update_dt_node_err;
			}
		}
	} while (rtas_rc == 1);

	rc = rtas_rc;
	of_node_put(dn);
update_dt_node_err:
	kfree(rtas_buf);
	return rc;
}

static int add_dt_node(u32 parent_phandle, u32 drc_index)
{
	struct device_node *dn;
	struct device_node *parent_dn;
	int rc;

	parent_dn = of_find_node_by_phandle(parent_phandle);
	if (!parent_dn)
		return -ENOENT;

	dn = dlpar_configure_connector(drc_index, parent_dn);
	if (!dn)
		return -ENOENT;

	rc = dlpar_attach_node(dn);
	if (rc)
		dlpar_free_cc_nodes(dn);

	of_node_put(parent_dn);
	return rc;
}

int pseries_devicetree_update(s32 scope)
{
	__be32 *rtas_buf;
	int update_nodes_token;
	int rc;
	__be32 *data;
	u32 node;

	update_nodes_token = rtas_token("ibm,update-nodes");
	if (update_nodes_token == RTAS_UNKNOWN_SERVICE)
		return -EINVAL;

	rtas_buf = kzalloc(RTAS_DATA_BUF_SIZE, GFP_KERNEL);
	if (!rtas_buf)
		return -ENOMEM;

	do {
		rc = mobility_rtas_call(update_nodes_token, rtas_buf, scope);
		if (rc && rc != 1)
			break;
		data = rtas_buf + 4;
		node = be32_to_cpu(*data++);

		while (node & NODE_ACTION_MASK) {
			int i;
			u32 action = node & NODE_ACTION_MASK;
			int node_count = node & NODE_COUNT_MASK;

			for (i = 0; i < node_count; i++) {
				u32 phandle;
				u32 drc_index;

				if (data + 1 - rtas_buf >= RTAS_DATA_BUF_SIZE) {
					printk(KERN_ERR "Device tree property"
					       " length exceeds rtas buffer\n");
					rc = -EOVERFLOW;
					goto pseries_devicetree_update_err;
				}
				phandle = be32_to_cpu(*data++);

				switch (action) {
				case DELETE_DT_NODE:
					delete_dt_node(phandle);
					break;
				case UPDATE_DT_NODE:
					update_dt_node(phandle, scope);
					break;
				case ADD_DT_NODE:
					drc_index = be32_to_cpu(*data++);
					add_dt_node(phandle, drc_index);
					break;
				default:
					/* Bogus action */
					i = node_count - 1; /* Break */
					data += node_count;
				}
			}
			if (data - rtas_buf >= RTAS_DATA_BUF_SIZE) {
				printk(KERN_ERR "Number of device tree update "
				       "nodes exceeds rtas buffer length\n");
				rc = -EOVERFLOW;
				goto pseries_devicetree_update_err;
			}
			node = be32_to_cpu(*data++);
		}
	} while (rc == 1);

pseries_devicetree_update_err:
	kfree(rtas_buf);
	return rc;
}

void post_mobility_fixup(void)
{
	int rc;
	int activate_fw_token;

	activate_fw_token = rtas_token("ibm,activate-firmware");
	if (activate_fw_token == RTAS_UNKNOWN_SERVICE) {
		printk(KERN_ERR "Could not make post-mobility "
		       "activate-fw call.\n");
		return;
	}

	do {
		rc = rtas_call(activate_fw_token, 0, 1, NULL);
	} while (rtas_busy_delay(rc));

	if (rc)
		printk(KERN_ERR "Post-mobility activate-fw failed: %d\n", rc);

	rc = pseries_devicetree_update(MIGRATION_SCOPE);
	if (rc)
		printk(KERN_ERR "Post-mobility device tree update "
			"failed: %d\n", rc);

	return;
}

static ssize_t migrate_store(struct class *class, struct class_attribute *attr,
			     const char *buf, size_t count)
{
	struct rtas_args args;
	u64 streamid;
	int rc;

	rc = kstrtou64(buf, 0, &streamid);
	if (rc)
		return rc;

	memset(&args, 0, sizeof(args));
	args.token = rtas_token("ibm,suspend-me");
	args.nargs = 2;
	args.nret = 1;

	args.args[0] = streamid >> 32 ;
	args.args[1] = streamid & 0xffffffff;
	args.rets = &args.args[args.nargs];

	do {
		args.rets[0] = 0;
		rc = rtas_ibm_suspend_me(&args);
		if (!rc && args.rets[0] == RTAS_NOT_SUSPENDABLE)
			ssleep(1);
	} while (!rc && args.rets[0] == RTAS_NOT_SUSPENDABLE);

	if (rc)
		return rc;
	else if (args.rets[0])
		return args.rets[0];

	post_mobility_fixup();
	return count;
}

static CLASS_ATTR(migration, S_IWUSR, NULL, migrate_store);

static int __init mobility_sysfs_init(void)
{
	int rc;

	mobility_kobj = kobject_create_and_add("mobility", kernel_kobj);
	if (!mobility_kobj)
		return -ENOMEM;

	rc = sysfs_create_file(mobility_kobj, &class_attr_migration.attr);

	return rc;
}
machine_device_initcall(pseries, mobility_sysfs_init);
