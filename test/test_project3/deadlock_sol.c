#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define MBOX1_KEY "mbox1"
#define MBOX2_KEY "mbox2"

// --- Thread Functions ---
// Note: We cast the void* arg back to int (mailbox id)
void sender_func(void *arg) {
    int mbox_id = (int)(long)arg;
    sys_move_cursor(0, 6);
    printf("   [Thread-Send] Sending... ");
    sys_mbox_send(mbox_id, "x", 1);
    printf("Done! ");
    sys_exit();
}

void recver_func(void *arg) {
    int mbox_id = (int)(long)arg;
    char buf[10];
    sys_sleep(1); // Ensure the other thread has time to try blocking
    sys_move_cursor(0, 7);
    printf("   [Thread-Recv] Recving... ");
    sys_mbox_recv(mbox_id, buf, 1);
    printf("Done! ");
    sys_exit();
}

void client_a_work() {
    int m1 = sys_mbox_open(MBOX1_KEY);
    int m2 = sys_mbox_open(MBOX2_KEY);
    
    // Create threads: One sends to M1, one receives from M2
    sys_thread_create(sender_func, (void*)(long)m1);
    sys_thread_create(recver_func, (void*)(long)m2);
    
    while(1) sys_sleep(1);
}

void client_b_work() {
    int m1 = sys_mbox_open(MBOX1_KEY);
    int m2 = sys_mbox_open(MBOX2_KEY);

    // Create threads: One sends to M2, one receives from M1
    sys_thread_create(sender_func, (void*)(long)m2);
    sys_thread_create(recver_func, (void*)(long)m1);

    while(1) sys_sleep(1);
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "A") == 0) { client_a_work(); return 0; }
        if (strcmp(argv[1], "B") == 0) { client_b_work(); return 0; }
    }

    sys_move_cursor(0, 1);
    printf("--- Deadlock Solution ---");
    
    int m1 = sys_mbox_open(MBOX1_KEY);
    int m2 = sys_mbox_open(MBOX2_KEY);

    // Fill mailboxes
    char data = 'a';
    for(int i=0; i<64; i++) { sys_mbox_send(m1, &data, 1); sys_mbox_send(m2, &data, 1); }

    char *arg_a[] = {"deadlock_sol", "A", NULL};
    char *arg_b[] = {"deadlock_sol", "B", NULL};
    sys_exec("deadlock_sol", 2, arg_a);
    sys_exec("deadlock_sol", 2, arg_b);
    return 0;
}
