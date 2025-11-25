#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define MBOX1_KEY "mbox1"
#define MBOX2_KEY "mbox2"

void client_a_work(int loc) {
    int m1 = sys_mbox_open(MBOX1_KEY);
    int m2 = sys_mbox_open(MBOX2_KEY);
    char buf[10];

    sys_move_cursor(0, loc);
    printf("[A] SENDING to mbox1 (Full)...");
    sys_mbox_send(m1, "x", 1); // BLOCKS HERE
    printf("[A] Reading mbox2...");
    sys_mbox_recv(m2, buf, 1);
}

void client_b_work(int loc) {
    int m1 = sys_mbox_open(MBOX1_KEY);
    int m2 = sys_mbox_open(MBOX2_KEY);
    char buf[10];

    sys_move_cursor(0, loc);
    printf("[B] SENDING to mbox2 (Full)...");
    sys_mbox_send(m2, "y", 1); // BLOCKS HERE
    printf("[B] Reading mbox1...");
    sys_mbox_recv(m1, buf, 1);
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "A") == 0) { client_a_work(atoi(argv[2])); return 0; }
        if (strcmp(argv[1], "B") == 0) { client_b_work(atoi(argv[2])); return 0; }
    }

    sys_move_cursor(0, 1);
    printf("--- Deadlock Reproduction ---");
    int m1 = sys_mbox_open(MBOX1_KEY);
    int m2 = sys_mbox_open(MBOX2_KEY);

    // Fill mailboxes
    char data = 'a';
    for(int i=0; i<64; i++) { sys_mbox_send(m1, &data, 1); sys_mbox_send(m2, &data, 1); }

    char *arg_a[] = {"deadlock", "A", "4", NULL};
    char *arg_b[] = {"deadlock", "B", "5", NULL};
    sys_exec("deadlock", 3, arg_a);
    sys_exec("deadlock", 3, arg_b);
    return 0;
}
