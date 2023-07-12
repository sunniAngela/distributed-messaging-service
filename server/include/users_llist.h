#ifndef DDSS_EXERCISE01_LIBKEYS_H
#define DDSS_EXERCISE01_LIBKEYS_H
#include <stddef.h>

#define USERNAME_MAX 50
#define IP_MAX 16
#define PORT_MAX 6
#define MESSAGE_MAX 256
#define MESSAGE_ID_MAX 11
#define OPERATION_MAX 15

#define USER_OPERATION_SUCCESS  0
#define USER_OPERATION_FAIL_1   1
#define USER_OPERATION_FAIL_2   2
#define USER_OPERATION_FAIL_3   3

// mutex, condition variables and global variable for proper message copy and array protection
pthread_mutex_t mutex_req,mutex_llist_user, mutex_llist_penmessage;
pthread_cond_t cond_req;

typedef struct request{
    char op[OPERATION_MAX];
    char username[USERNAME_MAX];
    char recipient[USERNAME_MAX];
    char message[MESSAGE_MAX];
    u_int32_t message_id;
    int  sc;
    char ip[IP_MAX];
    u_int32_t port;
}request;

typedef struct penmessage_node{
    char recipient[USERNAME_MAX];
    u_int32_t message_id;
    char message[MESSAGE_MAX];
    struct penmessage_node *next;

}penmessage_node;

typedef struct user_node {
    char username[USERNAME_MAX];
    char status;
    char ip[IP_MAX];
    u_int32_t port;
    penmessage_node *penmessages;
    u_int32_t message_id;
    struct user_node *next;
}user_node;


int insert_penmessage(penmessage_node** head, char* recipient, u_int32_t message_id, char* message);
penmessage_node* pop_penmessage_by_recipient(user_node **user, char* recipient);
int free_penmessages(penmessage_node** head);
int len_penmessages(penmessage_node* head);
void print_penmessages(penmessage_node* head);

int insert_user(user_node** head, char* username);
int len_users(user_node* head);
user_node* get_user(user_node* head, char* username);
int modify_user_status(user_node** head, char* username,char status, char* ip, u_int32_t port);
int del_user(user_node** head, char* username);
int free_users(user_node** head);
void print_users(user_node* head);

#endif
