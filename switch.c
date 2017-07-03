#include <stdio.h>
#include <string.h>   //strlen,memset
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros




#define TRUE   1
#define FALSE  0
#define PORT 8888
#define BUFLEN  1024 
#define MAX_SWITCHES 6
#define K 7
#define M 3

#define TOPOLOGY_FILE "topology.txt"

typedef struct
{
   int destination;
   int nexthop;
   int cost;

}routing_table;

int number_nodes = MAX_SWITCHES;
int number_edges = 0;

/**********For command line inputs************/

	 int switch_id;
	 char *controller_hostname;
	 int  controller_port;
	 char *flag;
	 int fail_neighbour = 101;
	 int verbosity = 0; // 0 for default 1 for high

/**********************************************/

void print_routing(routing_table *rec);
void read1();

int main(int argc , char *argv[])
{

   read1();

   struct sockaddr_in switch_addr, serv, neighbour; //Defining Socket Structs for Server and Client
   
   int alive[MAX_SWITCHES];
   bzero((char *) &neighbour, sizeof(neighbour)); 
   int switch_socket_id, i, slen=sizeof(switch_addr), slen2=sizeof(neighbour), slen3 = sizeof(serv);
   char buf[BUFLEN];

   char ids[BUFLEN];
   char message[] = "REGISTER_REQUEST";
   char message2[] = "KEEP_ALIVE"; 
   char message3[] = "TOPOLOGY_UPDATE";
   char message4[] = "TOPOLOGY_UPDATE_MK";
  
   struct timeval read_timeout;
   read_timeout.tv_sec = 0;
   read_timeout.tv_usec = 5;


   int id, idd = 0;
   struct timeval selTimeout;
   selTimeout.tv_sec = K;       /* timeout (secs.) */
   selTimeout.tv_usec = 0;            /* 0 microseconds */
   fd_set readSet;
   int ready, temp2;
   int received_id;     
   int switch_neighbours[3][MAX_SWITCHES];
   int counter = 0;
   int currently_not_alive[MAX_SWITCHES];
   bool inactive = false;
  
   int keep_track[MAX_SWITCHES];
    //****************************************************//
   int sw[1] = {0};


  // read1();

   int sum = 0;




   //****************************DEAD SWITCH LINK

   int dead_link[MAX_SWITCHES];
   memset(dead_link, 0, sizeof(dead_link[0]) * MAX_SWITCHES);


  //*******************************************


   memset(ids, '\0', sizeof(ids));
   memset(buf, '\0', sizeof(buf));
   /**************** Command Line Arguments Processing ************************************/

   // for normal running do ./switch switchid controller_hostname controller_portnumber
   // e.g. ./switch 3 127.1.1.1 9494


   if (argc == 4)
   {
      switch_id = atoi(argv[1]);
      controller_hostname = argv[2];
      controller_port = atoi(argv[3]);
      buf[0] = *argv[1];   // This is the switch ID
      id = switch_id;
      ids[0] = buf[0];  // Change ID here too
      printf("switchID: %d, controllerHostname: %s, controllerPort: %d, buf %s\n", switch_id, controller_hostname, controller_port, ids);
    }

   // for normal running with high verbosity do ./switch switchid controller_hostname controller_portnumber verbosity_level
   // e.g. ./switch 3 127.1.1.1 9494 1



   if (argc == 5)
   {
      switch_id = atoi(argv[1]);
      controller_hostname = argv[2];
      controller_port = atoi(argv[3]);
      verbosity = atoi(argv[4]);
      buf[0] = *argv[1];   // This is the switch ID
      id = switch_id;
      ids[0] = *argv[1];  // Change ID here too
      printf("switchID: %d, controllerHostname: %s, controllerPort: %d, verbosityLevel: %d\n", switch_id, controller_hostname, controller_port, verbosity);
   }

    // for failure do ./switch switchid controller_hostname controller_portnumber -f failing_neighbor_id
	// e.g. ./switch 3 127.1.1.1 9494 -f 1

   if (argc == 6)
   {
      switch_id = atoi(argv[1]);
      controller_hostname = argv[2];
      controller_port = atoi(argv[3]);
      flag = argv[4];
      fail_neighbour = atoi(argv[5]);
      buf[0] = *argv[1];   // This is the switch ID
      id = switch_id;
      ids[0] = *argv[1];  // Change ID here too	
			
      printf("switchID: %d, controllerHostname: %s, controllerPort: %d, flag: %s, failNeighbor: %d\n", switch_id, controller_hostname, controller_port, flag, fail_neighbour);
   }   
      
   // for failure do ./switch switchid controller_hostname controller_portnumber -f failing_neighbor_id verbosity_level
   // e.g. ./switch 3 127.1.1.1 9494 -f 2 1

   if (argc == 7)
   {
      switch_id = atoi(argv[1]);
      controller_hostname = argv[2];
      controller_port = atoi(argv[3]);
      flag = argv[4];
      fail_neighbour = atoi(argv[5]);
      verbosity = atoi(argv[7]);
      buf[0] = *argv[1];   // This is the switch ID
      id = switch_id;
      ids[0] = *argv[1];  // Change ID here too	
			
      printf("switchID: %d, controllerHostname: %s, controllerPort: %d, flag: %s, failNeighbor: %d, verbosity_level: %d\n", switch_id, controller_hostname, controller_port, flag, fail_neighbour, verbosity);
   }




   routing_table routing_record[MAX_SWITCHES + 1]; 

   if ((switch_socket_id = socket(AF_INET, SOCK_DGRAM,0)) == -1)
   {
      perror("socket failed");
      exit(EXIT_FAILURE);
   }
 
   memset((char *) &switch_addr, 0, sizeof(switch_addr));
   memset(alive, 0, sizeof(alive[0]) * MAX_SWITCHES);
   memset((char *) &serv, 0, sizeof(serv));
   memset(currently_not_alive, 0, sizeof(currently_not_alive[0])*MAX_SWITCHES);
   memset(keep_track, 0, sizeof(keep_track[0]) * MAX_SWITCHES);   

   switch_addr.sin_family = AF_INET;
   switch_addr.sin_port = htons(controller_port);
   switch_addr.sin_addr.s_addr =inet_addr("127.1.9.1");

   serv.sin_family = AF_INET;
   serv.sin_port = htons(controller_port);
   serv.sin_addr.s_addr =inet_addr("127.1.9.1");

             

             
   int send_id[1] = {0};
   //memset(buf,'\0',sizeof(buf));
   //memset(ids, '\0', sizeof(ids));

   //-------------------------If nothing received on switch for 10 seconds then make recv unblock-----------------
   setsockopt(switch_socket_id, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
   
   sw[0] = switch_id;
   send_id[0] = switch_id;
   id = 0;

   // Switch Registering Process 
   sendto(switch_socket_id, message, sizeof(message) , 0 , (struct sockaddr *) &switch_addr, slen);

   sendto(switch_socket_id, sw, sizeof(sw) , 0 , (struct sockaddr *) &switch_addr, slen);
   
   printf("REGISTER_REQUEST is sent from SWITCH %i \n", switch_id);
   recvfrom(switch_socket_id, switch_neighbours, sizeof(switch_neighbours), 0, (struct sockaddr *) &switch_addr, &slen);
   
   
   printf("REGISTER_RESPONSE is received from the controller.\nNeighbouring SWITCH ID's\nAlive (1) or Dead(0) status\nPort numbers for alive switches are:\n");
   
   //--------------Finding out the number of nodes here---------------------------
   
    



   //LOGGING INFO HERE
   int y1, y2;
   if(verbosity == 2)
   {
      for(y1 = 0; y1 < 3; ++y1)
      {
         for(y2 = 0; y2 < number_nodes; ++y2)
            printf("%i  ",switch_neighbours[y1][y2]);
         printf("\n");
      }
   }
 
  
   for(y1 = 0; y1 < 3; ++y1)
      {
         for(y2 = 0; y2 < number_nodes; ++y2)
         {
             if(y1 == 0) if(switch_neighbours[0][y2] == 1 )printf("%i      ", y2 + 1);
 
             
             if(y1 == 1) 
             {   
                
                if (switch_neighbours[0][y2] == 1 && switch_neighbours[y1][y2] == 1) printf("%i      ", switch_neighbours[y1][y2]);
               

             }
             if(y1 == 2) 
             {   
                
                if (switch_neighbours[0][y2] == 1 && switch_neighbours[1][y2] == 1) printf("%i ", switch_neighbours[y1][y2]);
                

             }
         }
         printf("\n");
      } 
 
    printf("\n");

   //Initial KEEP ALIVE messages after learning about the active neighbours
   for(y1 = 0; y1 < number_nodes; ++y1)
   {
      if(switch_neighbours[1][y1] == 1 && y1 != (fail_neighbour - 1))
      {
         neighbour.sin_family = AF_INET;
         neighbour.sin_port = htons(switch_neighbours[2][y1]);
         neighbour.sin_addr.s_addr =inet_addr("127.1.9.1");
   

         sendto(switch_socket_id, message2, sizeof(message2) , 0 , (struct sockaddr *) &neighbour, slen2);
         if(verbosity == 1)  printf("Sent KEEP ALIVE from %i\n", switch_id);

         sendto(switch_socket_id, buf, sizeof(buf) , 0 , (struct sockaddr *) &neighbour, slen2);
     

      }


   }

   int up = 0;

   memset(buf, '\0', BUFLEN);
  
   bool checker = false;

   while(TRUE)
   {
      //do
      //{
    if(checker)
    {
       selTimeout.tv_sec = K;       /* timeout (secs.) */
       selTimeout.tv_usec = 0;            /* 0 microseconds */
       checker = false;
    }

      FD_ZERO(&readSet);
      FD_SET(switch_socket_id, &readSet);
     
      ready = select(switch_socket_id+1, &readSet, NULL, NULL, &selTimeout);
      if(verbosity == 2) printf("select released in switch %i and select return value is%i\n", switch_id, ready);
      memset(buf, '\0', sizeof(buf));      
     
      //}while (ready == -1 && errno == EINTR);
      
      //--------Error Handling----------------------------
      if ((ready < 0) && (errno != EINTR))
      {
         printf("select error");
      }
      //--------------------------------------------------      
     

      if(ready > 0 && FD_ISSET(switch_socket_id, &readSet))
      {
     
         memset(buf,'\0', BUFLEN);
 
         if(verbosity == 2) printf("A message is received on switch %i\n", switch_id);
   
      
         if (recvfrom(switch_socket_id, buf, sizeof(buf), 0, (struct sockaddr *) &switch_addr, &slen) == -1)
    
         {
            perror("Recieve from failed after ISSET");
            exit(EXIT_FAILURE);
         }
         if(verbosity == 2) printf("Type of message received is %s\n", buf);      
       
         if(strcmp(buf,"KEEP_ALIVE") == 0)
         {
            memset(buf, '\0', BUFLEN);         

            
            if (recvfrom(switch_socket_id, sw, sizeof(sw), 0, (struct sockaddr *) &switch_addr, &slen) == -1)
            {
               perror("RECEIVE from failed in KEEP ALIVE");
               exit(EXIT_FAILURE);
            }        
            id = sw[0];
            if(id == fail_neighbour)
               alive[fail_neighbour  - 1] = 0;
        
            if(id != fail_neighbour)
            {
               if(verbosity == 2) printf("KEEP ALIVE Received from %s:%i\n", inet_ntoa(switch_addr.sin_addr), ntohs(switch_addr.sin_port));

               if(verbosity == 1 || verbosity == 2 )printf("KEEP ALIVE received from SWITCH %i \n", id);
            


            

               // Setting the status of received switches
               switch_neighbours[1][id - 1] = 1;
         
               switch_neighbours[2][id - 1] = ntohs(switch_addr.sin_port);
               keep_track[id - 1] = 1;       //We will reset this array after every M*K seconds
               // Just printing to check         
               if(verbosity == 2)
               {   
                  printf("SWITCH_NEIGHBOUR info received\n");
                  for(y1 = 0; y1 < 3; ++y1)
                  {
                     for(y2 = 0; y2 < number_nodes; ++y2)
                        printf("%i  ", switch_neighbours[y1][y2]);
                     printf("\n");


                  }
               }
               // ------------------------------UNREACHEABLE now REACHABLE again PRINTING--------------------------------------------------
               for(y1 = 0; y1 < number_nodes; ++y1)
               {
                  if(dead_link[y1] == 1 && id == y1 + 1)     //This will only be true when all links have registered
                  {
                     printf("\n\nNEIGHBOR SWITCH %i is REACHABLE again.\n\n ", id); 
                     dead_link[y1] = 0 ;  
                  }            
            
               }  
  
               //-----------------------------------------------------------------------------
      
  
            }
         }   


         if(strcmp(buf,"ROUTE_UPDATE") == 0)
         {
            
            printf("ROUTE_UPDATE message received from controller\n");

            if (recvfrom(switch_socket_id, routing_record, sizeof(routing_record), 0, (struct sockaddr *) &switch_addr, &slen) == -1)
            {
               perror("RECEIVE from failed in KEEP ALIVE");
               exit(EXIT_FAILURE);
            }
          
            print_routing(routing_record);           

         }
       

      }
    
      if(ready == 0)    // Timer Expired
      {
         

         checker = true;
         if(verbosity == 2) printf("Timer Expired After K seconds\n");
          
         // Counter to keep track of time
         counter++;
         
            if(counter == M)
            {
  
               for(y1 = 0; y1 < MAX_SWITCHES; ++y1)
               {
                  if (switch_neighbours[0][y1] == 1 && switch_neighbours[0][y1] != switch_neighbours[1][y1]  )
                  {
                     currently_not_alive[y1] = 1;    //It means this link is not alive anymore after M*K seconds     

                     
                     inactive = true;

       
                     dead_link[y1] = 1;        //***********************************************************************NEW LINW ADDED
                  }      
               }         
               
         
         

               for(y1 = 0; y1 < number_nodes; ++y1)
               {
                  sum = sum + currently_not_alive[y1] ;
                    
         
               }
                
               
               
               if(inactive) 
               {
                  printf("For SWITCH %i Currently unreachable NEIGHBORS are:\n", switch_id);
 
                  for(y1 = 0; y1 < number_nodes; ++y1)
                  {
                     if(currently_not_alive[y1] == 1) printf("%i  ", y1 + 1);
                  }
                  printf("\n");
               }   

              
              
               if(inactive)    
               {
                  if(verbosity == 1)     printf("Sending M*K messages \n");         
                  sendto(switch_socket_id, message4, sizeof(message4) , 0 , (struct sockaddr *) &serv, sizeof(serv));
                  sendto(switch_socket_id, currently_not_alive, sizeof(currently_not_alive) , 0 , (struct sockaddr *) &serv, sizeof(serv));


     
               }
               counter = 0; 
               // Reset keep_track array
               for(y1 = 0; y1 < number_nodes; ++y1)
               {
                  inactive = false;            
                  alive[y1] = 0;
                  currently_not_alive[y1] = 0;
                  switch_neighbours[1][y1] = 0;               //Set alive neighbours again
                  switch_neighbours[1][y1] = keep_track[y1];
                  keep_track[y1] = 0;
               }
            }

            if(counter != M)
            {       


               sendto(switch_socket_id, message3, sizeof(message3) , 0 , (struct sockaddr *) &serv, sizeof(serv));
        
               for(temp2 = 0; temp2 < number_nodes; ++temp2)
	       {
                  if(switch_neighbours[1][temp2] == 1)
                     alive[temp2] = 1;


	       }  
               
               sendto(switch_socket_id, alive, sizeof(alive) , 0 , (struct sockaddr *) &serv, sizeof(serv));
               
              
                if(verbosity == 2)printf("TOPOLOGY MEssages sent from %i\n", switch_id);
   
            }       



          if(verbosity == 1)printf("Now sending keep alive from switch %i\n", switch_id);

         for(y1 = 0; y1 < number_nodes; ++y1)
         {
            if(switch_neighbours[2][y1] != 0 && y1 != (fail_neighbour - 1))                    // Keep sending to the port number alive messages
            {
               neighbour.sin_family = AF_INET;
               neighbour.sin_port = htons(switch_neighbours[2][y1]);
               neighbour.sin_addr.s_addr =inet_addr("127.1.9.1");


               sendto(switch_socket_id, message2, sizeof(message2) , 0 , (struct sockaddr *) &neighbour, slen2);
               sendto(switch_socket_id, send_id, sizeof(send_id) , 0 , (struct sockaddr *) &neighbour, slen2);
                if(verbosity == 1) printf("Keep alive sent to switch %i\n", switch_neighbours[2][y1]);   
               memset((char *) &neighbour, 0, sizeof(neighbour));
            }



         }

       
      }
      

      
   }
   
   close(switch_socket_id);
   return 0;
}

void print_routing(routing_table *rec)
{
 	int i = 0;
	printf("Routing Table of Switch: %d\n", switch_id);
	for (i = 0; i< number_nodes; i++)
	{
		printf("source: %d | destination: %d | nexthop: %d | cost: %d \n",  switch_id, rec[i].destination, rec[i].nexthop, rec[i].cost);

	}
}

void read1()
{
	FILE *fp = fopen(TOPOLOGY_FILE, "r");
	char line[32];
	int count = 0;
	while(fgets(line, sizeof(line), fp) != NULL)
	{
		if (count == 0)
		{
			sscanf(line, "%d", &number_nodes);
			count ++;
		}
		else
		{
			count ++;
		}
	}

		number_edges = count - 1;
		fclose(fp);
}





