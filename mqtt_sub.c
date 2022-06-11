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

void connect_callback(struct mosquitto* sub, void* obj, int ret)
{
  printf("Connected ID: %d\n", *(int*)obj);
  if(ret != 0)
  {
    printf("Connection failed. Error: %d\n", ret);
    mosquitto_destroy(sub);
    exit(ERR_CONNECT);
  }
  mosquitto_subscribe(sub, NULL, topic, 0);
}

void receive_callback(struct mosquitto* sub, void* obj, const struct mosquitto_message* msg)
{
  printf("Message(%s): %s\n", msg->topic, (char*)msg->payload);
}

/*--------------------------------MAIN----------------------------------------*/

int main(int argc, char** argv)
{
  int global_ret = ALL_OK;

  //check input arguments
  if(argc != 4)
  {
    printf("Wrong input.\nPlease use: ./sub <ip> <ID> <full-topic>");
  }

  //load input arguments
  ip    = argv[1];
  ID    = argv[2];
  topic = argv[3];

  //set up publisher variables
  int ret;
  struct mosquitto* sub;

  //start mqtt
  mosquitto_lib_init();

  //create new subscriber instance
  sub = mosquitto_new(ID, true, NULL);

  //create callbacks
  mosquitto_connect_callback_set(sub, connect_callback);
  mosquitto_message_callback_set(sub, receive_callback);

  //attempt connection to broker
  ret = mosquitto_connect(sub, ip, 1883, 5);
  if(ret != 0)
  {
    global_ret = ERR_CONNECT;
    printf("Could not connect to broker. Error: %d\n", ret);
    mosquitto_destroy(sub);
  }
  else
  {
    //receiving loop
    mosquitto_loop_start(sub);

    printf("Press any key to quit...\n");
    getchar();

    mosquitto_loop_stop(sub, true);

    mosquitto_disconnect(sub);
    mosquitto_destroy(sub);
  }

  //end mqtt
  mosquitto_lib_cleanup();

  return global_ret;
}
