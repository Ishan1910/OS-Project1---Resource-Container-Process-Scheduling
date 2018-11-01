//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "processor_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

struct mutex lock;

struct task_list {
    int task_id;
    struct task_struct *current_task;
    struct task_list *next;
};

struct container_list {
    unsigned long long cid;
    struct  task_list *tasks;
    struct container_list *next;
} *container_head=NULL;

void printnodes(struct container_list *cont)
{
    printk("\n------------------------------------------------------\n");
    mutex_lock(&lock);
    struct container_list *temp = cont;
    struct task_list *task;
    while (temp != NULL)
    {
        printk("\n container:\t %d \n", temp->cid);
        task = temp->tasks;
        while (task != NULL)
        {
            printk("Tasks:\t %d\n",task->task_id);
            task = task->next;
        }
        temp = temp->next;
    }
    mutex_unlock(&lock);
    printk("\n------------------------------------------------------\n");
}

struct task_list *addTask(struct task_list *head_task, struct task_struct *current_task, int task_id){
    struct task_list *curr;
    struct task_list *new_task = kmalloc(sizeof(struct task_list), GFP_KERNEL);
    new_task->current_task = current_task;
    new_task->next = NULL;
    new_task->task_id = task_id;

    if (head_task == NULL)
    {
        head_task = new_task;
        printk(KERN_ERR "\n 1st thread is %d\n", head_task->task_id);
        mutex_unlock(&lock);
        return head_task;
    }
    else
    {
        curr = head_task;
        if (curr->next == NULL)
        {
            curr->next = new_task;
            mutex_unlock(&lock);
            printk(KERN_ERR "\n2nd thread is %d \n", (head_task->next)->task_id);
            set_current_state(TASK_INTERRUPTIBLE);
            schedule();
            return head_task;
        }
        else
        {
            while(curr->next != NULL)
            {
                curr = curr->next;
            }
            curr->next = new_task;
            mutex_unlock(&lock);
            printk(KERN_ERR "\n2nd thread is %d \n", (head_task->next)->task_id);
            set_current_state(TASK_INTERRUPTIBLE);
            schedule();
            return head_task;
        }
        curr->next = new_task;
    }
}

struct container_list *CIDInSwitch(int task_id){
    struct container_list *cont = container_head;
    printk("\nOK\n");
    int len = 0;
    while (cont->tasks != NULL)
    {
        cont->tasks = (cont->tasks)->next;
        len += 1;
    }
    printk("\n\n\n--------LENGTH is %d-------\n\n\n", len);
    if (cont != NULL)
    {
        if (cont->tasks != NULL)
        {
            if ((cont->tasks)->task_id == task_id)
                printk("task head is %d", task_id);
            {
                return cont->cid;
            }
        }
    }
    return -1;
}
/**
 * Delete the task in the container.
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), 
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
    struct container_list *container = container_head;
    struct container_list *temp = NULL;
    struct processor_container_cmd *cont;
    cont = kmalloc(sizeof(struct processor_container_cmd), GFP_KERNEL);
    copy_from_user(cont, user_cmd, sizeof(struct processor_container_cmd));
    printk("\n\n---cont is %d----\n\n", cont->cid);


    mutex_lock(&lock);

    while(container != NULL)
    {
        printk("\nHead is not null in delete %d\n", container->cid);

        if (container->cid == cont->cid)
        {
            struct task_list *task = container->tasks;
            if (task == NULL)
            {
                printk("\ncontainer is empty\n");
                if(temp == NULL) container_head = container->next;
                else temp->next = container->next;
                kfree(container);
                container = NULL;
                mutex_unlock(&lock);
                break;
            }
            if (task->next == NULL)
            {
                printk("\nEmptying now\n");
                kfree(task);
                task = NULL;
                if(temp == NULL) container_head = container->next;
                else temp->next = container->next;
                kfree(container);
                container = NULL;
                mutex_unlock(&lock);
                break;
            }
            container->tasks = task->next;
            wake_up_process((container->tasks)->current_task);
            kfree(task);
            task = NULL;
            break;
        }
        temp = container;
        container = container->next;
    }
    // mutex_unlock(&lock);
    printk("Deletion here with id \n");
    return 0;
}

/**
 * Create a task in the corresponding container.
 * external functions needed:
 * copy_from_user(), mutex_lock(), mutex_unlock(), set_current_state(), schedule()
 * 
 * external variables needed:
 * struct task_struct* current  
 */
int processor_container_create(struct processor_container_cmd __user *user_cmd)
{
    struct container_list *container;
    container = container_head;
    int cont_present = 0;

    if (container_head == NULL)
    {
        mutex_init(&lock);
        printk(KERN_ERR "This should print only once");
    }
    mutex_lock(&lock);

    struct processor_container_cmd *cont;
    cont = kmalloc(sizeof(struct processor_container_cmd), GFP_KERNEL);
    copy_from_user(cont, user_cmd, sizeof(struct processor_container_cmd));

    if (container != NULL)
    {
        while(container->next != NULL)
        {
            if (container->cid == cont->cid)
            {
                cont_present = 1;
                (container->tasks) = addTask(container->tasks, current, current->pid);
                // set_current_state(TASK_RUNNING);
                // schedule();
                // printk(KERN_INFO "Idhar nai aa raha current thread id is : %d\n", current_task->pid);
                break;
            }
            container = container->next;
        }
        if (container->next == NULL && container->cid == cont->cid)
        {
            cont_present = 1;
            container->tasks = addTask(container->tasks, current, current->pid);
            // set_current_state(TASK_RUNNING);
            // schedule();
            // printk(KERN_INFO "Idhar aa raha current thread id is : %d\n", current->pid);
        }
    }

    if (!cont_present)
    {
        struct container_list *new_container;
        new_container = kmalloc(sizeof(struct container_list), GFP_KERNEL);
        new_container->cid = cont->cid;
        new_container->next = NULL;
        new_container->tasks = NULL;
        new_container->tasks = addTask(new_container->tasks, current, current->pid);
        if (container_head == NULL)
        {
            container_head = new_container;
        }
        else
        {
            container->next = new_container;
        }
        container = new_container;
        // set_current_state(TASK_RUNNING);
        // schedule();
        // printk(KERN_INFO "current thread id is: %d and status id is: %d \n", ((container->tasks)->current_task)->pid, (container->tasks)->status);    
    }
    // printnodes(container_head);
    // set_current_state(TASK_INTERRUPTIBLE);
    // schedule();
    // printk("Creation here with id :%llu and thread id is %d \n", container_head->cid, current->pid);
    return 0;
}

/**
 * switch to the next task in the next container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */
int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
    int cid = 0;
    mutex_lock(&lock);
    printk("current in switch is %d\n", current->pid);
    cid = CIDInSwitch(current->pid);

    if (cid != -1)
    {   
        struct container_list *cont = container_head;
        struct task_list *head = NULL;
        while (cont != NULL)
        {
            if (cont->cid == cid)
            {
                head = cont->tasks;
                break;
            }
            cont = cont->next;
        }
        if (cont == NULL)
        {
            printk("No container");
            mutex_unlock(&lock);
        }
        printk("FIFO task is %d", head->task_id);
        if (head == NULL || head->next == NULL)
        {
            mutex_unlock(&lock);
        }
        else
        {
            printk("getting here\n");
            struct task_list *curr = head;
            struct task_list *temp = head->next;
            while (curr->next != NULL)
            {
                curr = curr->next;
            }
            curr->next = head;
            head->next = NULL;
            cont->tasks = temp;

            wake_up_process(temp->current_task);
            mutex_unlock(&lock);
            set_current_state(TASK_INTERRUPTIBLE);
            schedule();
        }
    }
    else
    {
        printk("\nNO CONTAINER\n");
        mutex_unlock(&lock);
    }
    return 0;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int processor_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{    
    switch (cmd)
    {
    case PCONTAINER_IOCTL_CSWITCH:
        return processor_container_switch((void __user *)arg);
    case PCONTAINER_IOCTL_CREATE:
        return processor_container_create((void __user *)arg);
    case PCONTAINER_IOCTL_DELETE:
        return processor_container_delete((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
