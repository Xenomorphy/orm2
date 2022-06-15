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

/*----------------------------------DEFINES---------------------------------*/

//Multicast group.
#define MULTICAST_IN_PORT          4444
#define MULTICAST_OUT_PORT         5555
#define MULTICAST_IP        "225.1.1.1"

//MQTT Broker IP.
#define BROKER_IP        "192.168.0.13"

//Constants.
#define BUFF_LEN                    256
#define MAX_DEVICES                  64
#define TTL_C                         2

/*----------------------------------STRUCTS---------------------------------*/

//String variable(very helpful).
typedef struct
{
  char string[BUFF_LEN];
} String;

//Sensors/actuators.
typedef struct
{
  int     id;
  int     TTL;
  int     active_for;
  String  message;
} Device;

//Arguments for incoming messages thread.
typedef struct
{
  unsigned int        socket;
  String*             buffer;
  struct sockaddr_in  addr;
} recv_thread_args;

/*----------------------------------THREADS---------------------------------*/

void* 
recvThread(void* vargs)
{
  recv_thread_args* args      = (recv_thread_args*)vargs; //Get passed arguments.
  unsigned int      addr_len  = sizeof(args->addr);       //Get address length.
  String            buffer;                               //Buffer for receiving.

  //Receive loop.
  while(1)
  {
    //Try to receive message.
    int ret = recvfrom(args->socket, buffer.string, BUFF_LEN, 0, (struct sockaddr*)&args->addr, &addr_len);
    if(ret < 0)
    {
      printf("ERROR(recvThread): recvfrom failed. Error code: %d - %s\n", ret, strerror(errno));
    }

    //Add device to list if it isn't already there.
    for(int i = 0; i < MAX_DEVICES; i++)
    {
      if(args->buffer[i].string[0] == 'C')
      {
        sprintf(args->buffer[i].string, "%s", buffer.string);
        break;
      }
      if(!strcmp(buffer.string, args->buffer[i].string))
      {
        break;
      }
    }
  }
}

/*---------------------------------FUNCTIONS--------------------------------*/

void
multicast()
{
  //Socket for sending.
  unsigned int multicast_out_socket;
  multicast_out_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if(multicast_out_socket < 0)
  {
    printf("ERROR(main): multicast_out_socket - socket failed. Error code: %d - %s\n", multicast_out_socket, strerror(errno));
    exit(EXIT_FAILURE);
  }

  //Set up sending IP properties.
  struct sockaddr_in send_addr;
  unsigned char      TTL_setup = 1;
  send_addr.sin_family      = AF_INET;
  send_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
  send_addr.sin_port        = htons(MULTICAST_OUT_PORT);
  int ret = setsockopt(multicast_out_socket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&TTL_setup, sizeof(TTL_setup));
  if(ret < 0)
  {
    printf("ERROR(main): send_addr - setsockopt failed. Eror code: %d - %s\n", ret, strerror(errno));
    exit(EXIT_FAILURE);
  }

  char to_send[BUFF_LEN];
  sprintf(to_send, BROKER_IP);
  ret = sendto(multicast_out_socket, to_send, strlen(to_send)+1, 0, (struct sockaddr*)&send_addr, sizeof(send_addr));
  if(ret < 0)
  {
    printf("ERROR(main): request - sendto failed. Error code: %d - %s\n", ret, strerror(errno));
    exit(EXIT_FAILURE);
  }

  close(multicast_out_socket);
}

void
processMessage(String received_message, Device sensors[], Device actuators[])
{
  //Get first 3 strings separated by " ".
  char* first = strtok(received_message.string, " ");
  char* second = strtok(NULL, " ");
  char* third = strtok(NULL, " ");

  printf("%s %s %s\n", first, second, third);

  //Check if it is a sensor or an actuator.
  Device* devices;

  if(!strcmp(first, "sensor"))
  {
    devices = sensors;
  }
  else if(!strcmp(first, "actuator"))
  {
    devices = actuators;
  }
  else
  {
    printf("Uknown type of message received: %s", received_message.string);
    return;
  }

  //Process
  for(int i = 0; i < MAX_DEVICES; i++)
  {
    if(devices[i].id == -1)
    {
      devices[i].id = atoi(second);
      devices[i].TTL = TTL_C;
      sprintf(devices[i].message.string, "%s (%s) - ACTIVE FOR %d", second, third, devices[i].active_for);
      break;
    }
    if(devices[i].id == atoi(second))
    {
      devices[i].active_for += 1;
      devices[i].TTL = TTL_C;
      sprintf(devices[i].message.string, "%s (%s) - ACTIVE FOR %d", second, third, devices[i].active_for);
      break;
    }
  }
}

void
reduceTTL(Device sensors[], Device actuators[])
{
  //Reduce each by 1.
  for(int i = 0; i < MAX_DEVICES; i++)
  {
    sensors[i].TTL -= 1;
    actuators[i].TTL -= 1;
  }
}

void
writeOutput(Device sensors[], Device actuators[])
{
  //Clear terminal.
  printf("\x1B[2J");
  printf("\x1B[H");

  //Header
  printf("|--------------------------------------|\n|--            --DEVICES--           --|\n|--------------------------------------|\n");

  //Sensors
  printf("  -SENSORS:\n");
  for(int i = 0; i < MAX_DEVICES; i++)
  {
    if(sensors[i].id == -1)
    {
      break;
    }
    if(sensors[i].TTL > 0)
    {
      printf("\t->%s\n", sensors[i].message.string);
    }
    else
    {
      printf("\t->%d - DISCONNECTED\n", sensors[i].id);
      sensors[i].active_for = 0;
    }
  }

  //Actuators
  printf("  -ACTUATORS:\n");
  for(int i = 0; i < MAX_DEVICES; i++)
  {
    if(actuators[i].id == -1)
    {
      break;
    }
    if(sensors[i].TTL > 0)
    {
      printf("\t->%s\n", actuators[i].message.string);
    }
    else
    {
      printf("\t->%d - DISCONNECTED\n", actuators[i].id);
      actuators[i].active_for = 0;
    }
  }
}

/*-----------------------------------MAIN-----------------------------------*/

void
main(void)
{
  //Socket for receiving.
  unsigned int multicast_in_socket;
  multicast_in_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if(multicast_in_socket < 0)
  {
    printf("ERROR(main): multicast_in_socket - socket failed. Error code: %d - %s\n", multicast_in_socket, strerror(errno));
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

  //Set up devices.
  Device sensors[MAX_DEVICES];
  Device actuators[MAX_DEVICES];
  for(int i = 0; i < MAX_DEVICES; i++)
  {
    sensors[i].id                 = -1;
    sensors[i].TTL                = TTL_C;
    sensors[i].active_for         = 0;
    sensors[i].message.string[0]  = '*';

    actuators[i].id                 = -1;
    actuators[i].TTL                = TTL_C;
    actuators[i].active_for         = 0;
    actuators[i].message.string[0]  = '*';
  }

  //Set up thread for receiving and start it.
  pthread_t         thread;
  recv_thread_args  args;
  String            buffer_temp[MAX_DEVICES];
  for(int i = 0; i < MAX_DEVICES; i++)
  {
    sprintf(buffer_temp[i].string, "C");
  }
  args.addr = recv_addr;
  args.socket = multicast_in_socket;
  args.buffer = buffer_temp;
  pthread_create(&thread, NULL, recvThread, (void*)&args);

  //Process loop
  int i;
  while(1)
  {
    //Multicast MQTT broker ip.
    multicast();

    //Process all received messages.
    i = 0;
    while(args.buffer[i].string[0] != 'C')
    {
      processMessage(args.buffer[i], sensors, actuators);
      sprintf(args.buffer[i].string, "C");
    }

    //Reduce time to live for each connection.
    reduceTTL(sensors, actuators);

    //Write to terminal.
    writeOutput(sensors, actuators);

    //Wait 1 second.
    sleep(1);
  }

  //Clean up.
  close(multicast_in_socket);
}
