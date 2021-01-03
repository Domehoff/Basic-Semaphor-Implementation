#include <linux/cs1550.h>
DEFINE_SPINLOCK(listLock);
int semID = 0;
struct cs1550_sem_list *mainList = NULL;

//mainList = (cs1550_sem_list*)kmalloc(sizeof(cs1550_sem_list));

asmlinkage long sys_cs1550_create(int value, char name[32], char key[32]){
//allocate memory for the semaphore structure
int i = 0;
struct cs1550_sem *newSem = NULL;
struct cs1550_sem *tempSem = NULL;
//printk(KERN_WARNING "attempting to create semaphore # %d \n",  semID);
newSem = (struct cs1550_sem*)kmalloc(sizeof(struct cs1550_sem), GFP_ATOMIC);
newSem->sem_id = semID;
newSem->value = value;
//initialize the name and key fields
for(i = 0; i < 33; i++)
{
  newSem->name[i] = name[i];
}
for(i = 0; i < 33; i++)
{
  newSem->key[i] = key[i];
}
//set next to be NULL
newSem->next = NULL;
//create and initialize the per-sem spinlock for the new sem
spin_lock_init(&newSem->lock);
//if this is the first semaphore, create the semaphore list and
//add the new semaphore as the head
if(mainList == NULL)
{
  //printk(KERN_WARNING "mainList was NULL, creating new semaphore list \n");
  mainList = (struct cs1550_sem_list*)kmalloc(sizeof(struct cs1550_sem_list), GFP_ATOMIC);
  mainList->head = newSem;
  //printk(KERN_WARNING "NEW mainList created!, new semaphore is the head\n");
}
else
{
  //printk(KERN_WARNING "mainList located, traversing...\n");
  //else traverse the list of semaphores until there the curr semaphore's
  //next pointer is null and add in the new semaphore
  spin_lock(&listLock);
  if(mainList->head == NULL)
  {
    mainList->head = newSem;
    semID = semID + 1;
    spin_unlock(&listLock);
    return newSem->sem_id;
  }
  tempSem = mainList->head;
  while(tempSem->next != NULL)
  {
    tempSem = tempSem->next;
  }
  tempSem->next = newSem;
  //printk(KERN_WARNING "NEW semaphore added to list, # %ld \n", newSem->sem_id);
  spin_unlock(&listLock);
}
//increment the semaphore counter and return the new semaphore's ID
semID = semID + 1;
//printk(KERN_WARNING "semaphore # %ld created and added to list. \n",  newSem->sem_id);
return newSem->sem_id;
}

asmlinkage long sys_cs1550_open(char name[32], char key[32]){
  struct cs1550_sem *tempSem = NULL;
  //printk(KERN_WARNING "Attempting to open semaphore...\n");
  //create sem pointer to traverse the list, enter sem list spin lock
  spin_lock(&listLock);
  if(mainList->head == NULL)
  {
    return -1;
  }
  tempSem = mainList->head;
  while(tempSem != NULL)
  {
    //printk(KERN_WARNING "Traversing semaphore list...\n");
    //traversing the list, while the pointer is not null,
    //if the names match, then check the keys
    if(tempSem->name == name)
    {
      //names match, check keys
      if(tempSem->key == key)
      {
        //once the keys match, return the curr semaphore's ID
        spin_unlock(&listLock);
        //printk(KERN_WARNING "FOUND returning sem id: # %ld \n", tempSem->sem_id);
        return tempSem->sem_id;
      }
    }
    //if there was no match, move to the next semaphore in the list
    tempSem = tempSem->next;
  }
  spin_unlock(&listLock);
  //if tempSem becomes NULL, the loop will break and the failure flag
  //will be returned
  return -1;
}


asmlinkage long sys_cs1550_down(long sem_id){
  //enter global spinlock, traverse semaphore list and find semaphore match with ID
  //then leave the spinlock
  struct cs1550_sem *tempSem = NULL;
  //printk(KERN_WARNING "Attempting to down() opened semaphore # %ld...\n", sem_id);
  spin_lock(&listLock);
  if(mainList->head == NULL)
  {
    return -1;
  }
  tempSem = mainList->head;
  while(tempSem != NULL)
  {
    //printk(KERN_WARNING "Traversing semaphore list...\n");
    if(tempSem->sem_id == sem_id)
    {
      //semaphore found, leave the loop
      //printk(KERN_WARNING "FOUND semaphore # %ld, moving on to critical section...\n", tempSem->sem_id);
      break;
    }
    tempSem = tempSem->next;
  }
  //if the value of the sem pointer is NULL,either the last semaphore is the match, or no matches
  //were found, and thus must return the failure flag
  if(tempSem == NULL)
  {
    //printk(KERN_WARNING "COULD NOT FIND semaphore # %ld. returning -1...\n", sem_id);
    spin_unlock(&listLock);
    return -1;
  }
  //unlock the global spinlock, we are done accessing the semaphore list
  spin_unlock(&listLock);
  //enter the critical section
 //printk(KERN_WARNING "Entering critical section in down()...\n");
  spin_lock(&tempSem->lock);
  //value -= 1;
  tempSem->value = tempSem->value - 1;
  //printk(KERN_WARNING "DECREMENTED SEMAPHORE VALUE : value is now %d \n", tempSem->value);
  if(tempSem->value < 0) // <-- add process to the list
  {
    //printk(KERN_WARNING "Semaphore's value was < 0, adding this process to the list.\n");
    //if the proc list is NULL, this is thye first waiting task.
    //initialize it and add the new task
    if(tempSem->head == NULL)
    {
      struct task_node *tempTask = NULL;
      //printk(KERN_WARNING "This semaphore's process list was unintialized, initializing...\n");
      tempTask = (struct task_node*)kmalloc(sizeof(struct task_node), GFP_ATOMIC);
      tempTask->next = NULL;
      tempTask->nodeproc = current;
      tempSem->head = tempTask;
      //printk(KERN_WARNING "SUCCESS new process list created for semaphore # %ld \n", tempSem->sem_id);
    }
    else
    {
      //else the list already has processes, so traverse the task list
      //until you reach the end and add the "current" process to the semaphore's list
      struct task_node *tempTask2 = NULL;
      struct task_node *tempTask = tempSem->head;
      //printk(KERN_WARNING "Semaphore's process list found, traversing...\n");
      if(tempTask->next == NULL)
      {
        //then there is one task in the list.
        tempTask2 = (struct task_node*)kmalloc(sizeof(struct task_node), GFP_ATOMIC);
        tempTask->next = tempTask2;
        tempTask2->nodeproc = current;
        tempTask2->next = NULL;
        //printk(KERN_WARNING "SUCCESS new process added to waiting queue.\n");
        spin_unlock(&tempSem->lock);
        set_current_state(TASK_INTERRUPTIBLE);
    		schedule();
        return 0;
      }
      while(tempTask->next != NULL)
      {
        tempTask = tempTask->next;
      }
      //reached the end of the list, add new task initialized above.
      //printk(KERN_WARNING "End of list reached, adding new process...\n");
      tempTask2 = (struct task_node*)kmalloc(sizeof(struct task_node), GFP_ATOMIC);
      tempTask->next = tempTask2;
      tempTask2->nodeproc = current;
      tempTask2->next = NULL;
      //printk(KERN_WARNING "SUCCESS new process added to waiting queue.\n");
    }
    //leave critical section, mark current task as unready, and envoke the scheduler
    spin_unlock(&tempSem->lock);
    set_current_state(TASK_INTERRUPTIBLE);
		schedule();
    return 0;
  }
  spin_unlock(&tempSem->lock);
  //printk(KERN_WARNING "down() operation successful.\n");
  return 0;
}


asmlinkage long sys_cs1550_up(long sem_id){
//traverse the semaphore list until a match is found
struct cs1550_sem *tempSem = NULL;
//printk(KERN_WARNING "Attemping to up() opened semaphore # %ld \n", sem_id);
spin_lock(&listLock);
if(mainList->head == NULL)
{
  return -1;
}
tempSem = mainList->head;
while(tempSem != NULL)
{
  //printk(KERN_WARNING "Traversing semaphore list...\n");
  if(tempSem->sem_id == sem_id)
  {
    //semaphore found, leave the loop
    //printk(KERN_WARNING "FOUND semaphore # %ld, moving on to critical section...\n", tempSem->sem_id);
    break;
  }
  tempSem = tempSem->next;
}
//if the value of the sem pointer is NULL, no matches
//were found, and thus must return the failure flag
if(tempSem == NULL)
{
  //printk(KERN_WARNING "COULD NOT FIND semaphore # %ld, returning -1...\n", sem_id);
  return -1;
}
spin_unlock(&listLock);
//enter the critical section
//printk(KERN_WARNING "Entering critical section in up()...\n");
spin_lock(&tempSem->lock);
//increment semaphore's value
tempSem->value = tempSem->value + 1;
//printk(KERN_WARNING "INCREMENTED SEMAPHORE VALUE : value is now %d \n", tempSem->value);
if(tempSem->value <= 0) //if(value <= 0) <-- remove process from the list
{
  struct task_struct *wakeUP = NULL;
  struct task_node *pointer = NULL;
  //printk(KERN_WARNING "Semaphore's value was =< 0, removing process from list.\n");

  //remove the first task in the structure
  if(tempSem->head->next == NULL)
  {
    //it is the only process in the list
    wakeUP = tempSem->head->nodeproc;
    kfree(tempSem->head);
    tempSem->head = NULL;
  }
  else
  {
    //else grab the next pointer and set it to head after
    //the kfree()
    wakeUP = tempSem->head->nodeproc;
    pointer = tempSem->head->next;
    kfree(tempSem->head);
    tempSem->head = pointer;
  }
  //printk(KERN_WARNING "SUCCESS removed process in semaphore # %ld \n", tempSem->sem_id);
  //leave critical section
  //wake up sleeping process put to sleep by down()
  wake_up_process(wakeUP);
  //printk(KERN_WARNING "Woke up semaphore # %ld 's next waiting proccess.\n", tempSem->sem_id);
}
spin_unlock(&tempSem->lock);
return 0;
}

asmlinkage long sys_cs1550_close(long sem_id){
  struct cs1550_sem *tempSem = NULL;
  struct cs1550_sem *placeHldr = NULL;
  //printk(KERN_WARNING "Attempting to remove semaphore # %ld...\n", sem_id);
  spin_lock(&listLock);
  if(mainList->head == NULL)
  {
    return -1;
  }
  tempSem = mainList->head;
  while(tempSem != NULL)
  {
  //  printk(KERN_WARNING "Traversing semaphore list...\n");
    //if the very first node matches
    if(tempSem->sem_id == sem_id)
    {
      //its the only semaphore in the list
      if(tempSem->next == NULL)
      {
        kfree(tempSem);
        mainList->head = NULL;
        spin_unlock(&listLock);
        //printk(KERN_WARNING "SUCCESS semaphore # %ld removed from list.\n", sem_id);
        return 0;
      }
      else
      {
        placeHldr = tempSem;
        mainList->head = mainList->head->next;
        kfree(placeHldr);
        spin_unlock(&listLock);
        //printk(KERN_WARNING "SUCCESS semaphore # %ld removed from list.\n", sem_id);
        return 0;
      }
    }
    //check the ->next semaphore's id to ensure it is possible to
    //reconnect the list if the element being removed is not the last
    if(tempSem->next != NULL)
    {
      if(tempSem->next->sem_id == sem_id)
      {
            //case if the element being removed is the last element
            if(tempSem->next->next == NULL)
            {
              //printk(KERN_WARNING "SUCCESS semaphore to be deleted was found.\n");
              //move pointer to the semaphore to be deleted
              kfree(tempSem->next);
              tempSem->next = NULL;
              spin_unlock(&listLock);
              //printk(KERN_WARNING "SUCCESS semaphore # %ld removed from list.\n", sem_id);
              return 0;
            }
            //else we will have to
            else
            {
              //printk(KERN_WARNING "SUCCESS semaphore to be deleted was found.\n");
              placeHldr = tempSem->next;
              tempSem->next = tempSem->next->next;
              kfree(placeHldr);
              spin_unlock(&listLock);
              //printk(KERN_WARNING "SUCCESS semaphore # %ld removed from list.\n", sem_id);
              return 0;
            }
          }
      }
    //if there was no match, move to the next semaphore in the list
    tempSem = tempSem->next;
  }

  spin_unlock(&listLock);
  //printk(KERN_WARNING "FAILED semaphore # %ld was not found.\n", sem_id);
  //if tempSem becomes NULL, the loop will break and the failure flag
  //will be returned
  return -1;
}
