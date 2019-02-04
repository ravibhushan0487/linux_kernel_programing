#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/hashtable.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>

#define PCFS_NAME "ht530_pcfs"
#define DEVICE_NAME "ht530_drv"
#define DEVICE_NAME1 "ht530-1"
#define DEVICE_NAME2 "ht530-2"

#define H_NAME ht530_tbl
#define H_NAME1 ht530_tbl_0
#define H_NAME2 ht530_tbl_1
#define DUMP _IOWR(530,0, struct dump_arg)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sushant Trivedi");
MODULE_DESCRIPTION("EOSI_Assignment_1");

//DATASTRUCT DEFINITION
//HASH TABLE OBJECT
typedef struct ht_object
{
    int key;
    int data ;
} ht_object_t;


//IOCTL STRUCT
struct dump_arg 
{
    int n;                          //Bucket number N
    ht_object_t object_array[8];    //8 objects to retrieve
};


//DRIVER DEVICE POINTER
struct ht530_dev
{
    struct cdev cdev;
    //DECLARE_HASHTABLE(H_NAME, 7);
    DECLARE_HASHTABLE(H_NAME1, 7);
    DECLARE_HASHTABLE(H_NAME2, 7);
    //char hash_name[40];
} *ht530_devp;
//} *ht530_devp_1, *ht530_devp_2;


//HASHLIST OBJECT NODE
struct ht_node
{
    ht_object_t ht_obj;
    struct hlist_node hlist;
};


//VARIABLES
static dev_t ht530_dev_number;
static dev_t current_dev_no;

static struct class *ht530_class;       
static struct device *ht530_device_1;     
static struct device *ht530_device_2;

//FUNCTION DEFINITION
//DRIVER FOPS FUNCTIONS
int ht530_drv_driver_open(struct inode *inode, struct file *file)
{
    
    struct ht530_dev *ht_devpp;
    ht_devpp = container_of(inode->i_cdev, struct ht530_dev, cdev);
    current_dev_no = inode->i_rdev;
    file->private_data = ht_devpp;
    if(MINOR(current_dev_no)==0)
    {
        hash_init(ht_devpp->H_NAME1);
    }
    else
    {
        hash_init(ht_devpp->H_NAME2);
    }
    printk(KERN_INFO "HT530 - DEVICE_OPENED: %d %d", MAJOR(current_dev_no), MINOR(current_dev_no));
    return 0;
}






ssize_t ht530_drv_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    struct hlist_head *hnode;
    struct ht_node *node_iter;
    ht_object_t ht_temp;    
    //struct hlist_node *temp;
    //int hcount=0;
    int hminkey;
    int flag = 0;  //1->Found an existing node with same key, 0-> no node found

    //POINTER CREATION
    struct ht530_dev *ht_devpp = file->private_data;
    struct ht_node *node = kmalloc(sizeof(struct ht_node), GFP_KERNEL);


    //STORES THE NODE READ FROM THE USER
    copy_from_user(&ht_temp, (ht_object_t*)buf, sizeof(ht_object_t));
    node->ht_obj.key = ht_temp.key;
    node->ht_obj.data = ht_temp.data;

    //BUCKET NO CALCULATED FOR WRITE
    hminkey = hash_min(node->ht_obj.key, 7);
 //   printk(KERN_ALERT "HT530 - Element from User - KEY: %d, DATA: %d HIMKEY: %d\n", node->ht_obj.key, node->ht_obj.data, hminkey);

    if(MINOR(current_dev_no)==0)
    {
        //printk(KERN_INFO "HT530 - HASH1");
        /////////////////////////////////////?/////////////
        //COMPARISONS AND CHECKING
        //List Head for the given Key

        hnode = &ht_devpp->H_NAME1[hash_min(node->ht_obj.key,7)];
        //KEY=0 -> Delete element 
        if(node->ht_obj.data == 0) 
        {
            printk(KERN_INFO "HT530 - HASH1 - Writing Element - Data: 0 - Deleting element");


            //ITERATING OVER THE GIVEN KEY BUCKET
            hash_for_each_possible(ht_devpp->H_NAME1, node_iter, hlist, node->ht_obj.key)
            {
                if(node_iter->ht_obj.key == node->ht_obj.key)
                {
                    hash_del(&node_iter->hlist);
                }
            }
        }
        else 
        {
            printk(KERN_INFO "HT530 - HASH1 - Writing Element - Key: %d Data: %d", node->ht_obj.key, node->ht_obj.data);
            
            //ITERATING OVER THE GIVEN KEY BUCKET
            hash_for_each_possible(ht_devpp->H_NAME1, node_iter, hlist, node->ht_obj.key)
            {
                //FOUND AN EXISITING NODE WITH SAME KEY
                if(node_iter->ht_obj.key == node->ht_obj.key)
                {
                    node_iter->ht_obj.data = node->ht_obj.data;
                    flag = 1;
                    //break;    ///TENTATIVE
                }
            }
            if (flag == 0)
            {
                hash_add(ht_devpp->H_NAME1, &node->hlist, node->ht_obj.key);
            }
        }
        //////////////////////////////////////////////////////////////////////////////
    }
    else
    {
/////////////////////////////////////?/////////////
//        printk(KERN_INFO "HT530 - HASH2");
        hnode = &ht_devpp->H_NAME2[hash_min(node->ht_obj.key,7)];
        //KEY=0 -> Delete element 
        if(node->ht_obj.data == 0) 
        {
            printk(KERN_INFO "HT530 - HASH2 - Writing Element - Data: 0 - Deleting element");


            //ITERATING OVER THE GIVEN KEY BUCKET
            hash_for_each_possible(ht_devpp->H_NAME2, node_iter, hlist, node->ht_obj.key)
            {
                if(node_iter->ht_obj.key == node->ht_obj.key)
                {
                    hash_del(&node_iter->hlist);
                }
            }
        }
        else 
        {
            printk(KERN_INFO "HT530 - HASH2 - Writing Element - Key: %d Data: %d", node->ht_obj.key, node->ht_obj.data);
            
            //ITERATING OVER THE GIVEN KEY BUCKET
            hash_for_each_possible(ht_devpp->H_NAME2, node_iter, hlist, node->ht_obj.key)
            {
                //FOUND AN EXISITING NODE WITH SAME KEY
                if(node_iter->ht_obj.key == node->ht_obj.key)
                {
                    node_iter->ht_obj.data = node->ht_obj.data;
                    flag = 1;
                    //break;    ///TENTATIVE
                }
            }
            if (flag == 0)
            {
                hash_add(ht_devpp->H_NAME2, &node->hlist, node->ht_obj.key);
            }
        }
//////////////////////////////////////////////////
    }
    return 1;
}


ssize_t ht530_drv_driver_read(struct file *file, char * buf, size_t count, loff_t *ppos)
{
    struct ht530_dev* ht_devpp;
    struct hlist_head *hnode;
    struct ht_node* node;
    ht_object_t ht_data;
    int hminkey;
    int flag = 0;
    
    ht_devpp = file->private_data;
    copy_from_user(&ht_data, (ht_object_t *)buf, sizeof(ht_object_t));
    hminkey = hash_min(ht_data.key, 7);
    //printk(KERN_INFO "HT530 - KEY: %d MINKEY: %d\n", ht_data.key, hminkey);
    
    if(MINOR(current_dev_no)==0)
    {
        //Finding Header pointer for the bucket
        hnode = &ht_devpp->H_NAME1[hash_min(ht_data.key,7)];

        //Empty bucket
        if (hnode->first == NULL)
        {
            printk(KERN_INFO "HT530 - HASH1 - Reading - Empty Bucket");
            ht_data.data = -1;
            copy_to_user((ht_object_t *)buf, &ht_data, sizeof(ht_object_t));
            return -EINVAL;
        }

        //ITERATING OVER THE GIVEN KEY BUCKET
        hash_for_each_possible(ht_devpp->H_NAME1, node, hlist, ht_data.key)
        {
            if(ht_data.key == node->ht_obj.key)
            {
                //FOUDN AN ELEMENT WITH SAME KEY
                ht_data.data = node->ht_obj.data;
                flag = 1;
                //break;    //TENTATIVE
            }

        }
        if(flag == 1)
        {
            printk(KERN_INFO "HT530 - HASH1 - Reading - Data: %d Key: %d\n",ht_data.data, ht_data.key);
            copy_to_user((ht_object_t *)buf, &ht_data, sizeof(ht_object_t));     
            return 1;
        }   

        //ELEMENT NOT FOUND IN GIVEN BUCKET
        else
        {
            printk(KERN_INFO "HT530 - HASH1 - Reading - Not found in Bucket");
            ht_data.data = -1;
            copy_to_user((ht_object_t *)buf, &ht_data, sizeof(ht_object_t));
            return -EINVAL;
        }
    }
    else
    {
        //Finding Header pointer for the bucket
        hnode = &ht_devpp->H_NAME2[hash_min(ht_data.key,7)];

        //Empty bucket
        if (hnode->first == NULL)
        {
            printk(KERN_INFO "HT530 - HASH2 Reading - Empty Bucket");
            ht_data.data = -1;
            copy_to_user((ht_object_t *)buf, &ht_data, sizeof(ht_object_t));
            return -EINVAL;
        }

        //ITERATING OVER THE GIVEN KEY BUCKET
        hash_for_each_possible(ht_devpp->H_NAME2, node, hlist, ht_data.key)
        {
            if(ht_data.key == node->ht_obj.key)
            {
                //FOUND AN ELEMENT WITH SAME KEY
                ht_data.data = node->ht_obj.data;
                flag = 1;
                //break;    //TENTATIVE
            }
        }
        if(flag == 1)
        {
            printk(KERN_INFO "HT530 - HASH2 - Reading - Data: %d Key: %d\n",ht_data.data, ht_data.key);
            copy_to_user((ht_object_t *)buf, &ht_data, sizeof(ht_object_t));     
            return 1;
        }   

        //ELEMENT NOT FOUND IN GIVEN BUCKET
        else
        {
            printk(KERN_INFO "HT530 - HASH2 - Reading - Not found in Bucket");
            ht_data.data = -1;
            copy_to_user((ht_object_t *)buf, &ht_data, sizeof(ht_object_t));
            return -EINVAL;
        }
    }
    return 0;
}


int ht530_drv_driver_release(struct inode *inode, struct file *file)
{
    //clean up hashtable
    struct ht530_dev *ht_devpp;
    struct ht_node* node_iter;
    struct hlist_node* temp;
    int bkt;

    printk(KERN_INFO "HT530 - Driver Release function");

    ht_devpp = file->private_data;

    if(MINOR(current_dev_no)==0)
    {
        if(!hash_empty(ht_devpp->H_NAME1)) 
        {
            hash_for_each_safe(ht_devpp->H_NAME1, bkt, temp, node_iter, hlist) 
            {
                hash_del(&node_iter->hlist);
                kfree(node_iter);
            }
        }
        hash_init(ht_devpp->H_NAME1);
    }
    else
    {
        if(!hash_empty(ht_devpp->H_NAME2)) 
        {
            hash_for_each_safe(ht_devpp->H_NAME2, bkt, temp, node_iter, hlist) 
            {
                hash_del(&node_iter->hlist);
                kfree(node_iter);
            }
        }
        hash_init(ht_devpp->H_NAME2);
    }


    return 0;
}

static long ht530_drv_driver_ioctl(struct file * file, unsigned int ioctlnum, unsigned long arg)
{   

    struct ht530_dev *ht_devpp;
    struct dump_arg *d_arg;
    struct ht_node *node;
    int nbkt, val, lcount = 0;
    int no_from_user;

    
    ht_devpp = file->private_data;
    switch(ioctlnum) 
    {
        case DUMP:
            d_arg = (struct dump_arg*) arg;
            no_from_user = d_arg->n;
            printk(KERN_INFO "IOCTL: BUCKET DUMPED: %d", no_from_user);


            for(nbkt=0; nbkt<1000000; nbkt++)
            {
                if (hash_min(nbkt,7)== no_from_user)
                    {break;}
            }
            
            //No of Bucket -> OUT OF RANGE
            if(nbkt > 128 || nbkt < 0) 
            {
                printk(KERN_INFO "IOCTL: Bucket number out of range\n");
                printk(KERN_INFO "\n");
                val = -1;
                copy_to_user(&arg, &val, sizeof(int));
                return -EINVAL;
            }


        
            //ITERATE AND COPY 8 OBJECTS
            lcount = 0;
            
            //ITERATING OVER THE GIVEN KEY BUCKET
            if(MINOR(current_dev_no)==0)
            {    
                hash_for_each_possible(ht_devpp->H_NAME1, node, hlist, nbkt)
                {
                    if((lcount<8)&&(node!=NULL) )
                    {
                        printk(KERN_INFO "%d KEY: %d, VALUE: %d -", lcount, node->ht_obj.key, node->ht_obj.data);
                        copy_to_user(&d_arg->object_array[lcount], &node->ht_obj, sizeof(ht_object_t));
                        lcount++;
                    }
                    else if (node == NULL)
                    {
                        printk(KERN_INFO "IOCTL: Bucket Empty");
                        copy_to_user(&d_arg->n, &lcount, sizeof(int));
                        return 1;
                    }
                    else
                    {
                        printk(KERN_INFO "8 elements found");
                        break;
                    }
                }
                printk(KERN_INFO "\n");
            }
            else
            {
                hash_for_each_possible(ht_devpp->H_NAME2, node, hlist, nbkt)
                {
                    if((lcount<8)&&(node!=NULL) )
                    {
                        printk(KERN_INFO "%d ITEM KEY: %d, VALUE: %d ", lcount+1, node->ht_obj.key, node->ht_obj.data);
                        copy_to_user(&d_arg->object_array[lcount], &node->ht_obj, sizeof(ht_object_t));
                        lcount++;
                    }
                    else if (node == NULL)
                    {
                        printk(KERN_INFO "IOCTL: Bucket Empty");
                        copy_to_user(&d_arg->n, &lcount, sizeof(int));
                        return 1;
                    }
                    else
                    {
                        printk(KERN_INFO "8 elements found");
                        break;
                    }
                }
                printk(KERN_INFO "\n");
            }
            break;
    
        default:
            return -EINVAL;
            break;
        }
    return 0;
}

//FILE OPERATIONS             
static struct file_operations ht530_drv_fops = {
    .owner        = THIS_MODULE,                //OWNER
    .open        = ht530_drv_driver_open,       //OPEN
    .write        = ht530_drv_driver_write,     //WRITE
    .read        = ht530_drv_driver_read,       //READ
    .release    = ht530_drv_driver_release,     //RELEASE
    .unlocked_ioctl = ht530_drv_driver_ioctl    //IOCTL
};


//DRIVER INITIALIZATION
int __init ht530_drv_driver_init(void)
{
    /* Request dynamic allocation of a device major number */
    if (alloc_chrdev_region(&ht530_dev_number, 0, 2, DEVICE_NAME) < 0)    //Use cat /proc/devices/ to look up the driver
    {
            printk(KERN_DEBUG "HT530 - Can't register device\n");
            return -1;
    }

    //MEMORY ALLOCATION
    ht530_devp = kmalloc(sizeof(struct ht530_dev), GFP_KERNEL);    
    if (!ht530_devp)
    {
        printk(KERN_DEBUG "Kmalloc incomplete\n");
        return -ENOMEM;
    }

    //CDEV - Connect
    cdev_init(&ht530_devp->cdev, &ht530_drv_fops);
    ht530_devp->cdev.owner = THIS_MODULE;  //DO WE NEED THIS?
    if (cdev_add(&ht530_devp->cdev, ht530_dev_number, 2 ))
    {
        printk(KERN_DEBUG "HT530 - CDEV_ADD failed");
        return -1;
    }

    //DEVICE CREATION
    //CLASS CREATE
    ht530_class =  class_create(THIS_MODULE, DEVICE_NAME);
    ht530_device_1 = device_create(ht530_class, NULL, MKDEV(MAJOR(ht530_dev_number), 0), NULL, DEVICE_NAME1);
    ht530_device_2 = device_create(ht530_class, NULL, MKDEV(MAJOR(ht530_dev_number), 1), NULL, DEVICE_NAME2);
    printk(KERN_INFO "HT530 - Devices created");
    printk(KERN_INFO "HT530 - Major: %d, Minor: %d", MAJOR(ht530_dev_number), MINOR(ht530_dev_number));  
    return 0;
}




//DRIVER EXIT
void __exit ht530_drv_driver_exit(void)
{
    cdev_del(&ht530_devp->cdev);
    device_destroy(ht530_class, MKDEV(MAJOR(ht530_dev_number), 0));
    device_destroy(ht530_class, MKDEV(MAJOR(ht530_dev_number), 1));
    kfree(ht530_devp);
    class_destroy(ht530_class);
    unregister_chrdev_region((ht530_dev_number), 2);
    printk("HT530 - Driver removed.\n");  
}




module_init(ht530_drv_driver_init);
module_exit(ht530_drv_driver_exit);