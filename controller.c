//EDITED BY G1 - GRUPA 33
//
//
//===================================================== file = mclient.c =====
//=  A multicast client to receive multicast datagrams                       =
//============================================================================
//=  Notes:                                                                  =
//=    1) This program receives on a multicast address and outputs the       =
//=       received buffer to the screen.                                     =
//=    2) Conditionally compiles for Winsock and BSD sockets by setting the  =
//=       initial #define to WIN or BSD as appropriate.                      =
//=    3) The multicast group address is GROUP_ADDR.                         =
//=--------------------------------------------------------------------------=
//=  Build: bcc32 mclient.c or cl mclient.c wsock32.lib for Winsock          =
//=         gcc mclient.c -lsocket -lnsl for BSD                             =
//=--------------------------------------------------------------------------=
//=  History:  JNS (07/11/02) - Genesis                                      =
//============================================================================
#define BSD                // WIN for Winsock and BSD for BSD sockets

//----- Include files -------------------------------------------------------
#include <stdio.h>          // Needed for printf()
#include <stdlib.h>         // Needed for memcpy()
#include <string.h>
#include <pthread.h>
#ifdef WIN
  #include <winsock.h>      // Needed for all Windows stuff
#endif
#ifdef BSD
  #include <sys/types.h>    // Needed for system defined identifiers.
  #include <netinet/in.h>   // Needed for internet address structure.
  #include <sys/socket.h>   // Needed for socket(), bind(), etc...
  #include <arpa/inet.h>    // Needed for inet_ntoa()
  #include <fcntl.h>
  #include <netdb.h>
#endif

//----- Defines -------------------------------------------------------------
#define PORT_NUM         4444             // Port number used
#define GROUP_ADDR "225.1.1.1"            // Address of the multicast group

#define CLRSCR  printf("\x1B[2J"); \
                printf("\x1B[H")

#define HEADER "|--------------------------------------|\n|--            --DEVICES--           --|\n|--------------------------------------|\n"

#define TTL_C 2

#define BUFF_LEN 256
#define MAX_DEVICES 64

//===== Structs =============================================================
typedef struct
{
  char string[BUFF_LEN];
} String;

typedef struct
{
  int id;
  int active_for;
  int TTL;
} Device;

//===== Functions ===========================================================
void processDevice(char buffer[], Device sensors[], Device actuators[])
{
  //separate ID and type
  char* ID = strsep(&buffer, " ");
  char* type = strsep(&buffer, " ");

  if(ID == NULL || type == NULL)
  {
    return;
  }

  //check if correct type
  Device* devices;

  switch(type[0])
  {
    case 's':
    {
      devices = sensors;
    }
    break;
    case 'a':
    {
      devices = actuators;
    }
    break;
    default: return;
  }

  //assign values to correct device
  for(int i = 0; i < MAX_DEVICES; i++)
  {
    if(devices[i].id == -1)
    {
      devices[i].id = atoi(ID);
      devices[i].TTL = TTL_C;
      break;
    }
    if(devices[i].id == atoi(ID))
    {
      devices[i].active_for += 1;
      devices[i].TTL = TTL_C;
      break;
    }
  }
}

void reduceTTL(Device sensors[], Device actuators[])
{
  //reduce each TTL by 1
  for(int i = 0; i < MAX_DEVICES; i++)
  {
    sensors[i].TTL -= 1;
    actuators[i].TTL -= 1;
  }
}

void writeOutputBuffer(String messages[], Device sensors[], Device actuators[])
{
  //make correct output messages for sensors
  int i = 0;
  while(sensors[i].id != -1 && i < MAX_DEVICES)
  {
    if(sensors[i].TTL > 0)
    {
      sprintf(messages[i].string, "SENSOR %d - ACTIVE FOR %ds", sensors[i].id, sensors[i].active_for);
    }
    else
    {
      sprintf(messages[i].string, "SENSOR %d - DISCONNECTED", sensors[i].id);
      sensors[i].active_for = 0;
    }
    i++;
  }

  //make correct output messages for actuators
  int j = 0;
  while(actuators[j].id != -1 && j < MAX_DEVICES)
  {
    if(actuators[j].TTL > 0)
    {
      sprintf(messages[i].string, "ACTUATOR %d - ACTIVE FOR %ds", actuators[j].id, actuators[j].active_for);
    }
    else
    {
      sprintf(messages[i].string, "ACTUATOR %d - DISCONNECTED", actuators[j].id);
      actuators[j].active_for = 0;
    }
    i++;
    j++;
  }
}

void CLI(String messages[])
{
  //clear terminal
  CLRSCR;

  //print outputs
  printf(HEADER);
  printf("  -SENSORS:\n");
  int i = 0;
  while(messages[i].string[0] == 'S')
  {
    printf("\t->%s\n", messages[i].string);
    i++;
  }
  printf("  -ACTUATORS:\n");
  while(messages[i].string[0] == 'A')
  {
    printf("\t->%s\n", messages[i].string);
    i++;
  }
}

//===== Threads =============================================================

typedef struct
{
  unsigned int        socket;
  String*             buffer;
  struct sockaddr_in  addr;
} t_args;

void* recvThread(void* varg)
{
    t_args*       args = (t_args*)varg;
    unsigned int  addr_len = sizeof(args->addr);
    char          buffer[BUFF_LEN];

    while(1)
    {
      // Receive a datagram from the multicast server
      if(recvfrom(args->socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&args->addr, &addr_len) < 0)
      {
        printf("*** ERROR - recvfrom() failed \n");
        exit(1);
      }

      for(int i = 0; i < MAX_DEVICES; i++)
      {
        if(args->buffer[i].string[0] == 'C')
        {
          sprintf(args->buffer[i].string, "%s", buffer);
          break;
        }
        if(!strcmp(buffer, args->buffer[i].string))
        {
          break;
        }
      }
    }
}

//===== Main program ========================================================
void main(void)
{
#ifdef WIN
  WORD wVersionRequested = MAKEWORD(1,1); // Stuff for WSA functions
  WSADATA wsaData;                        // Stuff for WSA functions
#endif
  unsigned int         multi_server_sock; // Multicast socket descriptor
  struct ip_mreq       mreq;              // Multicast group structure
  struct sockaddr_in   client_addr;       // Client Internet address
  int                  retcode;           // Return code

#ifdef WIN
  // This stuff initializes winsock
  WSAStartup(wVersionRequested, &wsaData);
#endif

  // Create a multicast socket and fill-in multicast address information
  multi_server_sock=socket(AF_INET, SOCK_DGRAM, 0);
  mreq.imr_multiaddr.s_addr = inet_addr(GROUP_ADDR);
  mreq.imr_interface.s_addr = INADDR_ANY;

  // Create client address information and bind the multicast socket
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = INADDR_ANY;
  client_addr.sin_port = htons(PORT_NUM);
  retcode = bind(multi_server_sock, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
  if (retcode < 0)
  {
    printf("*** ERROR - bind() failed with retcode = %d \n", retcode);
    return;
  }

  // Have the multicast socket join the multicast group
  retcode = setsockopt(multi_server_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) ;
  if (retcode < 0)
  {
    printf("*** ERROR - setsockopt() failed with retcode = %d \n", retcode);
    return;
  }

  // Threads for receiving
  pthread_t rt;
  t_args    args;
  String inputs[MAX_DEVICES];

  for(int i = 0; i < MAX_DEVICES; i++)
  {
    sprintf(inputs[i].string, "C");
  }

  args.socket = multi_server_sock;
  args.buffer = inputs;
  args.addr   = client_addr;

  pthread_create(&rt, NULL, recvThread, (void*)&args);

  // Variables for processing
  Device sensors[MAX_DEVICES];
  Device actuators[MAX_DEVICES];

  for(int i = 0; i < MAX_DEVICES; i++)
  {
    sensors[i].id = -1;
    sensors[i].active_for = 0;
    sensors[i].TTL = TTL_C;

    actuators[i].id = -1;
    actuators[i].active_for = 0;
    actuators[i].TTL = TTL_C;
  }

  String messages[MAX_DEVICES];

  int i;

  while(1)
  {
    // Process incoming device
    i = 0;
    while(args.buffer[i].string[0] != 'C')
    {
      processDevice(args.buffer[i].string, sensors, actuators);
      sprintf(args.buffer[i].string, "C");
      i++;
    }

    // Reduce TTL for each device connection
    reduceTTL(sensors, actuators);

    // Fill output messages buffer
    writeOutputBuffer(messages, sensors, actuators);

    // Write to terminal
    CLI(messages);

    sleep(1);
  }

  // Close and clean-up
#ifdef WIN
  closesocket(multi_server_sock);
  WSACleanup();
#endif
#ifdef BSD
  close(multi_server_sock);
#endif
}
