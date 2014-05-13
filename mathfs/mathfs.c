/*
 * I've always wanted a file that magically emits prime numbers.
 * THIS IS THE DAY!
 * 
 * Copyright 2014, Chad Williamson <chad@dahc.us>
 *
 *   based on the lwnfs demo at http://lwn.net/Articles/57369/
 *
 *   Copyright 2002, 2003 Jonathan Corbet <corbet-AT-lwn.net>
 *
 * This file may be redistributed under the terms of the GNU GPL.
 *
 * Chances are that this code will crash your system, delete your
 * nethack high scores, and set your disk drives on fire.  You have
 * been warned.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chad Williamson");

#define MATHFS_MAGIC 0x20140513

/*
 * TODO: Replace this with a spin-lock-protected struct. This isn't
 * re-entrant as it sits...
 */
static atomic_t prime;

/*
 * Check for primality.
 */
static int is_prime(long p)
{
	long n;

	for (n = 2; n < p/2 + 1; n++)
		if (p%n == 0)
			return 0;
	return 1;
}

/*
 * Open a file.  All we have to do here is to copy over a
 * copy of the counter pointer so it's easier to get at.
 */
static int mathfs_open(struct inode *inode, struct file *filp)
{
	if (inode->i_ino > 1)
		return -ENODEV;  /* Should never happen.  */
	filp->private_data = &prime;
	return 0;
}

#define TMPSIZE 20
/*
 * Read a file.  Here we increment and read the counter, then pass it
 * back to the caller.  The increment only happens if the read is done
 * at the beginning of the file (offset = 0); otherwise we end up counting
 * by twos.
 */
static ssize_t mathfs_read_file(struct file *filp, char *buf,
		size_t count, loff_t *offset)
{
	int v, len;
	char tmp[TMPSIZE];
	atomic_t *counter = (atomic_t *) filp->private_data;
/*
 * Encode the value, and figure out how much of it we can pass back.
 */
	v = atomic_read(counter);
	if (*offset > 0)
		v -= 1;  /* the value returned when offset was zero */
	else
		do {
			atomic_inc(counter);
			v++;
		} while (!is_prime(v));

	len = snprintf(tmp, TMPSIZE, "%d\n", v);
	if (*offset > len)
		return 0;
	if (count > len - *offset)
		count = len - *offset;
/*
 * Copy it back, increment the offset, and we're done.
 */
	if (copy_to_user(buf, tmp + *offset, count))
		return -EFAULT;
	*offset += count;
	return count;
}

/*
 * Write a file.
 */
static ssize_t mathfs_write_file(struct file *filp, const char *buf,
		size_t count, loff_t *offset)
{
	char tmp[TMPSIZE];
	atomic_t *counter = (atomic_t *) filp->private_data;
/*
 * Only write from the beginning.
 */
	if (*offset != 0)
		return -EINVAL;
/*
 * Read the value from the user.
 */
	if (count >= TMPSIZE)
		return -EINVAL;
	memset(tmp, 0, TMPSIZE);
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
/*
 * Store it in the counter and we are done.
 */
	atomic_set(counter, simple_strtol(tmp, NULL, 10));
	return count;
}

/*
 * Now we can put together our file operations structure.
 */
static struct file_operations mathfs_file_ops = {
	.open	= mathfs_open,
	.read 	= mathfs_read_file,
	.write  = mathfs_write_file,
};

/*
 * OK, create the files that we export.
 */
struct tree_descr OurFiles[] = {
	{ NULL, NULL, 0 },  /* Skipped */
	{ .name = "prime",
	  .ops = &mathfs_file_ops,
	  .mode = S_IWUSR|S_IRUGO },
	{ "", NULL, 0 }
};

/*
 * "Fill" a superblock with mundane stuff.
 */
static int mathfs_fill_super(struct super_block *sb, void *data, int silent)
{
	return simple_fill_super(sb, MATHFS_MAGIC, OurFiles);
}

/*
 * Stuff to pass in when registering the filesystem.
 */
static struct dentry *mathfs_get_super(struct file_system_type *fst,
		int flags, const char *devname, void *data)
{
	return mount_single(fst, flags, data, mathfs_fill_super);
}

static struct file_system_type mathfs_type = {
	.owner 		= THIS_MODULE,
	.name		= "mathfs",
	.mount		= mathfs_get_super,
	.kill_sb	= kill_litter_super,
};

/*
 * Get things set up.
 */
static int __init mathfs_init(void)
{
	atomic_set(&prime, 1);
	return register_filesystem(&mathfs_type);
}

static void __exit mathfs_exit(void)
{
	unregister_filesystem(&mathfs_type);
}

module_init(mathfs_init);
module_exit(mathfs_exit);

