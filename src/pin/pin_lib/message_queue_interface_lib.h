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

#ifndef __MESSAGE_QUEUE_INTERFACE_LIB_H__
#define __MESSAGE_QUEUE_INTERFACE_LIB_H__

extern "C" {
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
}

#include <algorithm>
#include <queue>
#include <stdint.h>
#include <string>
#include <unistd.h>
#include <vector>
#include "pin_scarab_common_lib.h"

#define MAX_PACKET_SIZE (1 << 12)

void assertm(bool p, const char* msg);

namespace std {

// TODO: couldnt get std::move to work, so doing this for now to remove compiler
// error
template <typename T>
T move(const T& rhs) {
  return rhs;
}

};  // namespace std

/********************************************************************************************
 * Message Functions
 *******************************************************************************************/
class MessageBase {
 protected:
  std::vector<char> data;
  uint32_t          data_size;

  void init();

 public:
  MessageBase();
  MessageBase(const std::vector<char>& obj);
  // MessageBase(std::vector<char> && obj);
  ~MessageBase();
  size_t                   size() const;
  const std::vector<char>* get_raw_data() const;
};

template <typename T>
class Message : public MessageBase {
 private:
  void copy(const T& obj);

 public:
  Message() : MessageBase() {}
  Message(const std::vector<char>& obj) : MessageBase(obj) {}

  Message(const T& obj);
  Message& operator=(const T& obj);
           operator T() const;
};

// Special case to send vectors as messages
template <typename T>
class Message<std::vector<T>> : public MessageBase {
 private:
  void copy(const std::vector<T>& obj);

 public:
  Message() : MessageBase() {}
  Message(const std::vector<char>& obj) : MessageBase(obj) {}

  Message(const std::vector<T>& obj);
  Message& operator=(const std::vector<T>& obj);
           operator std::vector<T>() const;
};

// Special case to send deques as messages
template <typename T>
class Message<std::deque<T>> : public MessageBase {
 private:
  void copy(const std::deque<T>& obj);

 public:
  Message() : MessageBase() {}
  Message(const std::vector<char>& obj) : MessageBase(obj) {}

  Message(const std::deque<T>& obj);
  Message& operator=(const std::deque<T>& obj);
           operator std::deque<T>() const;
};

/******************************************************************************/

template <typename T>
Message<T>::Message(const T& object) : MessageBase() {
  copy(object);
}

template <typename T>
Message<T>& Message<T>::operator=(const T& object) {
  copy(object);
  return *this;
}

template <typename T>
Message<T>::operator T() const {
  T     object;
  char* object_ptr = (char*)&object;

  uint32_t t_size = sizeof(T);
  assertm(t_size == data_size,
          "Recieve type is not the same size as the send type");

  std::copy(data.begin(), data.end(), object_ptr);
  return object;
}

template <typename T>
void Message<T>::copy(const T& object) {
  char* object_ptr = (char*)&object;
  data_size        = sizeof(T);
  assertm(data_size <= MAX_PACKET_SIZE, "Scarab does not currently support "
                                        "sending messages larger than "
                                        "MAX_PACKET_SIZE.\n");

  std::vector<char> temp(object_ptr, object_ptr + data_size);
  data = temp;
}

/******************************************************************************/

template <typename T>
Message<std::vector<T>>::Message(const std::vector<T>& obj) : MessageBase() {
  copy(obj);
}

template <typename T>
Message<std::vector<T>>& Message<std::vector<T>>::operator=(
  const std::vector<T>& obj) {
  copy(obj);
  return *this;
}

template <typename T>
Message<std::vector<T>>::operator std::vector<T>() const {
  uint32_t       size_of_vector = data_size / sizeof(T);
  std::vector<T> object(size_of_vector);

  for(uint32_t i = 0; i < size_of_vector; ++i) {
    uint32_t start_idx = i * sizeof(T);
    uint32_t stop_idx  = (i + 1) * sizeof(T);
    std::copy(&data[start_idx], &data[stop_idx], (char*)&object[i]);
  }
  return object;
}

template <typename T>
void Message<std::vector<T>>::copy(const std::vector<T>& object) {
  data_size = object.size() * sizeof(T);
  assertm(data_size <= MAX_PACKET_SIZE, "Scarab does not currently support "
                                        "sending messages larger than "
                                        "MAX_PACKET_SIZE.\n");

  for(uint32_t i = 0; i < object.size(); ++i) {
    char* object_ptr = (char*)&object[i];
    data.insert(data.end(), object_ptr, object_ptr + sizeof(T));
  }
}


/******************************************************************************/

template <typename T>
Message<std::deque<T>>::Message(const std::deque<T>& obj) : MessageBase() {
  copy(obj);
}

template <typename T>
Message<std::deque<T>>& Message<std::deque<T>>::operator=(
  const std::deque<T>& obj) {
  copy(obj);
  return *this;
}

template <typename T>
Message<std::deque<T>>::operator std::deque<T>() const {
  uint32_t      size_of_deque = data_size / sizeof(T);
  std::deque<T> object(size_of_deque);

  for(uint32_t i = 0; i < size_of_deque; ++i) {
    uint32_t start_idx = i * sizeof(T);
    uint32_t stop_idx  = (i + 1) * sizeof(T);
    std::copy(&data[start_idx], &data[stop_idx], (char*)&object[i]);
  }
  return object;
}

template <typename T>
void Message<std::deque<T>>::copy(const std::deque<T>& object) {
  data_size = object.size() * sizeof(T);
  assertm(data_size <= MAX_PACKET_SIZE, "Scarab does not currently support "
                                        "sending messages larger than "
                                        "MAX_PACKET_SIZE.\n");

  for(uint32_t i = 0; i < object.size(); ++i) {
    char* object_ptr = (char*)&object[i];
    data.insert(data.end(), object_ptr, object_ptr + sizeof(T));
  }
}

/******************************************************************************/

/********************************************************************************************
 * TCP Functions
 *******************************************************************************************/
class TCPSocket {
 protected:
  typedef int32_t SocketDescriptor;

  bool               is_server;
  SocketDescriptor   socket_fd;
  struct sockaddr_un socket_address;
  int32_t            socket_address_length;
  std::string        socket_path;  // TODO: initialize this
  std::deque<char>   receive_buffer;

  std::string server_init_message;
  std::string client_init_message;

  void send(SocketDescriptor socket, const std::vector<char>* msg);

#if !defined(PIN_COMPILE) || defined(GTEST_COMPILE)
  std::vector<char> scarab_receive(SocketDescriptor socket);
#endif

#if defined(PIN_COMPILE) || defined(GTEST_COMPILE)
  std::vector<char> pin_receive(SocketDescriptor socket,
                                uint32_t         num_bytes_recv);
#endif

  void verify_socket_read(SocketDescriptor new_socket, std::string msg);
  void verify_socket_write(SocketDescriptor new_socket, std::string msg);
  void create_socket_file_descriptor();
  void setup_unix_sockaddr_struct();
  void bind_socket_to_file();
  int  setNonblocking();
  void disconnect(SocketDescriptor socket);

 public:
  TCPSocket();
  ~TCPSocket();
  template <typename T>
  void send(SocketDescriptor socket, const Message<T>& m);
  template <typename T>
  Message<T> receive(SocketDescriptor socket);
};

class Server : public TCPSocket {
 private:
  typedef int32_t OptionType;

  std::vector<SocketDescriptor> client_fds;
  std::vector<uint32_t>         requested_client_ids;
  OptionType                    option;

  void listen_and_connect_clients();
  void listen_for_clients();
  void accept_new_clients();
  void verify_client_connection(SocketDescriptor socket);
  void get_requested_client_id(uint32_t current_client_id);
  void verify_and_assign_requested_client_ids(uint32_t numClients);

 public:
  Server(uint32_t numClients);
  Server(const std::string& _socket_path, uint32_t numClients);
  ~Server();
  void init(uint32_t numClients);
  template <typename T>
  void send(uint32_t id, const Message<T>& m);
  template <typename T>
  Message<T> receive(uint32_t id);
  void       disconnect(uint32_t client_id);
  uint32_t   getNumClients() const { return client_fds.size(); }
  void       wait_for_client_to_close(uint32_t client_id);
};

class Client : public TCPSocket {
 private:
  uint32_t client_id;

  void connect_to_server();
  void verify_server_connection();
  void send_requested_client_id(uint32_t requested_client_id);

 public:
  Client();
  Client(const std::string& _socket_path);
  Client(const std::string& _socket_path, uint32_t requested_client_id);
  ~Client();
  void init(uint32_t requested_client_id);
  template <typename T>
  void send(const Message<T>& m);
  template <typename T>
  Message<T> receive();
  void       disconnect();

#ifdef GTEST_COMPILE
  template <typename T>
  Message<T> pin_receive();
#endif
};

template <typename T>
void Server::send(uint32_t id, const Message<T>& m) {
  TCPSocket::send(client_fds[id], m);
}

template <typename T>
Message<T> Server::receive(uint32_t id) {
  return TCPSocket::receive<T>(client_fds[id]);
}

template <typename T>
void Client::send(const Message<T>& m) {
  TCPSocket::send(socket_fd, m);
}

template <typename T>
Message<T> Client::receive() {
  return TCPSocket::receive<T>(socket_fd);
}

#ifdef GTEST_COMPILE
template <typename T>
Message<T> Client::pin_receive() {
  return Message<T>(TCPSocket::pin_receive(socket_fd, sizeof(T)));
}
#endif

template <typename T>
void TCPSocket::send(SocketDescriptor socket, const Message<T>& m) {
  send(socket, m.get_raw_data());
}

template <typename T>
Message<T> TCPSocket::receive(SocketDescriptor socket) {
#ifndef PIN_COMPILE
  return Message<T>(scarab_receive(socket));
#else
  return Message<T>(pin_receive(socket, sizeof(T)));
#endif
}
#endif
