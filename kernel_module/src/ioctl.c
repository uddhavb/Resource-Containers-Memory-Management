//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
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

#include "memory_container.h"

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

// lock for access to the containerList
static DEFINE_MUTEX(lock);

struct object {
  int oid;
  unsigned long pageFrameNumber;
  struct object *next;
};

struct lock {
  int oid;
  struct mutex lockObject;
  struct lock *next;
};

// structure for process list
struct process {
    int pid;
    struct task_struct *current_process;
    struct process *next;
};
// structure for container list
struct container {
    int cid;
    struct process *headProcess;
    struct object *headObject;
    struct lock *headLock;
    struct container *next;
};

// The main containerList
struct container *containerList =  NULL;


void addProcess (int curr_cid, int new_pid) {
    struct process *curr=NULL;
    // struct process *newNode = NULL;
    struct container * currContainer = containerList;
    struct process* newNode = NULL;
    while(currContainer != NULL)
    {
      if(currContainer->cid == curr_cid)
      {
        break;
      }
      currContainer = currContainer->next;
    }
    printk("\t\tstart add process\n");
    newNode = (struct process*) kcalloc(1, sizeof(struct process), GFP_KERNEL);
    newNode->current_process = current;
    newNode->pid = new_pid;
    newNode->next = NULL;
    if(currContainer->headProcess == NULL)
      {
          printk("first is null\n");
          currContainer->headProcess=newNode;
          return;
      }
    else {
     curr=currContainer->headProcess;
     while(curr->next!=NULL)
     {
       curr=curr->next;
     }
     curr->next=newNode;
   }
   printk("\t\tend addprocess\n");
}

// remove process from given container
void removeProcess (int curr_pid) {
  bool isProcessFound = false;
  struct container *currContainer = NULL;
  struct container *prevContainer = NULL;
  struct process *currProcess = NULL;
  struct process *prevProcess = NULL;
  struct process *processHead = NULL;
  struct lock *currLock = NULL;
  struct object * currObject = NULL;
  struct lock *nextLock = NULL;
  struct object * nextObject = NULL;
  printk("\t\tstart removeTopProcess\n");
  currContainer = containerList;
  while(currContainer!=NULL)
  {
    printk("in while loop\n");
    printk("in if curr->cid == curr_cid\n");
    currProcess = currContainer->headProcess;
    prevProcess = NULL;
    processHead = currContainer->headProcess;
    printk("variable->'first' assigned \n");
        while(currProcess != NULL)
        {
          if(currProcess->pid == curr_pid)
          {
            isProcessFound = true;
            if(prevProcess == NULL)
            {
              currContainer->headProcess = currProcess->next;
              kfree(currProcess);
              currProcess = NULL;
              break;
            }
            else
            {
                prevProcess->next = currProcess->next;
                kfree(currProcess);
                currProcess = NULL;
                break;
            }
          }
          prevProcess = currProcess;
          currProcess = currProcess->next;
        }
        if(currContainer->headProcess == NULL)
        {
          if(prevContainer == NULL)
          {
            containerList = currContainer->next;
          }
          else
          {
          prevContainer->next = currContainer->next;
          }

          // delete the list of objects and locks for this container
          currLock = currContainer->headLock;
          while(currLock!=NULL)
          {
            nextLock = currLock->next;
            kfree(currLock);
            currLock = nextLock;
          }
          currLock = NULL;
          nextLock = NULL;

          currObject = currContainer->next;
          while(currObject!= NULL)
          {
            nextObject = currObject->next;
            kfree(currObject);
            currObject = nextObject;
          }
          currObject = NULL;
          nextObject = NULL;

          kfree(currContainer);
          currContainer = NULL;
        }
        if(isProcessFound) break;
      prevContainer = currContainer;
      currContainer = currContainer->next;
    }
    printk("Container with CID: not found\n");
    printk("\t\tend removeProcess\n");
    return;
}

// add container to the end of the container list
void addContainer (int new_cid, int new_pid) {
   struct container *curr=NULL;
   // struct container *new_node = NULL;
   struct container* new_node = (struct container*) kcalloc(1, sizeof(struct container), GFP_KERNEL);
   printk("\t\tstart addContainer\n");
   new_node->cid  = new_cid;
   new_node->headProcess = NULL;
   new_node->next = NULL;
   new_node->headObject = NULL;
   new_node->headLock = NULL;
   // assign process data
   new_node->headProcess = (struct process*) kcalloc(1, sizeof(struct process), GFP_KERNEL);
   new_node->headProcess->current_process = current;
   new_node->headProcess->pid = new_pid;
   new_node->headProcess->next = NULL;
   if(containerList == NULL)
     {
         printk("head is null\n");
         containerList=new_node;
         return;
     }
  else {
    curr = containerList;
    while(curr->next!=NULL)
    {
      curr = curr->next;
    }
    curr->next=new_node;
  }
  printk("\t\tend addContainer\n");
}


// check if container exists or add it if it does not
void addToContainer(int new_cid, int new_pid)
{

  struct container *curr=containerList;
  printk("\t\tstart addTOContainer\n");
  printk("Add to container:\tCID: %d\tpid: %d\n",new_cid,new_pid);
  if(containerList == NULL)
    {
        printk("head is null in addTOcontainer\n");
        addContainer(new_cid, new_pid);
        return;
    }
  while(curr!=NULL)
  {
    if(curr->cid == new_cid)
    {
      addProcess(curr->cid, new_pid);
      return;
    }
    curr=curr->next;
  }
  addContainer(new_cid, new_pid);
  printk("\t\tend addTOContainer\n");
  return;
}


// print the map
void printMap(void)
{
  printk("\n-----------------------------------------------------------------------------------------------------------------\n");
  if(containerList == NULL)
  {
    printk("\nNo containers\n");
    mutex_unlock(&lock);
    return;
  }
  else
  {
    struct process *currProcess;
    struct object *currObject;
    struct container *currContainer = containerList;
    while(currContainer != NULL)
    {
      printk("\nCID:\t%d\t",currContainer->cid);
      currProcess = currContainer->headProcess;
      while(currProcess != NULL)
      {
        printk("pid:\t%d\t",currProcess->pid);
        currProcess = currProcess->next;
      }
      printk("\n");
      currObject = currContainer->headObject;
      while(currObject != NULL)
      {
        printk("oid:\t%d\t", currObject->oid);
        currObject = currObject->next;
      }
      printk("\n");
      currContainer = currContainer->next;
    }
  }
  printk("\n----------------------------------------------------------------------------------------------------------------\n");
}

int findContainerForProcess(int curr_pid)
{
  struct container *curr_container=NULL;
  printk("\t\tstart findContainerForprocess\n");
  curr_container = containerList;
  printk("find container for process:\tpid: %d\n",curr_pid);
  while(curr_container != NULL)
  {
    struct process *currProcess = curr_container->headProcess;
    while(currProcess != NULL)
    {
      if(currProcess->pid == curr_pid)
      {
        return curr_container->cid;
      }
      currProcess = currProcess->next;
    }
    curr_container = curr_container->next;
  }
  printk("\t\tend findContainerForprocess\n");
  return -1;
}




int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
    char *reservedSpace = NULL;
    struct container *curr;
    struct object *currObject;
    struct object *prevObject = NULL;
    struct object *new_object = NULL;
    int curr_offset;
    unsigned long sizeOfObject;
    int curr_cid;
    struct vm_area_struct* memoryInfo = (struct vm_area_struct*) kcalloc(1, sizeof(struct vm_area_struct), GFP_KERNEL);
    mutex_lock(&lock);
    copy_from_user(memoryInfo, vma, sizeof(struct vm_area_struct));
    curr_offset = (int) memoryInfo->vm_pgoff;
    sizeOfObject = memoryInfo->vm_end - memoryInfo->vm_start;
    kfree(memoryInfo);
    memoryInfo = NULL;

    printk("Finding pid's Container to create object.:\tPID: %d\n",(int)current->pid);
    curr_cid = findContainerForProcess(current->pid);
    if(curr_cid == -1)
    {
      printk("Container for process requesting object creation not found\n");
      mutex_unlock(&lock);
      return 0;
    }


    printk("Find the Object or insert new one\n");
    curr = containerList;
    printk("Add to container:\tCID: %d\n",curr_cid);
    while(curr!=NULL)
    {
      // printk("Inside while curr\n");
      if(curr->cid == curr_cid)
      {
        currObject = curr->headObject;
        while(currObject!=NULL)
        {
          // printk("Inside while currObject\n");
          if(currObject->oid == vma->vm_pgoff)
          {
            // the object already exists
            remap_pfn_range(vma, vma->vm_start, currObject->pageFrameNumber, vma->vm_end-vma->vm_start, vma->vm_page_prot);
            break;
          }
          prevObject = currObject;
          currObject = currObject->next;
        }
        if(currObject == NULL)
        {
          printk("create new object\n");
          // create a new object and assign page table entry
          reservedSpace = (char*) kmalloc((vma->vm_end - vma->vm_start)*sizeof(char), GFP_KERNEL);
          new_object = (struct object*) kcalloc(1, sizeof(struct object), GFP_KERNEL);
          new_object->next = NULL;
          new_object->oid = (int)vma->vm_pgoff;
          new_object->pageFrameNumber = virt_to_phys((void*)reservedSpace)>>PAGE_SHIFT;
          remap_pfn_range(vma, vma->vm_start, new_object->pageFrameNumber, vma->vm_end-vma->vm_start, vma->vm_page_prot);
          if(prevObject == NULL)
          {
            curr->headObject = new_object;
          }
          else prevObject->next = new_object;
        }
        break;
      }
      curr=curr->next;
    }

    mutex_unlock(&lock);
    // above function unlocks mutex
    return 0;
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
    int new_oid;
    struct container *currContainer = NULL;
    struct process *currProcess = NULL;
    struct lock *currLock = NULL;
    struct lock *prevLock = NULL;
    struct lock *new_lock = NULL;
    bool foundProcess = false;
    struct memory_container_cmd* userInfo = (struct memory_container_cmd*) kcalloc(1, sizeof(struct memory_container_cmd), GFP_KERNEL);
    mutex_lock(&lock);
    copy_from_user(userInfo,user_cmd,sizeof(struct memory_container_cmd));
    new_oid = (int)userInfo->oid;
    kfree(userInfo);
    userInfo = NULL;


    printk("searching container for oid: %d and pid: %d", new_oid, current->pid);
    currContainer = containerList;
    while(currContainer != NULL)
    {
      currProcess = currContainer->headProcess;
      while(currProcess != NULL)
      {
        if(currProcess->pid == current->pid)
        {
          foundProcess = true;
          break;
        }
        currProcess = currProcess->next;
      }
      if(foundProcess)
      {
        break;
      }
      currContainer = currContainer->next;
    }
    // now you have the target container
    if(currContainer == NULL)
    {
      printk("NO SUCH PROCESS IN CONTAINER LIST\n");
      mutex_unlock(&lock);
      return 0;
    }

    // Now search if the lock exists in the locks list of this container
    currLock = currContainer->headLock;
    while(currLock!=NULL)
    {
      if(currLock->oid == new_oid)
      {
        printk("The oid already exists! Lock it\n");
        mutex_lock(&(currLock->lockObject));
        mutex_unlock(&lock);
        return 0;
      }
      prevLock = currLock;
      currLock = currLock->next;
    }
    // we did not find the lock in the LIST
    //  create new lock object
    new_lock = (struct lock*) kcalloc(1, sizeof(struct lock), GFP_KERNEL);
    new_lock->oid = new_oid;
    mutex_init(&(new_lock->lockObject));
    mutex_lock(&(new_lock->lockObject));
    new_lock->next = NULL;

    if(prevLock ==NULL)
    {
      currContainer->headLock = new_lock;
    }
    else
    {
      prevLock->next = new_lock;
    }
    mutex_unlock(&lock);
    return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{

    int new_oid;
    struct container *currContainer = NULL;
    struct process *currProcess = NULL;
    struct lock *currLock = NULL;
    struct lock *prevLock = NULL;
    bool foundProcess = false;
    struct memory_container_cmd* userInfo = (struct memory_container_cmd*) kcalloc(1, sizeof(struct memory_container_cmd), GFP_KERNEL);
    mutex_lock(&lock);
    copy_from_user(userInfo,user_cmd,sizeof(struct memory_container_cmd));
    new_oid = (int)userInfo->oid;
    kfree(userInfo);
    userInfo = NULL;

    printk("searching container for oid: %d and pid: %d", new_oid, current->pid);
    currContainer = containerList;
    while(currContainer != NULL)
    {
      currProcess = currContainer->headProcess;
      while(currProcess != NULL)
      {
        if(currProcess->pid == current->pid)
        {
          foundProcess = true;
          break;
        }
        currProcess = currProcess->next;
      }
      if(foundProcess)
      {
        break;
      }
      currContainer = currContainer->next;
    }

    // now you have the target container
    if(currContainer == NULL)
    {
      printk("NO SUCH PROCESS IN CONTAINER LIST\n");
      mutex_unlock(&lock);
      return 0;
    }

    // Now search if the lock exists in the locks list of this container
    currLock = currContainer->headLock;
    while(currLock!=NULL)
    {
      if(currLock->oid == new_oid)
      {
        printk("The oid already exists! Lock it\n");
        mutex_unlock(&(currLock->lockObject));
        mutex_unlock(&lock);
        return 0;
      }
      prevLock = currLock;
      currLock = currLock->next;
    }
    printk("Lock not found!!!!!!!!");

    mutex_unlock(&lock);
    return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{

  mutex_lock(&lock);
  printk("\t\tprocessor_container_delete ****************************\n");
  printk("deleting PID: %d\n",(int)current->pid);
  removeProcess((int)current->pid);
  mutex_unlock(&lock);
  printk("\t\tprocessor_container_delete ****************************\n");
 // printMap();
  return 0;

}


int memory_container_create(struct memory_container_cmd __user *user_cmd)
{

  int new_cid;
  struct memory_container_cmd* userInfo = (struct memory_container_cmd*) kcalloc(1, sizeof(struct memory_container_cmd), GFP_KERNEL);
  mutex_lock(&lock);
  printk("\t\tprocessor_container_create ****************************\n");
  copy_from_user(userInfo,user_cmd,sizeof(struct memory_container_cmd));
  new_cid = (int)userInfo->cid;
  kfree(userInfo);
  userInfo = NULL;
  printk("creating CID: %d\tPID: %d\n",new_cid,(int)current->pid);
  addToContainer(new_cid, (int)current->pid);
  printMap();
  mutex_unlock(&lock);
  // above function unlocks mutex
  printk("\t\tprocessor_container_create ****************************\n");
  return 0;








    return 0;
}


int memory_container_free(struct memory_container_cmd __user *user_cmd)
{
    return 0;
}


/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int memory_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case MCONTAINER_IOCTL_CREATE:
        return memory_container_create((void __user *)arg);
    case MCONTAINER_IOCTL_DELETE:
        return memory_container_delete((void __user *)arg);
    case MCONTAINER_IOCTL_LOCK:
        return memory_container_lock((void __user *)arg);
    case MCONTAINER_IOCTL_UNLOCK:
        return memory_container_unlock((void __user *)arg);
    case MCONTAINER_IOCTL_FREE:
        return memory_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
