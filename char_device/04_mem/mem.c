#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>

struct mem_dev
{
  char* test;
  struct kmem_cache* cache;
};

struct mem_dev* mem_devp;

static int __init mem_dev_init_module(void)
{
  printk(KERN_INFO "mem_init\n");

  mem_devp = kmalloc(sizeof(struct mem_dev), GFP_KERNEL);

  // kmalloc
  mem_devp->test = kmalloc(1024, GFP_KERNEL);
  if (!mem_devp->test)
  {
    return -ENOMEM;
  }
  printk(KERN_INFO "kmalloc get addr %p\n", mem_devp->test);
  kfree(mem_devp->test);

  // __get_free_page
  mem_devp->test = (void*)__get_free_page(GFP_KERNEL);
  if (!mem_devp->test)
  {
    return -ENOMEM;
  }
  printk(KERN_INFO "__get_free_page get addr %p\n", mem_devp->test);
  free_page((unsigned long)mem_devp->test);

  // vmalloc
  mem_devp->test = vmalloc(PAGE_SIZE * 16);
  if (!mem_devp->test)
  {
    return -ENOMEM;
  }
  printk(KERN_INFO "vmalloc get addr %p\n", mem_devp->test);
  vfree(mem_devp->test);

  // slab
  mem_devp->cache = kmem_cache_create("mem_cache", sizeof(struct mem_dev), 0, SLAB_HWCACHE_ALIGN, NULL);
  if (!mem_devp->cache)
  {
    return -ENOMEM;
  }
  mem_devp->test = kmem_cache_alloc(mem_devp->cache, GFP_KERNEL);
  if (!mem_devp->test)
  {
    return -ENOMEM;
  }
  printk(KERN_INFO "slab get addr %p\n", mem_devp->test);
  kmem_cache_free(mem_devp->cache, mem_devp->test);
  kmem_cache_destroy(mem_devp->cache);

  kfree(mem_devp);
  return 0;
}

static void mem_dev_exit_module(void)
{
  printk(KERN_INFO "mem_dev exit\n");
}

module_init(mem_dev_init_module);
module_exit(mem_dev_exit_module);

MODULE_AUTHOR("lidonghang-02");
MODULE_LICENSE("GPL");
