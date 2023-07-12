#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include "include/lines.h"
#include "include/users_llist.h"


// Users linked list head
static user_node * head_users = NULL;
int msg_not_copied = true;  // true = 1



/*** Client Functions ***/

int register_user (char* username){
    char result;
    if (strlen(username) < USERNAME_MAX) {
        if (get_user(head_users, username) != NULL) {
            // User already exists
            result = USER_OPERATION_FAIL_1;
        }
        else {
            //The user doesn't exist so it's created
            result = USER_OPERATION_SUCCESS;
            if (insert_user(&head_users, username) == -1){
                //Error while creating the user
                result = USER_OPERATION_FAIL_2;
            }
        }
    }
    else {
        // Username longer than expected
        result = USER_OPERATION_FAIL_2;
    }
    // print message on screen
    if (result == USER_OPERATION_SUCCESS){
        printf("s> REGISTER %s OK\n", username);
    } else {
        printf("s> REGISTER %s FAIL\n", username);
    }

    //Returns if the user has been registered or not
    return (result);
}


int unregister_user(char* username){
    char result;
    user_node *temp = NULL;

    if ( strlen(username) < USERNAME_MAX) {
        temp = get_user(head_users, username);
        if (temp == NULL) {
            // User does not exist
            result = USER_OPERATION_FAIL_1;
            printf("s> UNREGISTER %s OK\n", username);
        }
        else {
            result = USER_OPERATION_SUCCESS;
            // First free users pending messages list and then delete user
            int error_free_messages = free_penmessages(&temp->penmessages);
            int error_delete_user = del_user(&head_users, temp->username);
            if (error_free_messages == -1 || error_delete_user == -1){
                result = USER_OPERATION_FAIL_2;
                printf("s> UNREGISTER %s FAIL\n", username);
            }
        }
    }
    else {
        // Username longer than expected
        result = USER_OPERATION_FAIL_2;
        printf("s> UNREGISTER %s FAIL\n", username);
    }

    //Returns if the user has been registered or not
    return (result);

}

int send_ack(user_node* sender_user, u_int32_t message_id){
    /* Function to send the sender of a message an acknowledgment that it has been successfully sent to the recipient */
    // prepare to send message to recipient
    char user_ip[IP_MAX];
    strcpy(user_ip, sender_user->ip);
    u_int32_t user_port = sender_user->port;
    char operation[14], message_id_str[MESSAGE_ID_MAX];
    strcpy(operation, "SEND_MESS_ACK");
    sprintf(message_id_str, "%u", message_id);
    int sd;
    struct sockaddr_in user_addr;
    struct in_addr ip;
    struct hostent *hp;
    int err;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == 1) {
        perror("s> (SEND ACK) Error: socket creation\n");
        return -1;
    }

    bzero((char *)&user_addr, sizeof(user_addr));
    // as client IP address was saved as a string, we have to parse it first
    if (!inet_aton(user_ip, &ip)) {
        perror("s> (SEND ACK) Error: can't parse IP address");
        close(sd);
        return -1;
    }

    hp = gethostbyaddr(&ip, sizeof(ip), AF_INET);
    if (hp == NULL) {
        perror("s> (SEND ACK) Error: could not get host name\n");
        close (sd);
        return -1;
    }

    memcpy (&(user_addr.sin_addr), hp->h_addr, hp->h_length);
    user_addr.sin_family  = AF_INET;
    user_addr.sin_port    = htons(user_port);

    // if connect error, consider recipient disconnected
    err = connect(sd, (struct sockaddr *) &user_addr,  sizeof(user_addr));
    if (err == -1) {
        perror("s> (SEND ACK) Error: could not connect to recipient\n");
        if (modify_user_status(&head_users, sender_user->username, 0, "", 0) == -1){
            perror("s> (SEND ACK) Error: could not change user status.\n");
        }
        close (sd);
        return -1;
    }

    // send ack and message id
    err = send(sd, (char*) operation, strlen(operation)+1,0);
    if (err == -1){
        perror("s> (SEND ACK) Error sending operation\n");
        close (sd);
        return -1;
    }

    err = send(sd, (char*) message_id_str, strlen(message_id_str)+1,0);
    if (err == -1){
        perror("s> (SEND ACK) Error sending message id\n");
        close (sd);
        return -1;
    }

    close(sd);
    return 0;
}

int connect_and_send_message(user_node* sender_user, user_node* recipient_user, penmessage_node* message_node){
    /* Function to perform the socket creation and connection with client's listening thread to send a message
     * If success, proceed with ack to respective message sender */

    // prepare to send message to recipient
    char recipient_ip[IP_MAX],id[MESSAGE_ID_MAX], sender_name[USERNAME_MAX];
    strcpy(sender_name, sender_user->username);
    strcpy(recipient_ip, recipient_user->ip);
    sprintf(id,"%u", message_node->message_id);
    u_int32_t recipient_port = recipient_user->port;
    char operation[13];
    strcpy(operation, "SEND_MESSAGE");
    int sd;
    struct sockaddr_in user_addr;
    struct in_addr ip;
    struct hostent *hp;
    int err;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == 1) {
        perror("s> (SEND) Error: socket creation\n");
        if (insert_penmessage(&sender_user->penmessages, recipient_user->username, message_node->message_id, message_node->message) == -1){
            // from this point, if error, reinsert message to pending
            perror("s> (SEND) Error: could not store message in pending messages list.\n");
            return -1;
        }
        return -1;
    }

    bzero((char *)&user_addr, sizeof(user_addr));
    if (!inet_aton(recipient_ip, &ip)){
        perror("s> (SEND) Error: can't parse IP address");
        close(sd);
        return -1;
    }

    hp = gethostbyaddr(&ip, sizeof(ip), AF_INET);
    if (hp == NULL) {
        perror("s> (SEND) Error: could not get host name\n");
        if (insert_penmessage(&sender_user->penmessages, recipient_user->username, message_node->message_id, message_node->message) == -1){
            perror("s> (SEND) Error: could not store message in pending messages list.\n");
            return -1;
        }
        close (sd);
        return -1;
    }

    memcpy (&(user_addr.sin_addr), hp->h_addr, hp->h_length);
    user_addr.sin_family  = AF_INET;
    user_addr.sin_port    = htons(recipient_port);

    // if connect error, consider recipient disconnected
    err = connect(sd, (struct sockaddr *) &user_addr,  sizeof(user_addr));
    if (err == -1) {
        perror("s> (SEND) Error: could not connect to recipient\n");
        if (modify_user_status(&head_users, recipient_user->username, 0, "", 0) == -1){
            perror("s> (SEND) Error: could not change user status.\n");
        }
        if (insert_penmessage(&sender_user->penmessages, recipient_user->username, message_node->message_id, message_node->message) == -1){
            perror("s> (SEND) Error: could not store message in pending messages list.\n");
            return -1;
        }
        close (sd);
        return -1;
    }

    // send operation string "SEND_MESSAGE"
    err = send(sd, (char*) operation, strlen(operation)+1,0);
    if (err == -1){
        perror("s> (SEND) Error sending operation\n");
        if (insert_penmessage(&sender_user->penmessages, recipient_user->username, message_node->message_id, message_node->message) == -1){
            perror("s> (SEND) Error: could not store message in pending messages list.\n");
            return -1;
        }
        close (sd);
        return -1;
    }
    // send username
    err = sendMessage(sd, (char *)sender_name, strlen(sender_name)+1);
    if (err == -1) {
        perror("s> (SEND) Error sending sender's username\n");
        if (insert_penmessage(&sender_user->penmessages, recipient_user->username, message_node->message_id, message_node->message) == -1){
            perror("s> (SEND) Error: could not store message in pending messages list.\n");
            return -1;
        }
        close(sd);
        return -1;
    }

    // send message id
    err = sendMessage(sd, (char *)id, strlen(id)+1);
    if (err == -1) {
        perror("s> (SEND) Error sending message id\n");
        if (insert_penmessage(&sender_user->penmessages, recipient_user->username, message_node->message_id, message_node->message) == -1){
            perror("s> (SEND) Error: could not store message in pending messages list.\n");
            return -1;
        }
        close(sd);
        return -1;
    }

    //send message
    err = send(sd, (char*) message_node->message, strlen(message_node->message)+1,0);
    if (err == -1){
        perror("s> (SEND) (SEND) Error sending message\n");
        if (insert_penmessage(&sender_user->penmessages, recipient_user->username, message_node->message_id, message_node->message) == -1){
            perror("s> (SEND) Error: could not store message in pending messages list.\n");
            return -1;
        }
        close (sd);
        return -1;
    }

    close(sd);
    printf("s> SEND MESSAGE %u FROM %s TO %s\n", message_node->message_id, sender_user->username, recipient_user->username);

    // message send success, so send ack to sender
    send_ack(sender_user, message_node->message_id);
    return 0;
}


int connect_user (char* username, char* user_ip, u_int32_t user_port){
    char result;
    user_node * connect_user = NULL;
    connect_user = get_user(head_users, username);
    if (connect_user == NULL){
        //The user is not registered
        result = USER_OPERATION_FAIL_1;
        printf("s> CONNECT %s FAIL\n", username);
    }
    else {
        if (connect_user->status == 1){
            //The user is already connected
            result = USER_OPERATION_FAIL_2;
            printf("s> CONNECT %s FAIL\n", username);
        }
        else{
            //The user exist and is not connected, so it's updated
            if (modify_user_status(&head_users, username, 1, user_ip, user_port) == -1){
                result = USER_OPERATION_FAIL_3;
                printf("s> CONNECT %s FAIL\n", username);
            }
            else{
                result = USER_OPERATION_SUCCESS;
            }
        }
    }

    printf("s> CONNECT %s OK\n", username);
    return result;
}


int postconnection_penmessages(char* username){
    /* Need to perform two checks right after connection: messages to deliver to newly connected client, and messages
     * to send from newly connected client with the username passed as parameter */

    // Get the user node from the username
    user_node * connect_user = NULL;
    connect_user = get_user(head_users, username);
    if (connect_user == NULL){
        // The user is not registered
        return -1;
    }

    // First, receive the messages sent by the other users while client was disconnected
    user_node *current_user;
    current_user = head_users;
    penmessage_node *pending_message;
    while (current_user != NULL) {
        // check for all users with different name than the connected client, which are connected and have pending messages
        if ((strcmp(current_user->username, username) != 0) && (current_user->status == 1) && (current_user->penmessages != NULL)) {
            do {
                pending_message = pop_penmessage_by_recipient(&current_user, username);
                if (pending_message != NULL){ // send message to the newly connected user
                    connect_and_send_message(current_user, connect_user, pending_message);
                }
            // do while there are still pending messages for the current user, and the connected user has not disconnected
            } while(pending_message != NULL && current_user->penmessages != NULL && connect_user->status == 1);
        }
        current_user = current_user->next;
    }

    // Second, send all pending messages from the newly connected client if their recipient is connected
    pending_message = connect_user->penmessages;
    current_user = NULL;
    while (pending_message != NULL){
        current_user = get_user(head_users, pending_message->recipient);
        if (current_user != NULL && current_user->status == 1){
            // if recipient exists and is connected, send message
            connect_and_send_message(connect_user, current_user, pending_message);
        }
        pending_message = pending_message->next;
    }

    return 0;
}


int disconnect_user (char*username){
    char result;
    user_node *actual_user;
    actual_user =  get_user(head_users, username);

    if (actual_user == NULL){
        //The user is not registered
        result = USER_OPERATION_FAIL_1;
        printf("s> DISCONNECT %s FAIL\n", username);
    }

    else{
        if (actual_user->status == 0){
            // The user is already disconnected
            result = USER_OPERATION_FAIL_2;
            printf("s> DISCONNECT %s FAIL\n", username);
        }
        else{
            // The user exists and is connected, so it's updated
            if (modify_user_status(&head_users, username, 0, "0", 0) == -1){
                result = USER_OPERATION_FAIL_3;
                printf("s> DISCONNECT %s FAIL\n", username);
            }
            else{
                result = USER_OPERATION_SUCCESS;
                printf("s> DISCONNECT %s OK\n", username);
            }
        }
    }
    return result;
}


int send_message(request *req){
    /* Function called when client requests SEND operation. It connects to the listening thread of the recipient
     * after doing the appropriate checks and sends the data following the specification through the socket*/

    // check the names of both users
    user_node* sender_user;
    sender_user = get_user(head_users, req->username);
    if (sender_user == NULL) {
        perror("s> (SEND) Error: sender username does not exist.\n");
        return USER_OPERATION_FAIL_1;
    }

    user_node* recipient_user;
    recipient_user = get_user(head_users, req->recipient);
    if (recipient_user == NULL) {
        perror("s> (SEND) Error: recipient username does not exist\n");
        return USER_OPERATION_FAIL_1;
    }

    // store in pending messages to deliver
    if (insert_penmessage(&sender_user->penmessages, req->recipient, req->message_id, req->message) == -1){
        perror("s> (SEND) Error: could not store message in pending messages list.\n");
        return USER_OPERATION_FAIL_2;
    }

    // check if recipient is connected
    if (recipient_user->status == 0){
        printf("s> MESSAGE %u FROM %s TO %s STORED\n", req->message_id, req->username, req->recipient);
        return USER_OPERATION_SUCCESS;
    }
    else {
        penmessage_node* message_to_send;
        message_to_send = pop_penmessage_by_recipient(&sender_user, req->recipient);
        if (message_to_send != NULL) {
            // try to send message
            // if failed, message will be reinserted. If success, server will ack to sender
            connect_and_send_message(sender_user, recipient_user, message_to_send);
        }
    }

    return USER_OPERATION_SUCCESS;
}


int send_response(int sc,  char res){
    int err;
    err = sendMessage(sc, (char *)&res, sizeof(char));
    if (err == -1) {
        perror("s> Error sending response\n");
        close(sc);
        return -1;
    }
    return 0;
}

int send_message_id(int sc,  char* id){
    int err;
    err = sendMessage(sc, (char *)id, strlen(id)+1);
    if (err == -1) {
        perror("s> Error sending message id\n");
        close(sc);
        return -1;
    }
    return 0;
}



void*  process_request(void* received_req){
    /**  process request  **/
    char res;
    request *req;

    //  make local copy of the request and notify main thread when done
    pthread_mutex_lock(&mutex_req);
    req = received_req;
    msg_not_copied = false ; // FALSE = 0
    pthread_cond_signal(&cond_req);
    pthread_mutex_unlock(&mutex_req);

    printf("s> Processing request with operation %s...\n", req->op);
    if (strcmp(req->op, "REGISTER") == 0){
        res = register_user(req->username);
        send_response(req->sc, res);
    }
    else if (strcmp(req->op, "UNREGISTER") == 0){
        res = unregister_user(req->username);
        send_response(req->sc, res);
    }
    else if (strcmp(req->op, "CONNECT") == 0){
        res = connect_user(req->username, req->ip, req->port);
        send_response(req->sc, res);
        if (res == USER_OPERATION_SUCCESS) { // if successfull, check for pending messages to deliver
            sleep(1); // give time for client to start running listening thread
            postconnection_penmessages(req->username);
        }
    }
    else if (strcmp(req->op, "DISCONNECT") == 0){
        res = disconnect_user(req->username);
        send_response(req->sc, res);
    }
    else if (strcmp(req->op, "SEND") == 0){
        res = send_message(req);
        send_response(req->sc, res);
        if (res == USER_OPERATION_SUCCESS){ // if successful, respond to user with message id too
            char id_string[MESSAGE_ID_MAX];
            sprintf(id_string, "%u", req->message_id);
            send_message_id(req->sc, id_string);
        }
        pthread_exit(0);
    }
    else {
        perror("s> Error: Command not recognized.\n");
        pthread_exit(0);
    }

    close(req->sc);
    printf("s> Finished processing request with operation %s\n", req->op);
    pthread_exit(0);
}

int main(int argc, char **argv){

    int portNum ;
    if (argc < 3 || strcmp(argv[1], "-p")!=0){
        printf ( "s> Correct usage: ./server -p <port>\n");
        return(0);
    }
    else{
        struct hostent *host_init;
        host_init = gethostbyname("localhost");
        if (!host_init){
            perror("s> Error: could not get localhost\n");
            return -1;
        }
        portNum = atoi(argv[2]);
        printf("s> init server %s:%d\n", inet_ntoa(*((struct in_addr*)host_init->h_addr_list[0])),portNum);
    }


    // setting threads id and attributes
    pthread_attr_t t_attr;  // thread attributes
    pthread_t thid;
    pthread_attr_init(&t_attr);
    pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

    // mutex initialization
    pthread_mutex_init(&mutex_req, NULL);
    pthread_mutex_init(&mutex_llist_user, NULL);
    pthread_mutex_init(&mutex_llist_penmessage, NULL);
    pthread_cond_init(&cond_req, NULL);

    // global variable representing message id number
    u_int32_t message_id = 0;

    // socket creation
    struct sockaddr_in server_addr,  client_addr;
    socklen_t size;
    int sd, sc;
    int val;
    int err;
    char op[OPERATION_MAX], username[USERNAME_MAX], recipient[USERNAME_MAX], message[MESSAGE_MAX], port[PORT_MAX];

    if ((sd =  socket(AF_INET, SOCK_STREAM, 0))<0){
        perror ("SERVER: Error in socket");
        return (0);
    }
    val = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(int));

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(portNum);

    err = bind(sd, (const struct sockaddr *)&server_addr,
               sizeof(server_addr));
    if (err == -1) {
        perror("s> Error bind\n");
        return -1;
    }

    err = listen(sd, SOMAXCONN);
    if (err == -1) {
        perror("s> Error listen\n");
        return -1;
    }

    size = sizeof(client_addr);

    // server runs infinite loop to receive clients requests using worker model (create new thread per request)
    while (true) {
        printf("\ns> \n");
        sc = accept(sd, (struct sockaddr *)&client_addr, (socklen_t *)&size);
        if (sc == -1) {
            perror("s> Error: could not accept connection\n");
            return -1;
        }

        printf("s> Accepted connection IP: %s   Port: %d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));


        // receive strings (receive as many values as specified depending on the operation)
        err = readLine( sc, (char*) op, sizeof(op));
        if (err == -1) {
            perror("s> Error receiving operation\n");
            close(sc);
            continue;
        }

        err = readLine( sc, (char*) username, sizeof(username));
        if (err == -1) {
            perror("s> Error receiving username\n");
            close(sc);
            continue;
        }

        /*** encapsulate data to process ***/
        request req;
        strcpy(req.op, op);                                 // 1. operation string
        strcpy(req.username, username);                     // 2. username
        strcpy(req.recipient,"");                           // 3. recipient
        strcpy(req.message, "");                            // 4. message
        req.message_id = 0;                                 // 5. message id
        req.sc = sc;                                        // 6. socket descriptor (sc)
        strcpy(req.ip, "");                                 // 7. client ip
        req.port = 0;                                       // 8. client port

        if (strcmp(op, "SEND") == 0){
            err = readLine( sc, (char*) recipient, sizeof(recipient));
            if (err == -1) {
                perror("s> Error receiving recipient username\n");
                close(sc);
                continue;
            }

            err = readLine( sc, (char*) message, sizeof(message));
            if (err == -1) {
                perror("s> Error receiving message\n");
                close(sc);
                continue;
            }

            // assign recipient, message and message id
            strcpy(req.recipient, recipient);
            strcpy(req.message,message);
            req.message_id = message_id;
            if (message_id == UINT32_MAX){
                message_id = 0;
            } else {
                message_id++;
            }
        }
        else if (strcmp(op, "CONNECT") == 0){
            err = readLine( sc, (char*) port, sizeof(port));
            if (err == -1) {
                perror("s> Error receiving port\n");
                close(sc);
                continue;
                }
            // assign port and ip
            req.port = atoi(port);
            strcpy(req.ip, inet_ntoa(client_addr.sin_addr));
        }


        // convert numbers back to host format

        printf("s> Request received with operation %s\n", req.op);

//        process_request(&req);

        // create new worker thread to process the request
        pthread_create(&thid, &t_attr, process_request, (void *) &req);

        // wait for thread to copy message
        pthread_mutex_lock(&mutex_req);
        while (msg_not_copied)
            pthread_cond_wait(&cond_req, &mutex_req);
        msg_not_copied = true;
        pthread_mutex_unlock(&mutex_req);
    }

}

