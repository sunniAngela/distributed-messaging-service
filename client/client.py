from enum import Enum
import argparse
import socket
import threading
import zeep


class client:
    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum):
        OK = 0
        ERROR = 1
        USER_ERROR = 2
        FAIL = 3

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    server_sock = None
    rcv_msg_sock = None
    connected = True
    thread = None
    _send=0
    server_connection=None
    username = None
    listn_server = None
    listn_port = None

    # ******************** METHODS *******************
    # *
    # waits and decode the response into integer base 10
    @staticmethod
    def wait_response():
        msg = client.server_sock.recv(1)
        # declaring byte value
        byte_val = msg
        # converting to int
        # byteorder is big where MSB is at start
        msg = int.from_bytes(byte_val, "big")
        # print(msg)
        if client._send != 1:
            err = client.server_sock.close()
            if (err is not None):
                print("Error cannot close the socket \n")
                exit()

        return (msg)

    @staticmethod
    def remove_spaces(string):
        """Calling function from web service (spyne) to remove double spaces of input strings"""
        wsdl = "http://localhost:8080/?wsdl"
        client_temp = zeep.Client(wsdl=wsdl)
        string = client_temp.service.remove(string)
        return string

    @staticmethod
    def id_response():
        """Function to receive the message id"""
        message = ''
        while True:
            msg = client.server_sock.recv(1)
            if (msg == b'\0'):
                break
            message += msg.decode()
        return (message)


    @staticmethod
    def connect_server():
        # waits and decode the response into integer base 10
        ADDR = (client._server, client._port)
        client.server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.server_sock.connect(ADDR)
        return 0

    @staticmethod
    def send_str(msg):
        """Function to send string to server"""
        # Convert a string to bytes and then add a byte null in the end to send through socket
        msg = client.remove_spaces(msg)
        msg_byte = msg.encode('utf-8') + b'\0'
        err = client.server_sock.sendall(msg_byte)
        return err

    @staticmethod
    def run_thread():
        client.thread = threading.Thread(target=client.receive_message)
        client.thread.setDaemon(True)
        client.thread.start()


    @staticmethod
    def get_listening_port():
        client.rcv_msg_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.rcv_msg_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        client.listn_server = socket.gethostbyname("localhost")
        server_address = (client.listn_server, 0)
        client.rcv_msg_sock.bind(server_address)
        client.listn_port = client.rcv_msg_sock.getsockname()[1]
        print('listening on :', client.listn_server, client.listn_port)
        return client.listn_port

    @staticmethod
    def listner_disconnect_sig():
        ADDR = (client.listn_server, client.listn_port)
        listn_server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listn_server_sock.connect(ADDR)
        message = b'Disconnect\0'
        listn_server_sock.sendall(message)
        return 0

    @staticmethod
    def get_msg( size):
        message = ''
        while True:
            msg = client.server_connection.recv(size)
            if (msg == b'\0'):
                break

            msg=msg.decode()
            message += msg

        # print("the complete string is {} \n".format(message)  )
        return message



    @staticmethod
    def receive_message():
        print('waiting for a connection in listening thread...')
        # if client has disconnected, thread will finish
        while client.connected:
            size_rcv = 1
            client.rcv_msg_sock.listen(9)
            client.server_connection, client_address = client.rcv_msg_sock.accept()
            print('connection from', client_address)
            first_response = client.get_msg(size_rcv)
            if first_response == "SEND_MESSAGE":
                pending_msgs = 3
                complete_message = []
                # receive the sender username, the message id, and the message body
                for i in range(pending_msgs):
                    complete_message.append(client.get_msg(size_rcv))

                print("MESSAGE {} FROM {} :\n  {} \nEND \n".format(complete_message[1], complete_message[0],
                                                           complete_message[2]))
            elif first_response == "SEND_MESS_ACK" :
                msg = client.get_msg(size_rcv)
                print("SEND MESSAGE id {} OK\n".format(msg))
            elif first_response == "Disconnect" :
                break
            client.server_connection.close()  # Closes the connection after completing one interaction
        print("Thread stopped successfully \n")
        client.server_connection.close() # Closes the connection if we call disconnect()










    # * @param user - User name to register in the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def register(user):
        # Sending operation to server
        oper = b'REGISTER\0'
        err = client.server_sock.sendall(oper)
        if (err is not None):
            print("Error cannot send Operation {} to server \n".format(oper[:-1].decode()))
            exit()

        # Sending User Name to server
        err = client.send_str(user)
        if (err is not None):
            print("Error cannot send Username {} to server \n".format(user))
            exit()
        response  = client.wait_response()

        if response == 0:
            print("REGISTER OK\n")
            # save username in attribute
            client.username = user
            return client.RC.OK
        elif response == 1:
            print("USERNAME IN USE\n")
            return client.RC.ERROR
        elif response == 2:
            print("REGISTER FAIL\n")
            return client.RC.USER_ERROR

    # *
    # 	 * @param user - User name to unregister from the system
    # 	 *
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def unregister(user):
        # Sending operation to server
        oper = b'UNREGISTER\0'
        err = client.server_sock.sendall(oper)
        if (err is not None):
            print("Error cannot send Operation {} to server \n".format(oper[:-1].decode()))
            exit()

        # Sending User Name to server
        err = client.send_str(user)
        if (err is not None):
            print("Error cannot send Username {} to server \n".format(user))
            exit()

        response  = client.wait_response()

        if response == 0:
            print("UNREGISTER OK\n")
            return client.RC.OK
        elif response == 1:
            print("USER DOES NOT EXIST \n")
            return client.RC.ERROR
        elif response == 2:
            print("UNREGISTER FAIL \n")
            return client.RC.USER_ERROR

    # *
    # * @param user - User name to connect to the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def connect(user):
        oper = b'CONNECT\0'
        err = client.server_sock.sendall(oper)
        if (err is not None):
            print("Error cannot send Operation {} to server \n".format(oper[:-1].decode()))
            exit()

        # Sending User Name to server
        err = client.send_str(user)
        if (err is not None):
            print("Error cannot send Username {} to server \n".format(user))
            exit()

        # Get Clients receiving port and sending to server
        port = client.get_listening_port()


        #print(port)
        err = client.send_str(str(port))
        if (err is not None):
            print("Error cannot send Port number to server \n")
            exit()
        response  = client.wait_response()

        if response == 0:
            #Setting up a listening thread
            client.connected = True
            client.run_thread()
            print("CONNECT OK\n")
            return client.RC.OK
        elif response == 1:
            print("CONNECT FAIL , USER DOES NOT EXIST\n")
            return client.RC.ERROR
        elif response == 2:
            print("USER ALREADY CONNECTED\n")
            return client.RC.USER_ERROR
        elif response == 3:
            print("CONNECT FAIL \n")
            return client.RC.FAIL


    # *
    # * @param user - User name to disconnect from the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def disconnect(user):
        oper = b'DISCONNECT\0'
        err = client.server_sock.sendall(oper)
        if (err is not None):
            print("Error cannot send Operation {} to server \n".format(oper[:-1].decode()))
            exit()

        # Sending User Name to server
        err = client.send_str(user)
        if (err is not None):
            print("Error cannot send Username {} to server \n".format(user))
            exit()
        response = client.wait_response()


        if response == 0:
            print("DISCONNECT OK\n")
            client.listner_disconnect_sig()
            if client.thread.is_alive():
                print("Stopping listening thread... \n")
                return
            else:
                return client.RC.OK
        elif response == 1:
            print("DISCONNECT FAIL / USER DOES NOT EXIST\n")
            return client.RC.ERROR
        elif response == 2:
            print("DISCONNECT FAIL / USER NOT CONNECTED\n")
            return client.RC.USER_ERROR
        elif response == 3:
            print("DISCONNECT FAIL\n")
            return client.RC.FAIL




    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # *
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def send(user,user2, message):
        oper = b'SEND\0'
        err = client.server_sock.sendall(oper)
        if (err is not None):
            print("Error cannot send Operation {} to server \n".format(oper[:-1].decode()))
            exit()

        # Sending username to the server
        err = client.send_str(user)
        if (err is not None):
            print("Error cannot send Username {} to server \n".format(user))
            exit()

        err = client.send_str(user2)
        if (err is not None):
            print("Error cannot send Username {} to server \n".format(user2))
            exit()

        # sending message to server
        err = client.send_str(message)
        if (err is not None):
            print("Error cannot send Username {} to server \n".format(message))
            exit()

        client._send = 1
        response = client.wait_response()
        if response == 0:
            client._send = 0
            message_id = client.id_response()
            print("SEND OK - MESSAGE id {} ".format(message_id))
            return client.RC.OK
        elif response == 1:
            print("SEND FAIL / USER DOES NOT EXIST")
            return client.RC.ERROR
        elif response == 2:
            print("SEND FAIL")
            return client.RC.USER_ERROR


    # *
    # * @param user    - Receiver user name
    # * @param file    - file  to be sent
    # * @param message - Message to be sent
    # *
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def sendAttach(user, file, message):
        #  Write your code here
        return client.RC.ERROR

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True):
            try:
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):
                    #since we are supposed to disconnect after every function
                    client.connect_server()
                    line[0] = line[0].upper()

                    if (line[0] == "REGISTER"):
                        if (len(line) == 2):
                            client.register(line[1])
                        else:
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif (line[0] == "UNREGISTER"):
                        if (len(line) == 2):
                            client.unregister(line[1])
                        else:
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif (line[0] == "CONNECT"):
                        if (len(line) == 2):
                            client.connect(line[1])
                        else:
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif (line[0] == "DISCONNECT"):
                        if (len(line) == 2):
                            client.disconnect(line[1])
                        else:
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif (line[0] == "SEND"):
                        if (len(line) >= 3):
                            #  Remove first two words
                            message = ' '.join(line[2:])
                            if len(message)>256:
                                print("Please enter a message withing 256 characters/letters \n Word Count:{}\n".format(len(message)))
                            else:
                                client.send(client.username, line[1], message)
                        else:
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif (line[0] == "SENDATTACH"):
                        if (len(line) >= 4):
                            #  Remove first three words
                            message = ' '.join(line[3:])
                            client.sendAttach(client.username, line[1], message)
                        else:
                            print("Syntax error. Usage: SENDATTACH <userName> <filename> <message>")

                    elif (line[0] == "QUIT"):
                        if (len(line) == 1):
                            break
                        else:
                            print("Syntax error. Use: QUIT")
                    else:
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage():
        print("Usage: python3 client.py -s <server> -p <port>")

    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def parseArguments(argv):
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535")
            return False

        client._server = args.s
        client._port = args.p



        return True

    # ******************** MAIN *********************
    @staticmethod
    def main(argv):
        if (not client.parseArguments(argv)):
            client.usage()
            return

        #  Write code here
        client.shell()
        print("+++ FINISHED +++")


if __name__ == "__main__":
    client.main([])