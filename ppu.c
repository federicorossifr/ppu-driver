
#include <asm/uaccess.h> /* put_user */
#include <linux/cdev.h> /* cdev_ */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
 #include <linux/wait.h>
 #include <linux/poll.h>


#define BAR 0
#define CDEV_NAME "lkmc_ppu"
#define EDU_DEVICE_ID 0xFEFE
#define IO_IRQ_ACK 0x64
#define IO_IRQ_STATUS 0x24
#define QEMU_VENDOR_ID 0xFEDE
#define DMA_SIZE 4096
#define DMA_START 0x40000
#define DMA_SRC 0x80
#define DMA_DST 0x88
#define DMA_CNT 0x90
#define DMA_CMD 0x98
#define DMA_MASK DMA_BIT_MASK(28)
MODULE_LICENSE("GPL");

static struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(QEMU_VENDOR_ID, EDU_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);
static DECLARE_WAIT_QUEUE_HEAD(ppu_wait);
static int pci_irq;
static int major;
static struct pci_dev *pdev;
static void __iomem *mmio;
static dma_addr_t dma_handle; // to the card
static void* dma_cpu_addr; // cpu addr
//const char __user *usr_dma_buf;
bool sem_t = false;
static ssize_t read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;
	if (*off % 4 || len == 0) {
		ret = 0;
	} else {
		kbuf = ioread32(mmio + *off);
		if (copy_to_user(buf, (void *)&kbuf, 4)) {
			ret = -EFAULT;
		} else {
			ret = 4;
			(*off)++;
		}
	}
	return ret;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;
	u64 addr_buf;
	ret = len;
	if (!(*off % 4)) {

		switch(*off) {
			case DMA_SRC: //handle DMA_SRC write from user
				copy_from_user((void *)&addr_buf, buf, 8);
				pr_info("user writing to dma src, %llx\n",addr_buf); 
				pr_info("copying to internal buffer, %llx\n",dma_cpu_addr); 
				//usr_dma_buf = addr_buf;
				copy_from_user(dma_cpu_addr, (char* __user) addr_buf, 4);
				pr_info("data at src: %d\n",*(int*)dma_cpu_addr);
				iowrite64(dma_handle, mmio + DMA_SRC);
				iowrite64(DMA_START, mmio + DMA_DST);
				return ret;

			case DMA_DST: //handle DMA_CNT write from user
				copy_from_user((void *)&addr_buf, buf, 8);
				pr_info("user writing to dma dst, %llx\n",addr_buf); 
				pr_info("sub, %llx\n",addr_buf); 

				//usr_dma_buf = addr_buf;
				
				break;
			case DMA_CNT: pr_info("user writing to dma cnt, %ld\n",kbuf); break;
			case DMA_CMD: pr_info("user writing to dma CMD, %ld\n",kbuf); sem_t = false; break;

			default: break;
		}





		if (copy_from_user((void *)&kbuf, buf, 4) || len != 4) {
			ret = -EFAULT;
		} else {




			iowrite32(kbuf, mmio + *off);
		}
	}
	return ret;
}

static loff_t llseek(struct file *filp, loff_t off, int whence)
{
	filp->f_pos = off;
	return off;
}


 static unsigned int fpoll(struct file *file, poll_table *wait)
 {
     poll_wait(file, &ppu_wait, wait);
     if (sem_t)
         return POLLIN | POLLRDNORM;
     return 0;
 }

/* These fops are a bit daft since read and write interfaces don't map well to IO registers.
 *
 * One ioctl per register would likely be the saner option. But we are lazy.
 *
 * We use the fact that every IO is aligned to 4 bytes. Misaligned reads means EOF. */
static struct file_operations fops = {
	.owner   = THIS_MODULE,
	.llseek  = llseek,
	.read    = read,
	.write   = write,
	.poll    = fpoll
};

static irqreturn_t irq_handler(int irq, void *dev)
{
	int devi;
	irqreturn_t ret;
	u32 irq_status;

	devi = *(int *)dev;
	if (devi == major) {
		irq_status = ioread32(mmio + IO_IRQ_STATUS);
		pr_info("interrupt irq = %d dev = %d irq_status = %llx\n",
				irq, devi, (unsigned long long)irq_status);
		/* Must do this ACK, or else the interrupts just keeps firing. */
		sem_t = true;
 		wake_up_interruptible(&ppu_wait);		
		iowrite32(irq_status, mmio + IO_IRQ_ACK);
		ret = IRQ_HANDLED;
	} else {
		ret = IRQ_NONE;
	}
	return ret;
}

/**
 * Called just after insmod if the hardware device is connected,
 * not called otherwise.
 *
 * 0: all good
 * 1: failed
 */
static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	u8 val;

	pr_info("pci_probe\n");
	major = register_chrdev(0, CDEV_NAME, &fops);
	pdev = dev;
	if (pci_enable_device(dev) < 0) {
		dev_err(&(pdev->dev), "pci_enable_device\n");
		goto error;
	}
	if (pci_request_region(dev, BAR, "myregion0")) {
		dev_err(&(pdev->dev), "pci_request_region\n");
		goto error;
	}
	mmio = pci_iomap(pdev, BAR, pci_resource_len(pdev, BAR));

	/* IRQ setup. */
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &val);
	pci_irq = val;
	if (request_irq(pci_irq, irq_handler, IRQF_SHARED, "pci_irq_handler0", &major) < 0) {
		dev_err(&(dev->dev), "request_irq\n");
		goto error;
	}

	dma_cpu_addr = dma_alloc_coherent(&pdev->dev, DMA_SIZE, &dma_handle, GFP_KERNEL );
	pr_info("allocated DMA memory. Size: %d, start: 0x%llx, handle: 0x%llx \n",DMA_SIZE,(char*)dma_cpu_addr,dma_handle);
	pci_set_master(pdev); 
	return 0;
error:
	return 1;
}

static void pci_remove(struct pci_dev *dev)
{
	pr_info("pci_remove\n");
	dma_free_coherent(&dev->dev,DMA_SIZE,dma_cpu_addr,dma_handle);
	pci_release_region(dev, BAR);
	unregister_chrdev(major, CDEV_NAME);
}

static struct pci_driver pci_driver = {
	.name     = "lkmc_ppu",
	.id_table = pci_ids,
	.probe    = pci_probe,
	.remove   = pci_remove,
};

static int myinit(void)
{
	if (pci_register_driver(&pci_driver) < 0) {
		return 1;
	}
	return 0;
}

static void myexit(void)
{
	pci_unregister_driver(&pci_driver);
}

module_init(myinit);
module_exit(myexit);
