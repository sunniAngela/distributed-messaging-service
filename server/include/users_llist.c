#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "users_llist.h"

/*********  LINKED LIST OF USER PENDING MESSAGES ************/
int insert_penmessage(penmessage_node** head, char* recipient, u_int32_t message_id, char* message){
    // insert new pending message to deliver at the end of the linked list (tail)
    penmessage_node *new_penmessage;
    new_penmessage = (penmessage_node*) malloc(sizeof(penmessage_node));
    if (!new_penmessage){
        return -1;
    }

    strcpy(new_penmessage->recipient, recipient);
    new_penmessage->message_id = message_id;
    strcpy(new_penmessage->message, message);
    new_penmessage->next = NULL;

    penmessage_node *current;
    current = (penmessage_node*) malloc(sizeof(penmessage_node));
    if (!current){
        return -1;
    }

    // if list is empty
    pthread_mutex_lock(&mutex_llist_penmessage);
    if (*head == NULL){
        *head = new_penmessage;
        pthread_mutex_unlock(&mutex_llist_penmessage);
        return 0;
    }

    // otherwise, traverse list until end and assign last node's next to new node
    current = *head;
    while (current->next != NULL){
        current = current->next;
    }
    current->next = new_penmessage;

    pthread_mutex_unlock(&mutex_llist_penmessage);
    return 0;
}


penmessage_node* pop_penmessage_by_recipient(user_node **user, char* recipient){
    // returns user's first (oldest) pending message to the specified recipient
    penmessage_node *aux = (penmessage_node*) malloc(sizeof(penmessage_node));
    if (!aux){
        return NULL;
    }

    // if head is message to recipient, pop head
    pthread_mutex_lock(&mutex_llist_penmessage);
    if (strcmp((*user)->penmessages->recipient, recipient) == 0){
        aux = (*user)->penmessages;
        (*user)->penmessages = (*user)->penmessages->next;
        pthread_mutex_unlock(&mutex_llist_penmessage);
        return aux;
    }
    pthread_mutex_unlock(&mutex_llist_penmessage);

    // otherwise, traverse list until found, update previous node's pointer and return the node
    penmessage_node *prev = (penmessage_node*) malloc(sizeof(penmessage_node));
    if (!prev){
        return NULL;
    }
    pthread_mutex_lock(&mutex_llist_penmessage);
    aux = (*user)->penmessages;
    while (aux != NULL){
        if (strcmp(aux->recipient, recipient) == 0){
            prev->next = aux->next;
            pthread_mutex_unlock(&mutex_llist_penmessage);
            return aux;
        }
        prev = aux;
        aux = aux->next;
    }

    pthread_mutex_unlock(&mutex_llist_penmessage);
    return NULL;
}


int free_penmessages(penmessage_node** head){
    // given head of the pending messages linked list, frees the entire list to prepare for user deletion
    penmessage_node *aux = (penmessage_node*) malloc(sizeof(penmessage_node));
    if (!aux){
        return -1;
    }
    pthread_mutex_lock(&mutex_llist_penmessage);
    while (*head != NULL){
        aux = *head;
        *head = (*head)->next;
        free(aux);
    }
    *head = NULL;
    pthread_mutex_unlock(&mutex_llist_penmessage);
    return 0;
}

int len_penmessages(penmessage_node* head){
    penmessage_node* current = (penmessage_node*) malloc(sizeof(penmessage_node));
    if (!current){
        return -1;
    }
    int counter = 0;

    pthread_mutex_lock(&mutex_llist_penmessage);
    current = head;
    while (current != NULL){
        current = current->next;
        counter++;
    }
    pthread_mutex_unlock(&mutex_llist_penmessage);
    return counter;
}


void print_penmessages(penmessage_node* head){
    penmessage_node *current = (penmessage_node*) malloc(sizeof(penmessage_node));
    current = head;
    while (current != NULL){
        printf("RECIPIENT USERNAME: %s, MESSAGE ID: %u, MESSAGE: %s\n", current->recipient, current->message_id,
               current->message);
        current = current->next;
    }

}






/***** LINKED LIST USERS *****/

int insert_user(user_node** head, char* username) {
    user_node *aux ;
    aux = (user_node*) malloc(sizeof(user_node));
    if (!aux){
        return -1;
    }
    // fill new node's fields
    strcpy(aux->username, username);
    aux->status = 0; // disconnected
    strcpy(aux->ip, "");
    aux->port = 0;
    aux->penmessages = NULL; // pointer to the head of the pending messages linked list
    aux->message_id = 0;

    pthread_mutex_lock(&mutex_llist_user);
    aux->next = *head;
    *head = aux; // new head
    pthread_mutex_unlock(&mutex_llist_user);
    return 0;
}

int len_users(user_node* head){
    user_node *current = (user_node*) malloc(sizeof(user_node));
    if (!current){
        return -1;
    }
    int counter = 0;
    pthread_mutex_lock(&mutex_llist_user);
    current = head;
    while (current != NULL){
        current = current->next;
        counter++;
    }
    pthread_mutex_unlock(&mutex_llist_user);
    return counter;
}


user_node* get_user(user_node* head, char* username){
    user_node *current = (user_node*) malloc(sizeof(user_node));
    if (!current){
        return NULL;
    }
    pthread_mutex_lock(&mutex_llist_user);
    current = head;
    while (current != NULL){
        if (strcmp(current->username, username) == 0){
            pthread_mutex_unlock(&mutex_llist_user);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&mutex_llist_user);
    return NULL;
}

int modify_user_status(user_node** head, char* username,char status, char* ip, u_int32_t port){
    if (status < 0 || status > 1){
        return -1;
    }

    user_node *current = (user_node*) malloc(sizeof(user_node));
    if (!current){
        return -1;
    }

    pthread_mutex_lock(&mutex_llist_user);
    // get user first
    current = *head;
    while (current != NULL){
        if (strcmp(current->username, username) == 0){
            break;
        }
        current = current->next;
    }

    if (status == 1){
        // if client is connecting, update IP and port
        strcpy(current->ip, ip);
        current->port = port;
    } else {
        strcpy(current->ip, "\0");
        current->port = 0;
    }
    current->status = status;
    pthread_mutex_unlock(&mutex_llist_user);

    return 0;
}


int del_user(user_node** head, char* username){
    user_node *current = (user_node*) malloc(sizeof(user_node));
    if (!current){
        return -1;
    }
    user_node *prev = (user_node*) malloc(sizeof(user_node));
    if (!prev){
        return -1;
    }
    pthread_mutex_lock(&mutex_llist_user);
    current = *head;
    prev = NULL;

    while (current != NULL){
        if (strcmp((*head)->username, username) == 0){ // element to delete is head
            prev = *head; // using prev as temporal pointer to head
            *head = (*head)->next; // update new head
            free(prev);
            pthread_mutex_unlock(&mutex_llist_user);
            return 0;
        }
        else if (strcmp(current->username, username) == 0){ // any other element in the list
            prev->next = current->next;
            free(current);
            pthread_mutex_unlock(&mutex_llist_user);
            return 0;
        }
        prev = current;
        current = current->next;
    }
    pthread_mutex_unlock(&mutex_llist_user);
    return -1;
}

int free_users(user_node** head){
    user_node *aux = (user_node*) malloc(sizeof(user_node));
    if (!aux){
        return -1;
    }
    pthread_mutex_lock(&mutex_llist_user);
    while (*head != NULL){
        aux = *head;
        *head = (*head)->next;
        free(aux);
    }
    *head = NULL;
    pthread_mutex_unlock(&mutex_llist_user);
    return 0;
}


void print_users(user_node* head){
    user_node *current = (user_node*) malloc(sizeof(user_node));
    current = head;
    while (current != NULL){
        printf("USER: %s, STATUS: %d, IP: %s, PORT: %d, LAST MESSAGE RECEIVED: %d\n", current->username,
               current->status, current->ip, current->port, current->message_id);
        current = current->next;
    }
}



