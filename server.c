/*
    Handle multiple socket connections wite select and fd_seq on Linux
*/

#include <stdio.h>
#include <string.h>       //strlen,memset
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>       //close
#include <arpa/inet.h>    //close
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>     //FD_SET, FD_ISSET, FD_ZERO macros




#define MAX_EDGES 8
#define MAX_SWITCHES 6
#define M 3
#define K 7

#define TRUE 1
#define FALSE  0
#define PORT 8888
#define BUFLEN  1024 

#define TOPOLOGY_FILE "topology.txt"

int number_edges = 0;
int number_nodes = 0;
int reg_index = 0;
int top_index = 0;
int routing_index = 0;
int dijkstra_index = 1; // it serves as the node id as well
int verbosity = 0;
int ii;
int source_node = 0;
//int number_nodes = 0;

typedef struct
{
   int switch_id;
   // char *host;
   int port;
   int status; // 1 for active and 0 for inactive 
}reg_table;
/*
typedef struct neighbour
{
   int ids[MAX_SWITCHES];
   int active[MAX_SWITCHES];
   int port[MAX_SWITCHES];


}neighbour; */

typedef struct
{
   int link_capacity;
   int predecessor;
}dijkstra_table;



typedef struct
{
   int node1;
   int node2;
   int bandwidth;
   int delay;
   int status;
}top_table;

typedef struct
{
   int destination;
   int nexthop;
   int cost;

}routing_table;



void set_neighbouring_array(int dim[][MAX_SWITCHES], top_table *, int rows);
//void set_routing_table(top_table * table, int controller id, struct sockaddr_in serv, );



void switch_failed(top_table *table, int switch_id, int rows);
void switch_alive_again(top_table *table, int switch_id, int rows);

void link_fail(top_table *table, int switch_id_of_sender, int switch_id_of_failed_link, int rows);
void link_alive_again(top_table *table, int switch_id_of_sender, int switch_id_of_revived_link, int rows);
int check_link_status(top_table *table, int switch_id_1, int switch_id_2, int rows);

void file_top (top_table *);
void insert_reg(reg_table *, int, int, int);
void modify_reg(reg_table *);
void print_reg(reg_table *);

void insert_top(top_table *,int, int, int, int);
void modify_top(top_table *);
void print_top(top_table *);

void insert_routing(routing_table *, int, int, int);
void modify_routing(routing_table *);
void print_routing(routing_table *);

void widest_path(top_table *, dijkstra_table *, routing_table *, int);
int min(int, int);
bool not_found_nodes(int, int *, int);
int next_found(dijkstra_table *, int *, int);
void print_dijsktra_table(dijkstra_table *);
void build_routing_table(dijkstra_table *, routing_table *, int);

void write_log();


int main(int argc , char *argv[])
{

   struct sockaddr_in controller_addr, switch_addr, serv; //Defining Socket Structs for Server and Client
   int controller_addr_length, switch_addr_length, controller_socket_id, switch_socket_id; 
 
   reg_table Status[MAX_SWITCHES];
    
   top_table Topology_Table[MAX_EDGES];
 
   char *message;
   int max_fd;   // For select() first argument
   int switch_descriptor; //for registering switches to use in select()
   char buffer[BUFLEN];  //buffer for holding message. Here we should have the data structure to pass
   char update[] = "ROUTE_UPDATE";
   
   int switch_tracking_array[3][MAX_SWITCHES]; // Array to register multiple switches
   int alive[MAX_SWITCHES];
   int file_row = 0;
   int currently_alive[MAX_SWITCHES];
   int topo_rec_id = 0;
   int switch_neighbours[MAX_SWITCHES][MAX_SWITCHES]; 
   bool SET = false;   
   bool all_registered = false;
 
   int check_MK_seconds[MAX_SWITCHES];
   int switch_died[MAX_SWITCHES];

   int now_dead[MAX_SWITCHES];
   int temporary[1] = {0};

   struct timeval selTimeout;
   selTimeout.tv_sec = 10000;       /* timeout (secs.) */
   selTimeout.tv_usec = 0;            /* 0 microseconds */   
    
   bool checker = false;
   bool link_alive = false;
   bool link_failed = false;
   bool switch_dead_g = false;
   bool switch_lives = false;
   bool any_switch_dead = false;
   time_t start, end;


   fd_set rfds;   // Structure to use before all switches have registered
   fd_set set_rfds; // Structure with timeout M*K to use after all switches have registered 
   int read_now; //To get output of select function
   bool use_timer = false; 


   dijkstra_table d_table[MAX_SWITCHES+1];   
   int controller_activity; // holds return value for select() indicating some activity at controller socket
   int number_of_bytes_received;
   
   file_top(Topology_Table);
 
   number_nodes = MAX_SWITCHES;


   // bzero((reg_table *)&Status[0], sizeof(Status[0])*3 );
   int res;
   for(res = 0; res < MAX_SWITCHES; ++res)
   {
      Status[res].switch_id = 0;
      Status[res].port = 0;
      Status[res].status = 0;
 
   }  
   
   int tw = 0;	
   
   /* Initialize the d_table */

   for (tw = 0; tw < number_nodes + 1; tw++)
   {
      d_table[tw].link_capacity = 0;
      d_table[tw].predecessor = 0;	
   }

   //**----------------------------------
  
   if(argc == 1) verbosity = atoi(argv[1]);
    




   // static const struct reg_table EmptyStruct;
   // Status = EmptyStruct;
   

  
   routing_table routing_record[MAX_SWITCHES + 1]; 
 
   

   // initialize_space(&switch_neighbours); 
   memset(switch_neighbours, 0, sizeof(switch_neighbours[0][0]) * MAX_SWITCHES * MAX_SWITCHES);
 
   memset(switch_tracking_array, 0, sizeof(switch_tracking_array[0][0]) * 3 * MAX_SWITCHES);
  
   memset(alive, 0, sizeof(alive[0]) * MAX_SWITCHES); 
   memset(currently_alive, 0, sizeof(currently_alive[0]) * MAX_SWITCHES);
   memset(now_dead, 0, sizeof(now_dead[0])*MAX_SWITCHES);
   memset(check_MK_seconds, 0, sizeof(check_MK_seconds[0]) * MAX_SWITCHES);
   memset(switch_died, 0, sizeof(switch_died[0]) * MAX_SWITCHES);



   if(verbosity == 1) print_top(Topology_Table);
   
   set_neighbouring_array(switch_neighbours, Topology_Table, number_edges);
  
   
   

   
   int y1,y2;
   if(verbosity == 1)
   {  
      printf("Printing the switch neighbours in a 2d array\n"); 
      for(y1 = 0; y1 < number_nodes; ++y1)
      {
         for(y2 = 0; y2 < number_nodes; ++y2)
         {
           printf("%i  ", switch_neighbours[y1][y2]);

         }
         printf("\n");


      } 
   }
  
  
   controller_socket_id = socket(AF_INET , SOCK_DGRAM , 0);

   if(controller_socket_id == -1)
   {
      perror("Controller failed");
      exit(EXIT_FAILURE);
   }

    //Allow multiple connections

   int option = 1;
   if( setsockopt(controller_socket_id, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0 )
   {
      perror("setsockopt failure");
      exit(EXIT_FAILURE);
   }

   // zero out the structure
   memset((char *) &controller_addr, 0, sizeof(controller_addr));

   memset((char *) &serv, 0, sizeof(serv));
   //UDP Socket Struct parametres
   controller_addr.sin_family = AF_INET;
   controller_addr.sin_addr.s_addr = inet_addr("127.1.9.1");
   controller_addr.sin_port = htons( PORT );


   //bind the Controller to localhost port 8888
   if (bind(controller_socket_id, (struct sockaddr *)&controller_addr, sizeof(controller_addr))<0)
   {
      perror("bind failed");
      exit(EXIT_FAILURE);
   }
 

   int temp = 0, temp2, temp3; 
   
   controller_addr_length = sizeof(controller_addr); 
   switch_addr_length = sizeof(switch_addr);
   printf("Waiting for switches to register ...\n");
   
   if(verbosity == 1)
   { 
      for(res = 1; res < number_nodes + 1; ++res)
      {
                  printf("ID %i ALIVE %i PORT%i\n",Status[res-1].switch_id, Status[res-1].status, Status[res-1].port);

      } 
   }

   memset((char *) &serv, 0, sizeof(serv));

   while(TRUE)
   {
      
      if(use_timer)
      {
         selTimeout.tv_sec = M*K;       /* timeout (secs.) */
         selTimeout.tv_usec = 0;            /* 0 microseconds */
         use_timer = false;

      }
      
      if(checker)
      {
         selTimeout.tv_sec = M*K;       /* timeout (secs.) */
         selTimeout.tv_usec = 0;            /* 0 microseconds */
         checker = false;
      }

      //clear the socket set
      FD_ZERO(&rfds);
      FD_ZERO(&set_rfds);
      
      FD_SET(controller_socket_id, &rfds);
      FD_SET(controller_socket_id, &set_rfds);
      max_fd = controller_socket_id;

      
  
      
      
      
      
      controller_activity = select( max_fd + 1 , &rfds , NULL , NULL , &selTimeout);
      
     


      //printf("Hi I am after Select and select value is %i\n", controller_activity);
      //printf("Switch Value is after %d controller %d \n",switch_socket_id,controller_socket_id);

      //--------Error Handling----------------------------
      if ((controller_activity < 0) && (errno!=EINTR) && (read_now < 0))
      {
         printf("select error");
      }
      //--------------------------------------------------
      
      //If something happened for(nodo = 1; node < number_nodes + 1; ++nodo)
     // widest_path(Topology_Table, d_table, routing_record, source_node);on the master socket , then its an incoming connection
      if (controller_activity > 0 && FD_ISSET(controller_socket_id, &rfds))
      {
         memset(buffer,'\0', sizeof(buffer));
         
         if ((number_of_bytes_received = recvfrom(controller_socket_id, buffer, sizeof(buffer), 0, (struct sockaddr *) &switch_addr, &switch_addr_length)) == -1)
         {
            perror("recive from error");
            exit(EXIT_FAILURE);
         }

         // printf("The Type of message received %s\n", buffer);         
         
         if(strcmp(buffer,"REGISTER_REQUEST") == 0)
               
         {  



            // Should send acknowldegment to sender so that register is successful?
            memset(buffer,'\0',sizeof(buffer));
         
            if ((number_of_bytes_received = recvfrom(controller_socket_id, temporary, sizeof(temporary), 0, (struct sockaddr *) &switch_addr, &switch_addr_length)) == -1)
            {
               perror("recive from error");
               exit(EXIT_FAILURE);
       	    }
            
            temp = temporary[0];
            
            
            if(all_registered)
            {
               printf("Switch %i had previously failed. SO AGAIN, \n",temp);
    
            }
   	    
            printf("It was REGISTER_REQUEST from Switch %i \n", temp);
         
            
            Status[temp - 1].switch_id = temp;
            Status[temp - 1].port = ntohs(switch_addr.sin_port);         
            Status[temp - 1].status = 1;
            /*for(res = 1; res < number_nodes +1; ++res)
            {
               printf("ID %i ALIVE %i PORT%i\n",Status[res-1].switch_id, Status[res-1].status, Status[res-1].port);

            }*/
 
            for(temp2 = 0; temp2 < 3; ++temp2)
            {
               if(temp2 == 0)
	       { 
                  for(temp3 = 0; temp3 < number_nodes; ++temp3)
                  {
                     switch_tracking_array[temp2][temp3] = switch_neighbours[temp - 1][temp3];     
                  }
               }
               
               if(temp2 == 1)
               {

                 for(temp3 = 0; temp3 < number_nodes; ++temp3)
                  {
                     if((switch_neighbours[temp - 1][temp3] == 1) && Status[temp3].status == 1)
                     {
                          
                     switch_tracking_array[temp2][temp3] = 1;            
                     }
                  }
               }

               if(temp2 == 2)
               {

                 for(temp3 = 0; temp3 < number_nodes; ++temp3)
                  {
                     if((switch_neighbours[temp - 1][temp3] == 1) && Status[temp3].status == 1)
                     {

                     switch_tracking_array[temp2][temp3] = Status[temp3].port;
                     }
                  }
               }



	    }
            
            if(verbosity == 1)
            {
               for(temp2 = 0; temp2 < 3; ++temp2)
               {
                  for(temp3 = 0; temp3 < number_nodes; ++temp3)
                     printf("%i  ", switch_tracking_array[temp2][temp3]);
                  printf("\n");
               }
            }
            for(temp2 = 1; temp2 < number_nodes + 1; ++temp2)
            {
               printf("ID %i ALIVE %i PORT%i\n",Status[temp2-1].switch_id, Status[temp2-1].status, Status[temp2-1].port);  
  
            }          
 
            //Keep Track of Currently ALive Switches
            currently_alive[temp - 1] = 1;
            temp3 = 0;
            printf("Check which switches are currently alive\n");
            for(temp2 = 0; temp2 < number_nodes; ++temp2)
            {
               printf("%i ", currently_alive[temp2]);
               temp3 += currently_alive[temp2];    

            }
            printf("\n");

            //--------Check Here If all have registered and then perform path computations-------
            
            if(temp3 == number_nodes) 
            {
               SET = true; 
               printf("ALL SWITCHES NOW REGISTERED\n");
               use_timer = true; // NOW USE THE OTHER TIMER struct
               all_registered = true;

            }
           
             printf("REGISTER_RESPONSE was sent to Switch %i \n", temp);


            if (sendto(controller_socket_id, switch_tracking_array, sizeof(switch_tracking_array), 0, (struct sockaddr*) &switch_addr, switch_addr_length) == -1)
            {
               perror("send to method error");
               exit(EXIT_FAILURE);
            }
            memset(switch_tracking_array, 0, sizeof(switch_tracking_array[0][0]) * 3 * MAX_SWITCHES);

  
         }
	
         if(any_switch_dead) {
          
                          switch_alive_again(Topology_Table, temporary[0], number_edges);
           print_top(Topology_Table);}
         if(SET)
         {
                       
            for(source_node = 1; source_node < number_nodes + 1; ++source_node)
            {
               serv.sin_family = AF_INET;
               serv.sin_port = htons(Status[source_node - 1].port);
               serv.sin_addr.s_addr =inet_addr("127.1.9.1");
               //printf("Message sent to %i\n",Status[source_node - 1].port );
             

            
               widest_path(Topology_Table, d_table, routing_record, source_node);
     
               //print_dijkstra_table(d_table);			

	            //print_routing(routing_record);
               sendto(controller_socket_id, update, sizeof(update) , 0 , (struct sockaddr *) &serv, sizeof(serv));
               sendto(controller_socket_id, routing_record, sizeof(routing_record) , 0 , (struct sockaddr *) &serv, sizeof(serv));
               memset((char *) &serv, 0, sizeof(serv));
	          for (ii=0; ii<number_nodes+1; ii++)
	             {
	                d_table[ii].link_capacity = 0;
	                d_table[ii].predecessor = 0;	
	             }

	          for (ii=0; ii<number_nodes+1; ii++)
          	     {
		        routing_record[ii].destination = 0;
	                routing_record[ii].nexthop = 0;
	                routing_record[ii].cost	= 0;
	             }
                routing_index = 0;
                dijkstra_index = 1;  
             }

            printf("Sent ROUTE_UPDATE \n");
            SET = false;
        }


         if(strcmp(buffer,"TOPOLOGY_UPDATE") == 0)
         { 
            recvfrom(controller_socket_id, alive, sizeof(alive), 0, (struct sockaddr *) &switch_addr, &switch_addr_length);         
            //printf("Message was received from Port no %i\n", ntohs(switch_addr.sin_port));

            for(temp2 = 0; temp2 < number_nodes; ++temp2)
            {
               if(Status[temp2].port == ntohs(switch_addr.sin_port))
               {
                  topo_rec_id = Status[temp2].switch_id;

	       }
   
   
            } 
            
            
               
            if(verbosity == 1) printf("The TOPOLOGY_UPDATE message was received from SWITCH %i\n", topo_rec_id);
            
            if(all_registered)            
            { 
              
                  if(switch_died[topo_rec_id - 1] == 1)
                  {    
                     switch_died[topo_rec_id - 1] = 0;
                     
                     switch_alive_again(Topology_Table, topo_rec_id, number_edges);

                     switch_lives = true;           
                     //print_top(Topology_Table);
                     printf("SWITCH %i WAS PREVIOUSLY DEAD. Now alive again \n", topo_rec_id ) ;          
                  }
               
            }

            if(verbosity == 1) {for(temp2 = 0; temp2 < number_nodes; ++temp2)
            {
               printf("%i  ", alive[temp2]);
        
            }
            printf("\n");}
            //---------------SET check_MK_seconds array here-----------------
            
            if(all_registered)
            {
               for(temp2 = 0; temp2 < number_nodes; ++temp2)
               {
                  if(switch_neighbours[topo_rec_id - 1][temp2] != alive[temp2])
                  {
                     printf("The link between %i and %i failed after all switches have registered\n", topo_rec_id, temp2 + 1 );
                     link_fail(Topology_Table, topo_rec_id, temp2 + 1, number_edges); 
                      //print_top(Topology_Table);
                     link_failed = true;       
          
                  } 
                  if((switch_neighbours[topo_rec_id - 1][temp2] == alive[temp2]) && (check_link_status(Topology_Table, topo_rec_id, temp2 + 1, number_edges ) == 0))
                  {
                     

                     printf("The PREVIOUS DEAD LINK between %i and %i has been established again\n", topo_rec_id, temp2 + 1);
                     link_alive_again(Topology_Table, topo_rec_id, temp2 + 1, number_edges);
                      //print_top(Topology_Table);
                     link_alive = true;
                      
                  }
               }


            }

         

 
            check_MK_seconds[topo_rec_id - 1] = 1;  // 	CHeck the aliveness of switches after every M*K Seconds
            
          // memset(buffer, '\0', sizeof(buffer));            

         }

         if(strcmp(buffer, "TOPOLOGY_UPDATE_MK") == 0)
         {

            if ((number_of_bytes_received = recvfrom(controller_socket_id, now_dead , sizeof(now_dead), 0, (struct sockaddr *) &switch_addr, &switch_addr_length)) == -1)
            {
               perror("recive from error");
               exit(EXIT_FAILURE);
       	    }
            
            // Check the id from where it received something
            for(temp2 = 0; temp2 < number_nodes; ++temp2)
            {
               if(Status[temp2].port == ntohs(switch_addr.sin_port))
               {
                  topo_rec_id = Status[temp2].switch_id;

	       }
   
   
            }


            if(all_registered)            
            { 
              
                  if(switch_died[topo_rec_id - 1] == 1)
                  {    
                     switch_died[topo_rec_id - 1] = 0;
                     
                     switch_alive_again(Topology_Table, topo_rec_id, number_edges);
                    

                     switch_lives = true;
                      // print_top(Topology_Table);

                     printf("\n\nSWITCH %i was PREVIOUSLY DEAD. NOW ALIVE AGAIN\n\n", topo_rec_id ) ;          
                  }
               
            }
            
            
            for(temp2 = 0; temp2 < number_nodes; ++temp2)
            {
               if(now_dead[temp2] == 1)
                  {   
                     printf("\n\nThe LINKS FAILED between SWITCH %i and  SWITCH %i\n\n", topo_rec_id, temp2 + 1);
                  
                     link_fail(Topology_Table, topo_rec_id, temp2 + 1, number_edges);        
                     link_failed = true;
                   // print_top(Topology_Table);
                     
                  }
            }

            check_MK_seconds[topo_rec_id - 1] = 1;
 
           



         }



         memset(buffer, '\0', sizeof(buffer));
         memset((char *) &switch_addr, 0, sizeof(switch_addr)); 

        
       }
        
       if(controller_activity == 0)
       {
         
         checker = true;  // Now timer will be periodically reset. IT comes here only when all switches have registered

         // printf("Controller timer expired\n Checking which have not sent TOPOLOGY UPDATE yet\n");
        
         for(temp2 = 0; temp2 < number_nodes; ++temp2)
         {
            if(check_MK_seconds[temp2] == 0)
            {    
               currently_alive[temp2] = 0;
               switch_died[temp2] = 1 ;
               switch_failed(Topology_Table, temp2 + 1, number_edges);
               switch_dead_g = true;
               any_switch_dead = true;
               printf("Switch DEAD %i \n", temp2 +1 ) ;  
                       
            }
         } 

         // If any change in the topology table then send route update messages

//         if(verbosity == 2 || verbosity == 1)
if(switch_dead_g || switch_lives || link_failed || link_alive) 
          //if(switch_dead_g)
         {         
            printf("\n\n\nROUTING TABLES CHANGED. Sending UPDATED TABLES\n\n\n");

            for(source_node = 1; source_node < number_nodes + 1; ++source_node)
            {
               serv.sin_family = AF_INET;
               serv.sin_port = htons(Status[source_node - 1].port);
               serv.sin_addr.s_addr =inet_addr("127.1.9.1");
                         
               widest_path(Topology_Table, d_table, routing_record, source_node);
     
               sendto(controller_socket_id, update, sizeof(update) , 0 , (struct sockaddr *) &serv, sizeof(serv));
               sendto(controller_socket_id, routing_record, sizeof(routing_record) , 0 , (struct sockaddr *) &serv, sizeof(serv));
               memset((char *) &serv, 0, sizeof(serv));
	          for (ii=0; ii<number_nodes+1; ii++)
		  {
		     d_table[ii].link_capacity = 0;
		     d_table[ii].predecessor = 0;	
	          }

		  for (ii=0; ii<number_nodes+1; ii++)
		  {
		     routing_record[ii].destination = 0;
		     routing_record[ii].nexthop = 0;
		     routing_record[ii].cost	= 0;
	          }

                  routing_index = 0;
                  dijkstra_index = 1;  
               }
        
               


            }
             

         for(temp2 = 0; temp2 < number_nodes; ++temp2)
         {
            check_MK_seconds[temp2] = 0;           
  
         }
        
        if(switch_dead_g || switch_lives || link_failed || link_alive) 
        //if(switch_dead_g)
        {
          print_top(Topology_Table);
          switch_dead_g = false;
           switch_lives = false;
            link_failed = false;
         link_alive = false;
}

       }


           
    
   }
   close(controller_socket_id);
   return 0;
}

void set_neighbouring_array(int dim[][MAX_SWITCHES], top_table *table, int rows)
{
      
   int row, n1 = 0, n2 = 0; 
   for(row = 0; row < rows; row++)
   {
      n1 = table[row].node1;
      n2 = table[row].node2;  
      dim[n1 - 1][n2 - 1] = 1;
      dim[n2 - 1][n1 - 1] = 1;
   }


}

void link_fail(top_table *table, int switch_id_of_sender, int switch_id_of_failed_link, int rows)
{

   int row;
   for(row = 0; row < rows; ++row)
   {
      if((table[row].node1 == switch_id_of_sender ||  table[row].node2 == switch_id_of_sender) && (table[row].node1 == switch_id_of_failed_link ||  table[row].node2 == switch_id_of_failed_link))
            table[row].status = 0;      
   }
 

}

void link_alive_again(top_table *table, int switch_id_of_sender, int switch_id_of_revived_link, int rows)
{

   int row;
   for(row = 0; row < rows; ++row)
   {
     if((table[row].node1 == switch_id_of_sender ||  table[row].node2 == switch_id_of_sender) && (table[row].node1 == switch_id_of_revived_link ||  table[row].node2 == switch_id_of_revived_link))
            table[row].status = 1;      
   }
 

}


int check_link_status(top_table *table, int switch_id_1, int switch_id_2, int rows)
{
   int row;
   for(row = 0; row < rows; ++row)
   {
     if((table[row].node1 == switch_id_1 &&  table[row].node2 == switch_id_2) || (table[row].node1 == switch_id_1 &&  table[row].node2 == switch_id_2))
            return table[row].status;      
   }



}


void switch_failed(top_table *table, int switch_id, int rows)
{

   int row;
   for(row = 0; row < rows; ++row)
   {
      if(table[row].node1 == switch_id ||  table[row].node2 == switch_id)
         table[row].status = 0;      
   }

}



void switch_alive_again(top_table *table, int switch_id, int rows)
{

   int row;
   for(row = 0; row < rows; ++row)
   {
      if(table[row].node1 == switch_id ||  table[row].node2 == switch_id)
         table[row].status = 1;      
   }

}


void file_top(top_table *rec)
{
	
	int val1, val2, val3, val4;
	FILE *fp = fopen(TOPOLOGY_FILE, "r");
	char line[32];
	int count = 0;
	while(fgets(line, sizeof(line), fp) != NULL)
	{
		if (count == 0)
		{
			sscanf(line, "%d", &number_nodes);
			//printf("nodes_num: %d\n", number_nodes);
			count ++;
		}
		else
		{
			sscanf(line, "%d %d %d %d", &val1, &val2, &val3, &val4);
			//printf("node1: %d node2: %d link-bandwidth: %d link-delay: %d\n", val1, val2, val3, val4);
			insert_top(rec, val1, val2, val3, val4);
			count ++;
		}
	}

	number_edges = count - 1;
	fclose(fp);

}


void insert_reg(reg_table *rec,int row, int s_id, int s_port)
{
   rec[row].switch_id = s_id;
//	rec[reg_index].host = s_name;
   rec[row].port = s_port;
// 	rec[reg_index].status = s_status;

//	reg_index = reg_index + 1;
}

void modify_reg(reg_table *rec)
{

   printf("I'm in the modify record\n");

}

void print_reg(reg_table *rec)
{
   int i = 0;  //chnage here3...................loop thiungy
   printf("We have  records in the registration table\n");
   for (i = 0; i< 2; i++)
   {
   
   printf("switch id: %d, switch port: %d \n",  rec[i].switch_id, rec[i].port);

   }
}

void insert_top(top_table *rec, int nod1, int nod2, int bandw, int dl)
{
	rec[top_index].node1 = nod1;
	rec[top_index].node2 = nod2;
	rec[top_index].bandwidth = bandw;
 	rec[top_index].delay = dl ;
        rec[top_index].status = 1;
	top_index = top_index + 1;
}

void modify_top(top_table *rec)
{

   printf("I'm in the modify record\n");

}

void print_top(top_table *rec)
{
   int i;
   printf("We have %i switches records in the topology table\n", number_edges);
   for (i = 0; i< number_edges; ++i)
   {
      printf("node 1: %i, node 2: %i, bandwidth: %i, delay: %i, Status: %i\n",  rec[i].node1, rec[i].node2, rec[i].bandwidth, rec[i].delay, rec[i].status);

   }
}	


void insert_routing(routing_table *rec, int d_id, int nexthop, int cost)
{
	rec[routing_index].destination = d_id;
	rec[routing_index].nexthop = nexthop;
	rec[routing_index].cost = cost;

	routing_index = routing_index + 1;
}

void modify_routing(routing_table *rec)
{

   printf("I'm in the modify record\n");

}
void print_routing(routing_table *rec)
{
 	int i = 0;
	printf("We have %d records in the routing table\n", routing_index);
	for (i = 0; i< routing_index; i++)
	{
		printf("destination: %d, nexthop: %d, cost: %d \n",  rec[i].destination, rec[i].nexthop, rec[i].cost);

	}
}

//--- Widest Path Algorithm -----------//

void widest_path(top_table *top_record, dijkstra_table *table, routing_table *rec, int source)
{
	int found[number_nodes];
	int found_index = 0;
	int i;

	found[found_index] = source;
	found_index = found_index + 1;

	//printf("node inserted in found set: %d and found set index is: %d\n", found[found_index-1], found_index);

	//Build the found set insertion function

	while(found_index != number_nodes)
	{
	
		for (i=0; i<top_index; i++)
		{
			if(top_record[i].node1 == found[found_index-1] && not_found_nodes(top_record[i].node2, found, found_index) && top_record[i].status == 1)
			{
				//printf("I'm in the first IF\n");
				//printf("id of node2:%d , label of node2:%d , id of node1:%d, label of node1:%d, bandwidth of link n1 and n2: %d  \n", top_record[i].node2, table[top_record[i].node2].link_capacity, top_record[i].node1, table[top_record[i].node1].link_capacity, top_record[i].bandwidth);

				if (table[top_record[i].node2].link_capacity <= min(table[top_record[i].node1].link_capacity, top_record[i].bandwidth))
				{
	                       				//printf("I'm in the first SUB-IF\n");
					if (top_record[i].node1 == source) // checking if node1 in the current top_table row is source node or not
						table[top_record[i].node2].link_capacity = top_record[i].bandwidth;
					else
						table[top_record[i].node2].link_capacity = min(table[top_record[i].node1].link_capacity, top_record[i].bandwidth);
					table[top_record[i].node2].predecessor = top_record[i].node1;			
				} 
			}
			
			else if(top_record[i].node2 == found[found_index-1] && not_found_nodes(top_record[i].node1, found, found_index) && top_record[i].status == 1)
			{
				//printf("I'm in the Second IF\n");
				if (table[top_record[i].node1].link_capacity <= min(table[top_record[i].node2].link_capacity, top_record[i].bandwidth))
				{
				//	printf("I'm in the Second SUB-IF\n");
					if (top_record[i].node2 == source) // checking if node2 in the current top_table row is source node or not
						table[top_record[i].node1].link_capacity = top_record[i].bandwidth;
					else
						table[top_record[i].node1].link_capacity = min(table[top_record[i].node2].link_capacity, top_record[i].bandwidth);
					table[top_record[i].node1].predecessor = top_record[i].node2;			
				} 
			}
	
		}

		//Found set insertion 

		found[found_index] = next_found(table, found, found_index);
		found_index = found_index + 1;
		
		//printf("node inserted in found set: %d and found set index is: %d\n", found[found_index-1], found_index);
	
	}

// To make distinction between source node and the dead switch
for (i = 1; i < number_nodes + 1; i++)
	{
			if(table[i].predecessor ==  0 && i != source)
			{
				table[i].link_capacity = -1;
				table[i].predecessor = -1;			
			}
		
	}

build_routing_table(table, rec, source);

}

int min(int a, int b)
{
	if (a<b)
		return a;
	else
		return b;
}

bool not_found_nodes(int val, int *arr, int size)
{
	int i;
	for(i=0; i<size; i++)
	{
		if (arr[i] == val)
			return false;
	}
	return true;	
}

int next_found(dijkstra_table *table, int *array, int size)
{
	int i = 1;
	int max_cap = 0;
	int max_cap_node = 0;

	for (i = 1; i < number_nodes + 1; i++)
	{
		if(not_found_nodes(i, array, size))
		{
			if(table[i].link_capacity > max_cap)
			{
				max_cap = table[i].link_capacity;
				max_cap_node = i;			
			}
		}
	}
	
	return max_cap_node;
}

void print_dijkstra_table(dijkstra_table *table)
{
	int i = 0;
	printf("We have %d nodes in the Dijkstra table\n", number_nodes);
	for (i = 1; i < number_nodes+1; i++)
	{
		printf("node: %d, predecessor: %d, min cap in path: %d\n",  i, table[i].predecessor, table[i].link_capacity);

	}

}

void build_routing_table (dijkstra_table *table, routing_table *rec, int source)
{
	int i=0;
	int j=0;
	int k=0;
	int destination; // i corresponds to destination
	int nexthop; // j corresponds to nexthop
	int cost; // cost to reach destination from source

	for (i=1; i<number_nodes+1; i++)
	{
		if (i == source)
		{
			destination = source;
			cost = 0;
			j = source;						
		}

		else if (table[i].predecessor == -1)
		{
			j = -1;
		}

		else
		{
			j = i;
			k = table[j].predecessor;

			while(k!=source)
			{
				j = k;
				k = table[j].predecessor;
			}
		}

		destination = i;
		nexthop = j;
		cost = table[i].link_capacity;

		insert_routing(rec, destination, nexthop, cost);	
	}

}


void write_log()
{
	int i;
	char switch_no[16] = {0};
	char *msg = "Hello I am switch: ";
	
	FILE *fpointer = fopen("log.txt", "wb");

	for (i=0; i<7; i++)
	{	
		sprintf(switch_no, "%d", i);
		fputs(msg, fpointer);
		fputs(switch_no, fpointer);
		fputs("\n", fpointer);
	}
	fclose(fpointer);
}




