/*---------------------------------INCLUDES---------------------------------*/

//Standard C libraries.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

//Error handling.
#include <errno.h>
extern int errno;

//System types(Linux).
#include <sys/types.h>

//Communication libraries(BSD Socket).
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

//MQTT
#include <mosquitto.h>

/*----------------------------------DEFINES---------------------------------*/

//Multicast group.
#define MULTICAST_OUT_PORT         4444
#define MULTICAST_IN_PORT          5555
#define MULTICAST_IP        "225.1.1.1"

//Constants.
#define BUFF_LEN                    256

/*----------------------------------STRUCTS---------------------------------*/

//String variable(very helpful).
typedef struct
{
  char string[BUFF_LEN];
} String;

/*----------------------------------THREADS---------------------------------*/
/*---------------------------------FUNCTIONS--------------------------------*/
/*-----------------------------------MAIN-----------------------------------*/

void
main(int argc, char** argv)
{
  //Argument check.
  if(argc != 3)
  {
    printf("ERORR(main): Incorrect input - ./sen <id> <topic>");
    exit(EXIT_FAILURE);
  }

  //Socket for receiving.
  unsigned int multicast_in_socket;
  multicast_in_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if(multicast_in_socket < 0)
  {
    printf("ERROR(main): multicast_in_socket - socket failed. Error code: %d - %s\n", multicast_in_socket, strerror(errno));
    exit(EXIT_FAILURE);
  }

  //Make sure the socket is reusable.
  int val = 1;
  setsockopt(multicast_in_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  //Socket for sending.
  unsigned int multicast_out_socket;
  multicast_out_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if(multicast_out_socket < 0)
  {
    printf("ERROR(main): multicast_out_socket - socket failed. Error code: %d - %s\n", multicast_out_socket, strerror(errno));
    close(multicast_in_socket);
    exit(EXIT_FAILURE);
  }

  //Set up receiving IP properties and bind.
  struct sockaddr_in recv_addr;
  recv_addr.sin_family      = AF_INET;
  recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  recv_addr.sin_port        = htons(MULTICAST_IN_PORT);
  int ret = bind(multicast_in_socket, (struct sockaddr*)&recv_addr, sizeof(struct sockaddr));
  if(ret < 0)
  {
    printf("ERROR(main): recv_addr - bind failed. Error code: %d - %s\n", ret, strerror(errno));
    close(multicast_in_socket);
    close(multicast_out_socket);
    exit(EXIT_FAILURE);
  }

  //Multicast group setup.
  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  ret = setsockopt(multicast_in_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
  if(ret < 0)
  {
    printf("ERROR(main): mreq - setsockopt failed. Error code: %d - %s\n", ret, strerror(errno));
    exit(EXIT_FAILURE);
  }

  //Set up sending IP properties and TTL.
  struct sockaddr_in  send_addr;
  unsigned char       TTL_setup = 1;
  send_addr.sin_family      = AF_INET;
  send_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
  send_addr.sin_port        = htons(MULTICAST_OUT_PORT);
  ret = setsockopt(multicast_out_socket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&TTL_setup, sizeof(TTL_setup));
  if(ret < 0)
  {
    printf("ERROR(main): send_addr - setsockopt failed. Eror code: %d - %s\n", ret, strerror(errno));
    close(multicast_in_socket);
    close(multicast_out_socket);
    exit(EXIT_FAILURE);
  }

  //Receive MQTT broker IP.
  char          recv_buffer[BUFF_LEN];
  unsigned int  recv_addr_len = sizeof(recv_addr);
  ret = recvfrom(multicast_in_socket, recv_buffer, BUFF_LEN, 0, (struct sockaddr*)&recv_addr, &recv_addr_len);
  if(ret < 0)
  {
    printf("ERROR(main): receive - recvfrom failed. Error code: %d - %s\n", ret, strerror(errno));
    close(multicast_in_socket);
    close(multicast_out_socket);
    exit(EXIT_FAILURE);
  }
  printf("MQTT Broker IP: %s\n", recv_buffer);
  close(multicast_in_socket);

  //Load arguments.
  char* ID    = argv[1];
  char* desc  = argv[2];
  char  multicast_message[BUFF_LEN];
  sprintf(multicast_message, "sensor %s %s", ID, desc);

  //Set up MQTT.
  struct mosquitto* publisher;
  char              mqtt_message[BUFF_LEN];
  mosquitto_lib_init();

  //Attempt connection to broker.
  publisher = mosquitto_new(ID, true, NULL);
  ret = mosquitto_connect(publisher, recv_buffer, 1883, 60);
  if(ret != 0)
  {
    printf("ERROR(main): mqtt_connect - mosquitto_connect failed. Error code: %d - %s\n", ret, mosquitto_strerror(errno));
    close(multicast_in_socket);
    close(multicast_out_socket);
    mosquitto_destroy(publisher);
    mosquitto_lib_cleanup();
    exit(EXIT_FAILURE);
  }

  //Process loop.
  while(1)
  {
    //Send message to multicast group.
    sendto(multicast_out_socket, multicast_message, strlen(multicast_message)+1, 0, (struct sockaddr*)&send_addr, sizeof(send_addr));
    
    //Send message to MQTT broker.
    sprintf(mqtt_message, "S%s %d", ID, rand()%100+1);
    mosquitto_publish(publisher, NULL, desc, strlen(mqtt_message), mqtt_message, 0, false);
    
    //Wait for 0.2 seconds.
    usleep(200000);
  }

  //Clean up.
  close(multicast_in_socket);
  close(multicast_out_socket);
  mosquitto_disconnect(publisher);
  mosquitto_destroy(publisher);
  mosquitto_lib_cleanup();
}
