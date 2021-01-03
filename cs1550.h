#include <linux/smp_lock.h>

struct cs1550_sem
{
   int value;
   long sem_id;
   spinlock_t lock;
   char key[32];
   char name[32];
   struct cs1550_sem *next;
   //FIFO queue
   struct task_node  *head;
};

struct cs1550_sem_list
{
  struct cs1550_sem *head;
};

struct task_node
{
  struct task_struct *nodeproc;
  struct task_node *next;
};


