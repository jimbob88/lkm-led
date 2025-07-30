#define DEVICE_NAME "jimbob_led"
#define NUM_DEVICES 1
#define BUF_LEN 80

enum {
  DEVICE_FREE,
  DEVICE_BOUND,
};

static ssize_t on_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t on_write(struct file *, const char __user *, size_t, loff_t *);
static int on_open(struct inode *, struct file *);
static int on_release(struct inode *, struct file *);
