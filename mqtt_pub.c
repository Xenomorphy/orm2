/*------------------------------INCLUDES--------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>

/*-------------------------------DEFINES--------------------------------------*/

#define ALL_OK       0
#define ERR_ARGS    -1
#define ERR_CONNECT -2

char* ip;
char* ID;
char* topic;

/*------------------------------FUNCTIONS-------------------------------------*/
/*--------------------------------MAIN----------------------------------------*/

int main(int argc, char** argv)
{
  int global_ret = ALL_OK;

  //check input arguments
  if(argc != 4)
  {
    printf("Wrong input.\nPlease use: ./pub <ip> <ID> <full-topic>");
    return ERR_ARGS;
  }

  //load input arguments
  ip    = argv[1];
  ID    = argv[2];
  topic = argv[3];
  //set up publisher variables
  int ret;
  struct mosquitto* pub;
  char* to_send = "Test message.";

  //start mqtt
  mosquitto_lib_init();

  //create new publisher instance
  pub = mosquitto_new(ID, true, NULL);

  //attempt connection to broker
  ret = mosquitto_connect(pub, ip, 1883, 60);
  if(ret != 0)
  {
    global_ret = ERR_CONNECT;
    printf("Could not connect to broker. Error: %d\n", ret);
    mosquitto_destroy(pub);
  }
  else
  {
    global_ret = ALL_OK;
    printf("Connected to broker.\n");

    //send message to topic
    mosquitto_publish(pub, NULL, topic, strlen(to_send), to_send, 0, false);

    mosquitto_disconnect(pub);
    mosquitto_destroy(pub);
  }

  //end mqtt
  mosquitto_lib_cleanup();

  return global_ret;
}
