#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_node_t *node, *next_node;

    for (node = sleep_queue.next, next_node = node->next;
         node != &sleep_queue;
         node = next_node, next_node = node->next)
    {
        pcb_t *task_to_wake = list_entry(node, pcb_t, list);

        if (get_timer() >= task_to_wake->wakeup_time) {
            do_unblock(node);
        }
    }
}
