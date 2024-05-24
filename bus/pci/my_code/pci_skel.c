#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

struct pci_skel_card
{
    unsigned int irq;
    // ...
};

static struct pci_device_id ids[] = {
    {
        PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801AA_3),
    },
    {
        0,
    } // 最后一组是0，表示结束
};
MODULE_DEVICE_TABLE(pci, ids);

static void skel_get_configs(struct pci_dev *dev)
{
    uint8_t revision;
    uint16_t vendor_id, device_id;
    uint32_t class_code;

    pci_read_config_word(dev, PCI_VENDOR_ID, &vendor_id);
    pci_read_config_word(dev, PCI_DEVICE_ID, &device_id);
    pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
    pci_read_config_dword(dev, PCI_CLASS_REVISION, &class_code);

    printk(KERN_INFO "PCI device: vendor_id=0x%04x, device_id=0x%04x, revision=0x%02x, class_code=0x%08x\n",
           vendor_id, device_id, revision, class_code);
}

static irqreturn_t pci_skel_interrupt(int irq, void *dev_id)
{
    struct pci_skel_card *my_pci = (struct pci_skel_card *)dev_id;

    // 处理中断
    // ...

    printk(KERN_INFO "Interrupt received irq = %d and mu_pci.irq%d\n", irq, my_pci->irq);

    return IRQ_HANDLED;
}

static int probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct pci_skel_card *my_pci;
    int ret = 0;

    if (pci_enable_device(pdev))
    {
        printk(KERN_ERR "Failed to enable PCI device\n");
        return -ENODEV;
    }

    my_pci = kzalloc(sizeof(struct pci_skel_card), GFP_KERNEL);
    if (!my_pci)
    {
        printk(KERN_ERR "Failed to allocate memory for device\n");
        return -ENOMEM;
    }
    my_pci->irq = pdev->irq;
    if (my_pci->irq < 0)
    {
        printk(KERN_ERR "Failed to get IRQ for device\n");
        return -ENODEV;
    }
    // 请求 PCI 设备的 I/O 端口和内存区域资源
    ret = pci_request_regions(pdev, "pci_skel");
    if (ret)
    {
        printk(KERN_ERR "Failed to request regions for device\n");
        goto out_1;
    }

    ret = request_irq(my_pci->irq, pci_skel_interrupt, IRQF_SHARED, "pci_skel", my_pci);
    if (ret)
    {
        printk(KERN_ERR "Failed to request IRQ for device\n");
        goto out_2;
    }
    pci_set_drvdata(pdev, my_pci);

    skel_get_configs(pdev);

    return 0;
out_2:
    pci_release_regions(pdev);
out_1:
    kfree(my_pci);
    return ret;
}

static void remove(struct pci_dev *dev)
{
    struct pci_skel_card *my_pci = (struct pci_skel_card *)pci_get_drvdata(dev);

    free_irq(my_pci->irq, my_pci);
    pci_release_regions(dev);
    kfree(my_pci);
    pci_disable_device(dev);
}

static struct pci_driver pci_driver = {
    .name = "pci_skel",
    .id_table = ids,
    .probe = probe,
    .remove = remove,
};

static int __init pci_skel_init(void)
{
    return pci_register_driver(&pci_driver);
}

static void __exit pci_skel_exit(void)
{
    pci_unregister_driver(&pci_driver);
}

module_init(pci_skel_init);
module_exit(pci_skel_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lidonghang-02");