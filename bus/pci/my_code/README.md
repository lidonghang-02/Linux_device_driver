# my_code

一个简单的PCI驱动示例

```bash
// 安装
make
insmod pci_skel.ko


➜  pci_skel git:(master) ✗ pwd
/sys/bus/pci/drivers/pci_skel
➜  pci_skel git:(master) ✗ ls
bind  module  new_id  remove_id  uevent  unbind

```