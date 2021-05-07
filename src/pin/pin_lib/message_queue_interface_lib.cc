/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include "message_queue_interface_lib.h"

extern "C" {}

#define RECEIVE_BUFFER_MAX_SIZE (0x01 << 12)
#define CHECK_FOR_FAILURE(f, str)                                       \
  if(f) {                                                               \
    char error_message[1024];                                           \
    snprintf(error_message, 1024, "%s:%d (%s)$ %s", __FILE__, __LINE__, \
             is_server ? "Server" : "Client", str);                     \
    perror(error_message);                                              \
    exit(1);                                                            \
  }
//#define MQ_NON_BLOCKING 1

void assertm(bool p, const char* msg) {
  if(!p) {
    printf("Message Queue Assertion Fired: %s\n", msg);
    exit(1);
  }
}

/********************************************************************************************
 * MessageBase Functions
 *******************************************************************************************/

MessageBase::MessageBase() {
  init();
}


void MessageBase::init() {
  data_size = 0;
}

MessageBase::MessageBase(const std::vector<char>& obj) {
  init();
  data      = obj;
  data_size = data.size();
  assertm(data_size <= MAX_PACKET_SIZE, "Scarab does not currently support "
                                        "sending messages larger than "
                                        "MAX_PACKET_SIZE.\n");
}

// MessageBase::MessageBase (std::vector<char>&& obj)
//{
//  init();
//  data = std::move(obj);
//  data_size = data.size();
//  assertm(data_size <= MAX_PACKET_SIZE, "Scarab does not currently support
//  sending messages larger than MAX_PACKET_SIZE.\n");
//}

MessageBase::~MessageBase() {}

size_t MessageBase::size() const {
  return data_size;
}

const std::vector<char>* MessageBase::get_raw_data() const {
  return &data;
}

/********************************************************************************************
 * TCPSocket Functions
 *******************************************************************************************/

TCPSocket::TCPSocket() {
  server_init_message   = "Server Init Message";
  client_init_message   = "Client Init Message";
  socket_address_length = sizeof(socket_address);
  socket_path           = "/tmp/Scarab_Pin_Socket.tmp";
}

TCPSocket::~TCPSocket() {
  close(socket_fd);
}

void TCPSocket::send(SocketDescriptor         socket,
                     const std::vector<char>* message) {
  uint32_t total_bytes_sent = 0;
  int32_t  failure;
  char*    buffer = (char*)&(
    *message)[0]; /*CPP guuarantees vector stored in contiguous memory*/
  assertm(message->size() <= RECEIVE_BUFFER_MAX_SIZE,
          "Need to allocation more space in the send buffer");

  do {
    do {
      failure = ::send(socket, buffer, (int32_t)message->size(), 0);
    } while(failure < 0 && (errno == EWOULDBLOCK || errno == EAGAIN));

    CHECK_FOR_FAILURE(failure < 0, "Send Failed");
    total_bytes_sent += failure;
  } while(total_bytes_sent < message->size());

  CHECK_FOR_FAILURE(total_bytes_sent != message->size(),
                    "TCPSocket Send did not send the correct number of bytes!")
}

#if !defined(PIN_COMPILE) || defined(GTEST_COMPILE)

std::vector<char> TCPSocket::scarab_receive(SocketDescriptor socket) {
  char    buffer[RECEIVE_BUFFER_MAX_SIZE];
  int32_t bytes_recv;

  do {
    bytes_recv = recv(socket, buffer, sizeof(buffer), 0);
    CHECK_FOR_FAILURE(
      bytes_recv == 0,
      "Socket closed unepectedly on read. PIN process probably died.");
    if(bytes_recv < 0)
      perror("ERRNO: ");
  } while(bytes_recv < 0 && (errno == EWOULDBLOCK || errno == EAGAIN));

  CHECK_FOR_FAILURE(bytes_recv < 0, "Receive Failed");

  std::vector<char> message(buffer, buffer + bytes_recv);
  return message;
}

#endif

#if defined(PIN_COMPILE) || defined(GTEST_COMPILE)

std::vector<char> TCPSocket::pin_receive(SocketDescriptor socket,
                                         uint32_t         num_bytes_recv) {
  char    buffer[RECEIVE_BUFFER_MAX_SIZE];
  int32_t bytes_recv = 0;

  while(receive_buffer.size() < num_bytes_recv) {
    do {
      bytes_recv = read(socket, buffer, RECEIVE_BUFFER_MAX_SIZE);
      CHECK_FOR_FAILURE(
        bytes_recv < 0 && (errno != EWOULDBLOCK && errno != EAGAIN),
        "Receive Failed (pin_receive)");
      CHECK_FOR_FAILURE(
        bytes_recv == 0,
        "Socket closed unexpectedly on read. Scarab process probably died.");
    } while(bytes_recv < 0 && (errno == EWOULDBLOCK || errno == EAGAIN));

    receive_buffer.insert(receive_buffer.end(), buffer, buffer + bytes_recv);
  }

  CHECK_FOR_FAILURE(bytes_recv < 0, "Receive Failed (pin_receive)");

  std::vector<char> message(receive_buffer.begin(),
                            receive_buffer.begin() + num_bytes_recv);
  receive_buffer.erase(receive_buffer.begin(),
                       receive_buffer.begin() + num_bytes_recv);
  return message;
}

#endif

void TCPSocket::verify_socket_read(SocketDescriptor new_socket,
                                   std::string      expected_message) {
#ifndef PIN_COMPILE
  std::vector<char> received_message = TCPSocket::scarab_receive(new_socket);
#else
  std::vector<char> received_message = TCPSocket::pin_receive(
    new_socket, expected_message.size() + 1);
#endif

  assertm(received_message.size() == expected_message.size() + 1,
          "First Received Message length incorrect");
  for(uint32_t i = 0; i < received_message.size(); ++i) {
    assertm(received_message[i] == expected_message[i],
            "Character mismatch between received message and expected message");
  }
}

void TCPSocket::verify_socket_write(SocketDescriptor new_socket,
                                    std::string      expected_message) {
  std::vector<char> message;
  std::copy(expected_message.begin(), expected_message.end(),
            std::back_inserter(message));
  message.push_back('\0');  // null terminating string
  TCPSocket::send(new_socket, &message);
}

void TCPSocket::create_socket_file_descriptor() {
  /* AF_UNIX: communicate over a shared unix file system.
   * SOCK_STREAM: Treat communication as continuous stream of characters, use
   * TCP. 0 (the protocol field): allow the OS to choose the most appropirate
   * protocol. Should choose TCP.
   */
  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  CHECK_FOR_FAILURE(socket_fd == 0, "Socket Failed");
}

void TCPSocket::setup_unix_sockaddr_struct() {
  socket_address.sun_family = AF_UNIX;
  strcpy(socket_address.sun_path, socket_path.c_str());
}

void TCPSocket::bind_socket_to_file() {
  unlink(socket_path.c_str());
  int32_t failure = bind(socket_fd, (struct sockaddr*)&socket_address,
                         socket_address_length);
  CHECK_FOR_FAILURE(failure < 0, "Bind Failed");
}

int TCPSocket::setNonblocking() {
  int32_t flags;

  /* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
  /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
  if(-1 == (flags = fcntl(socket_fd, F_GETFL, 0)))
    flags = 0;
  return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
#else
  /* Otherwise, use the old way of doing it */
  flags = 1;
  return ioctl(socket_fd, FIOBIO, &flags);
#endif
}

void TCPSocket::disconnect(SocketDescriptor socket) {
  close(socket);
}

/********************************************************************************************
 * Server Functions
 *******************************************************************************************/
Server::Server(uint32_t numClients) {
  init(numClients);
}

Server::Server(const std::string& _socket_path, uint32_t numClients) {
  socket_path = _socket_path;
  init(numClients);
}

Server::~Server() {
  for(uint32_t i = 0; i < client_fds.size(); ++i) {
    close(client_fds[i]);
  }
  unlink(socket_path.c_str());
}

void Server::init(uint32_t numClients) {
  is_server = true;
  create_socket_file_descriptor();
  setup_unix_sockaddr_struct();
  bind_socket_to_file();

#ifdef MQ_NON_BLOCKING
  setNonblocking();
#endif

  printf("Listening for new clients\n");
  listen_for_clients();
  printf("accepting for new clients\n");
  for(uint32_t i = 0; i < numClients; ++i) {
    // listen_and_connect_clients();
    accept_new_clients();
  }
  verify_and_assign_requested_client_ids(numClients);
}

void Server::listen_and_connect_clients() {
  listen_for_clients();
  accept_new_clients();
}

void Server::listen_for_clients() {
  int BACKLOG = 5;
  int failure = listen(socket_fd, BACKLOG);
  CHECK_FOR_FAILURE(failure < 0, "Listen Failed");
}

void Server::accept_new_clients() {
  SocketDescriptor new_socket;

  do {
    new_socket = accept(socket_fd, (struct sockaddr*)&socket_address,
                        (socklen_t*)&socket_address_length);
    CHECK_FOR_FAILURE(
      new_socket < 0 && (errno != EWOULDBLOCK && errno != EAGAIN),
      "Accept Failed (1)");
  } while(new_socket < 0 && (errno == EWOULDBLOCK || errno == EAGAIN));

  // unlink(new_socket);
  CHECK_FOR_FAILURE(new_socket < 0, "Accept Failed (2)");
  client_fds.push_back(new_socket);

  verify_client_connection(new_socket);
  get_requested_client_id(client_fds.size() - 1);
}

void Server::verify_client_connection(SocketDescriptor socket) {
  verify_socket_read(socket, client_init_message);
  verify_socket_write(socket, server_init_message);
  printf("Server verified connection.\n");
  fflush(NULL);
}

void Server::get_requested_client_id(uint32_t current_client_id) {
  requested_client_ids.push_back(receive<uint32_t>(current_client_id));
}

void Server::wait_for_client_to_close(uint32_t client_id) {
#if !defined(PIN_COMPILE) || defined(GTEST_COMPILE)
  char     buffer[RECEIVE_BUFFER_MAX_SIZE];
  uint32_t bytes_recv = recv(client_fds[client_id], buffer, sizeof(buffer),
                             MSG_PEEK);
  CHECK_FOR_FAILURE(bytes_recv < 0,
                    "wait_for_client_to_close failed due to an error");
  CHECK_FOR_FAILURE(
    bytes_recv > 0,
    "wait_for_client_to_close found a message in the buffer after exit");
#endif
}

/*If client requested a specific core, that assignment will be made here. If
 * client requests max uint32 id, then it will keep its default id. No two
 * clients can have the same id, so either all clients must assign a specific
 * id, or all clients must use default id.
 */
void Server::verify_and_assign_requested_client_ids(uint32_t numClients) {
  uint32_t                      max_unsigned_int = -1;
  std::vector<SocketDescriptor> temp_client_fds(numClients, max_unsigned_int);
  assertm(requested_client_ids.size() == numClients,
          "Some clients did not request a client id");

  for(uint32_t i = 0; i < numClients; ++i) {
    uint32_t requested_id = requested_client_ids[i] == max_unsigned_int ?
                              i :
                              requested_client_ids[i];
    assertm(temp_client_fds[requested_id] == (SocketDescriptor)max_unsigned_int,
            "Two or more clients requested the same id");
    temp_client_fds[requested_id] = client_fds[i];
  }
  client_fds = temp_client_fds;
}

void Server::disconnect(uint32_t client_id) {
  assertm(client_id < client_fds.size(),
          "Attempting to disconnect from an invalid client_id!");
  TCPSocket::disconnect(client_fds[client_id]);
}

/********************************************************************************************
 * Client Functions
 *******************************************************************************************/
Client::Client() {
  init((uint32_t)-1);
}

Client::Client(const std::string& _socket_path) {
  socket_path = _socket_path;
  init((uint32_t)-1);
}

Client::Client(const std::string& _socket_path, uint32_t requested_client_id) {
  socket_path = _socket_path;
  init(requested_client_id);
}

Client::~Client() {}

void Client::init(uint32_t requested_client_id) {
  is_server = false;
  client_id = requested_client_id;
  create_socket_file_descriptor();
  setup_unix_sockaddr_struct();
  connect_to_server();
  verify_server_connection();
  send_requested_client_id(requested_client_id);
}

void Client::disconnect() {
  TCPSocket::disconnect(socket_fd);
}

void Client::connect_to_server() {
  constexpr int WAIT_PERIOD_IN_USECONDS = 100000;  // Retry after 100ms
  constexpr int NUM_TRIALS              = 100;     // Total trail time = 10s

  for(int i = 0; i < NUM_TRIALS; ++i) {
    if(connect(socket_fd, (struct sockaddr*)&socket_address,
               socket_address_length) == 0) {
      return;
    }

    printf("Connected to Server failed. Trying again after %d ms\n",
           WAIT_PERIOD_IN_USECONDS / 1000);
    usleep(WAIT_PERIOD_IN_USECONDS);
  }
  CHECK_FOR_FAILURE(true, "Connection to Server Failed");
}

void Client::verify_server_connection() {
  verify_socket_write(socket_fd, client_init_message);
  verify_socket_read(socket_fd, server_init_message);
  printf("Client verified connection.\n");
  fflush(NULL);
}

void Client::send_requested_client_id(uint32_t requested_client_id) {
  send((Message<uint32_t>)requested_client_id);
}
