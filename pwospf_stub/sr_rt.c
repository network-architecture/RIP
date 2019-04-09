/*-----------------------------------------------------------------------------
 * file:  sr_rt.c
 * date:  Mon Oct 07 04:02:12 PDT 2002
 * Author:  casado@stanford.edu
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>


#include <sys/socket.h>
#include <netinet/in.h>
#define __USE_MISC 1 /* force linux to show inet_aton */
#include <arpa/inet.h>

#include "sr_rt.h"
#include "sr_if.h"
#include "sr_utils.h"
#include "sr_router.h"
#include "sr_protocol.h"

struct sr_rt* sr_longest_prefix_match(struct sr_instance* sr, uint32_t my_ip) {
	struct sr_rt* my_iter = sr->routing_table;
	uint32_t best_match = 0;
	struct sr_rt* result = 0;
	while (my_iter) {
		if ((my_iter->mask.s_addr & my_iter->dest.s_addr) == (my_iter->mask.s_addr & my_ip) && my_iter->metric<16 && sr_obtain_interface_status(sr, my_iter->interface)) {
		   	if ((best_match < my_iter->mask.s_addr) || !result) {
		   		result = my_iter;
		   		best_match = my_iter->mask.s_addr;
		   	}
		}
		my_iter = my_iter->next;
	}
	if(!result){
	my_iter = sr->routing_table;
		while (my_iter) {
			if (((my_iter->mask.s_addr<<4) & my_iter->dest.s_addr) == ((my_iter->mask.s_addr<<4) & my_ip) && my_iter->metric<16 && sr_obtain_interface_status(sr, my_iter->interface)) {
			   	if ((best_match < my_iter->mask.s_addr) || !result) {
			   		result = my_iter;
			   		best_match = my_iter->mask.s_addr;
			   	}
			}
			my_iter = my_iter->next;
		}
	}
	if(!result){
	my_iter = sr->routing_table;
		while (my_iter) {
			if (((my_iter->mask.s_addr<<8) & my_iter->dest.s_addr) == ((my_iter->mask.s_addr<<8) & my_ip) && my_iter->metric<16 && sr_obtain_interface_status(sr, my_iter->interface)) {
			   	if ((best_match < my_iter->mask.s_addr) || !result) {
			   		result = my_iter;
			   		best_match = my_iter->mask.s_addr;
			   	}
			}
			my_iter = my_iter->next;
		}
	}
	if(!result){
	my_iter = sr->routing_table;
		while (my_iter) {
			if (((my_iter->mask.s_addr<<12) & my_iter->dest.s_addr) == ((my_iter->mask.s_addr<<12) & my_ip) && my_iter->metric<16 && sr_obtain_interface_status(sr, my_iter->interface)) {
			   	if ((best_match < my_iter->mask.s_addr) || !result) {
			   		result = my_iter;
			   		best_match = my_iter->mask.s_addr;
			   	}
			}
			my_iter = my_iter->next;
		}
	}
	if(!result){
	my_iter = sr->routing_table;
		while (my_iter) {
			if (((my_iter->mask.s_addr<<16) & my_iter->dest.s_addr) == ((my_iter->mask.s_addr<<16) & my_ip) && my_iter->metric<16) {
			   	if ((best_match < my_iter->mask.s_addr) || !result) {
			   		result = my_iter;
			   		best_match = my_iter->mask.s_addr;
			   	}
			}
			my_iter = my_iter->next;
		}
	}
	return result;
}

/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/

int sr_load_rt(struct sr_instance* sr,const char* filename)
{
    FILE* fp;
    char  line[BUFSIZ];
    char  dest[32];
    char  gw[32];
    char  mask[32];    
    char  iface[32];
    struct in_addr dest_addr;
    struct in_addr gw_addr;
    struct in_addr mask_addr;
    int clear_routing_table = 0;

    /* -- REQUIRES -- */
    assert(filename);
    if( access(filename,R_OK) != 0)
    {
        perror("access");
        return -1;
    }

    fp = fopen(filename,"r");

    while( fgets(line,BUFSIZ,fp) != 0)
    {
        sscanf(line,"%s %s %s %s",dest,gw,mask,iface);
        if(inet_aton(dest,&dest_addr) == 0)
        { 
            fprintf(stderr,
                    "Error loading routing table, cannot convert %s to valid IP\n",
                    dest);
            return -1; 
        }
        if(inet_aton(gw,&gw_addr) == 0)
        { 
            fprintf(stderr,
                    "Error loading routing table, cannot convert %s to valid IP\n",
                    gw);
            return -1; 
        }
        if(inet_aton(mask,&mask_addr) == 0)
        { 
            fprintf(stderr,
                    "Error loading routing table, cannot convert %s to valid IP\n",
                    mask);
            return -1; 
        }
        if( clear_routing_table == 0 ){
            printf("Loading routing table from server, clear local routing table.\n");
            sr->routing_table = 0;
            clear_routing_table = 1;
        }
        sr_add_rt_entry(sr,dest_addr,gw_addr,mask_addr,(uint32_t)0,iface);
    } /* -- while -- */

    return 0; /* -- success -- */
} /* -- sr_load_rt -- */

/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/
int sr_build_rt(struct sr_instance* sr){
    struct sr_if* interface = sr->if_list;
    char  iface[32];
    struct in_addr dest_addr;
    struct in_addr gw_addr;
    struct in_addr mask_addr;

    while (interface){
        dest_addr.s_addr = (interface->ip & interface->mask);
        gw_addr.s_addr = 0;
        mask_addr.s_addr = interface->mask;
        strcpy(iface, interface->name);
        sr_add_rt_entry(sr, dest_addr, gw_addr, mask_addr, (uint32_t)0, iface);
        interface = interface->next;
    }
    return 0;
}

void sr_add_rt_entry(struct sr_instance* sr, struct in_addr dest,
struct in_addr gw, struct in_addr mask, uint32_t metric, char* if_name)
{   
    struct sr_rt* rt_walker = 0;

    /* -- REQUIRES -- */
    assert(if_name);
    assert(sr);

    pthread_mutex_lock(&(sr->rt_lock));
    /* -- empty list special case -- */
    if(sr->routing_table == 0)
    {
        sr->routing_table = (struct sr_rt*)malloc(sizeof(struct sr_rt));
        assert(sr->routing_table);
        sr->routing_table->next = 0;
        sr->routing_table->dest = dest;
        sr->routing_table->gw   = gw;
        sr->routing_table->mask = mask;
        strncpy(sr->routing_table->interface,if_name,sr_IFACE_NAMELEN);
        sr->routing_table->metric = metric;
        time_t now;
        time(&now);
        sr->routing_table->updated_time = now;

        pthread_mutex_unlock(&(sr->rt_lock));
        return;
    }

    /* -- find the end of the list -- */
    rt_walker = sr->routing_table;
    while(rt_walker->next){
      rt_walker = rt_walker->next; 
    }

    rt_walker->next = (struct sr_rt*)malloc(sizeof(struct sr_rt));
    assert(rt_walker->next);
    rt_walker = rt_walker->next;

    rt_walker->next = NULL;
    rt_walker->dest = dest;
    rt_walker->gw   = gw;
    rt_walker->mask = mask;
    strncpy(rt_walker->interface,if_name,sr_IFACE_NAMELEN);
    rt_walker->metric = metric;
    time_t now;
    time(&now);
    rt_walker->updated_time = now;
    
     pthread_mutex_unlock(&(sr->rt_lock));
} /* -- sr_add_entry -- */

/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/

void sr_print_routing_table(struct sr_instance* sr)
{
    pthread_mutex_lock(&(sr->rt_lock));
    struct sr_rt* rt_walker = 0;

    if(sr->routing_table == 0)
    {
        printf(" *warning* Routing table empty \n");
        pthread_mutex_unlock(&(sr->rt_lock));
        return;
    }
    printf("  <---------- Router Table ---------->\n");
    printf("Destination\tGateway\t\tMask\t\tIface\tMetric\tUpdate_Time\n");

    rt_walker = sr->routing_table;
    
    while(rt_walker){
        if (rt_walker->metric < INFINITY)
            sr_print_routing_entry(rt_walker);
        rt_walker = rt_walker->next;
    }
    pthread_mutex_unlock(&(sr->rt_lock));


} /* -- sr_print_routing_table -- */

/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/

void sr_print_routing_entry(struct sr_rt* entry)
{
    /* -- REQUIRES --*/
    assert(entry);
    assert(entry->interface);
    
    char buff[20];
    struct tm* timenow = localtime(&(entry->updated_time));
    strftime(buff, sizeof(buff), "%H:%M:%S", timenow);
    printf("%s\t",inet_ntoa(entry->dest));
    printf("%s\t",inet_ntoa(entry->gw));
    printf("%s\t",inet_ntoa(entry->mask));
    printf("%s\t",entry->interface);
    printf("%d\t",entry->metric);
    printf("%s\n", buff);

} /* -- sr_print_routing_entry -- */


void *sr_rip_timeout(void *sr_ptr) {
    struct sr_instance *sr = sr_ptr;
    while (1) {
        sleep(5);
        pthread_mutex_lock(&(sr->rt_lock));
        /* Fill your code here */
        send_rip_request(sr);
        struct sr_rt *current = NULL;
        if(sr->routing_table != NULL){
            current = sr->routing_table;
        }
        while(current->next != NULL){
	    time_t now;
            time(&now);
            double dif = difftime(now, current->updated_time);
            if(dif > 20 && current->gw.s_addr != 0){
                current->metric = 16;
            }
	    if(sr_obtain_interface_status(sr, current->interface)==0){
		/*current->metric = 16;*/
	    }
            current = current->next;
        }


	/*struct sr_if *cur1 = sr->if_list;
	while(cur1 != NULL){
		if(sr_obtain_interface_status(sr, cur1->name)==1){
			struct sr_rt *cur2 = sr->routing_table;
			int flag = 1;
			while(cur2 != NULL){
				struct sr_if *test = sr_get_interface(sr,cur2->interface);
				if(cur1->ip == test->ip && cur2->metric < 10){
					flag = 0;
				}
				cur2 = cur2->next;
			}
			if(flag){
				printf("ADDING");
				struct in_addr dest;
	    			dest.s_addr = cur1->mask&cur1->ip;
	    			struct in_addr gw;
				gw.s_addr = 0;
	    			struct in_addr mask;
	    			mask.s_addr = cur1->mask;
				sr_add_rt_entry(sr, dest, gw, mask, 0, cur1->name);
				send_rip_update(sr);
			}		
		}			
		cur1 = cur1->next;
	}*/


        pthread_mutex_unlock(&(sr->rt_lock));
    }
    return NULL;
}

void send_rip_request(struct sr_instance *sr){
	/* Send a RIP request to each neighbor */
	struct sr_if* current = sr->if_list;
	while (current != NULL) {
		uint8_t ffff[6] = {255,255,255,255,255,255};		
		unsigned int len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_udp_hdr_t) + sizeof(sr_rip_pkt_t);
   		uint8_t* buf = (uint8_t*)calloc(1,len);
		/* Ethernet Header */
   	 	sr_ethernet_hdr_t* eth_hdr = (sr_ethernet_hdr_t*)(buf);
    		eth_hdr->ether_type = htons(ethertype_ip);
		memcpy(eth_hdr->ether_shost, current->addr, 6);
		memcpy(eth_hdr->ether_dhost, ffff, 6);
		
		/* IP Header */
    		sr_ip_hdr_t *new_ip_hdr = (sr_ip_hdr_t*)(eth_hdr+1);
    		new_ip_hdr->ip_hl = 5;
    		new_ip_hdr->ip_v = 4;
    		/*new_ip_hdr->ip_tos = my_ip_hdr->ip_tos;*/
    		new_ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t));
    		new_ip_hdr->ip_id = 0;
    		new_ip_hdr->ip_off = 0;
    		new_ip_hdr->ip_ttl = 64;
    		new_ip_hdr->ip_p = ip_protocol_udp;
    		new_ip_hdr->ip_sum = cksum(buf + sizeof(sr_ethernet_hdr_t), sizeof(sr_ip_hdr_t));
		new_ip_hdr->ip_dst = 4294967295;
		/*new_ip_hdr->ip_dst = current->ip | ~current->mask;*/   
    		new_ip_hdr->ip_src = current->ip;
  
    	        /* UDP Header */
		sr_udp_hdr_t *udp_hdr = (sr_udp_hdr_t*)(new_ip_hdr + 1);
		udp_hdr->port_src = 520;
		udp_hdr->port_dst = 520;
		udp_hdr-> udp_len = htons(sizeof(sr_udp_hdr_t) + sizeof(sr_rip_pkt_t));

		/* RIP Packet */
		sr_rip_pkt_t *rip_pkt = (sr_rip_pkt_t*)(udp_hdr + 1);
		rip_pkt->command = 1;
		rip_pkt->version = 2;
		rip_pkt->unused = 0;	                              	
		sr_send_packet(sr, buf, len, current->name);
		free(buf);
		current = current->next;
	}
}

/* Call when you receive a RIP request packet or in the sr_rip_timeout function. You should enable split horizon here to prevent count-to-infinity problem */
void send_rip_update(struct sr_instance *sr){
        pthread_mutex_lock(&(sr->rt_lock));
        /* Send a RIP response to all neighbors */
	int rip_index;
	struct sr_if *current = sr->if_list;
	while (current != NULL) {
		/* Populate packet fields */
		unsigned int len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_udp_hdr_t) + sizeof(sr_rip_pkt_t);		
   		uint8_t* buf = (uint8_t*)malloc(len);
		uint8_t ffff[6] = {255,255,255,255,255,255};
   
		/* Ethernet Header */
   	 	sr_ethernet_hdr_t* eth_hdr = (sr_ethernet_hdr_t*)(buf);
    		eth_hdr->ether_type = htons(ethertype_ip);
		memcpy(eth_hdr->ether_shost, current->addr, 6);
		memcpy(eth_hdr->ether_dhost, ffff, 6);

		/* IP Header */
    		sr_ip_hdr_t *new_ip_hdr = (sr_ip_hdr_t*)(eth_hdr+1);
    		new_ip_hdr->ip_hl = 5;
    		new_ip_hdr->ip_v = 4;
    		/*new_ip_hdr->ip_tos = my_ip_hdr->ip_tos;*/
    		new_ip_hdr->ip_len = htons(len - sizeof(sr_ethernet_hdr_t));
    		new_ip_hdr->ip_id = 0;
    		new_ip_hdr->ip_off = 0;
    		new_ip_hdr->ip_ttl = 64;
    		new_ip_hdr->ip_p = ip_protocol_udp;
    		new_ip_hdr->ip_sum = cksum(buf + sizeof(sr_ethernet_hdr_t), sizeof(sr_ip_hdr_t));
		new_ip_hdr->ip_dst = 4294967295;
		/*new_ip_hdr->ip_dst = current->ip | ~current->mask;*/  
    		new_ip_hdr->ip_src = current->ip;
  
    	        /* UDP Header */
		sr_udp_hdr_t *udp_hdr = (sr_udp_hdr_t*)(new_ip_hdr+1);
		udp_hdr->port_src = 520;
		udp_hdr->port_dst = 520;
		udp_hdr-> udp_len = htons(sizeof(sr_udp_hdr_t) + sizeof(sr_rip_pkt_t));

		/* RIP Packet */
		sr_rip_pkt_t *rip_pkt = (sr_rip_pkt_t*)(udp_hdr+1);
		rip_pkt->command = 2;
		rip_pkt->version = 2;
		rip_pkt->unused = 0;
		struct sr_rt *my_rip_entry = sr->routing_table;
		rip_index = 0;
		while (my_rip_entry != NULL) {
			if(sr_obtain_interface_status(sr, my_rip_entry->interface)==0){
				my_rip_entry = my_rip_entry->next;
			}
			else{
				if((my_rip_entry->gw.s_addr&current->mask) != (current->ip&current->mask)){
					rip_pkt->entries[rip_index].afi = 2;
					rip_pkt->entries[rip_index].tag = 13;
					rip_pkt->entries[rip_index].metric = my_rip_entry->metric;
			       		rip_pkt->entries[rip_index].address = my_rip_entry->dest.s_addr;
					rip_pkt->entries[rip_index].mask = my_rip_entry->mask.s_addr;
					rip_pkt->entries[rip_index].next_hop = current->ip;
					rip_index++;
				}
				my_rip_entry = my_rip_entry->next;
				
			}		
        	}
		while (rip_index < 25){
			rip_pkt->entries[rip_index].afi = NULL;
			rip_pkt->entries[rip_index].tag = NULL;
			rip_pkt->entries[rip_index].metric = NULL;
               		rip_pkt->entries[rip_index].address = NULL;
                	rip_pkt->entries[rip_index].mask = NULL;
			rip_pkt->entries[rip_index].next_hop = NULL;
                	rip_index++;
		}
		sr_send_packet(sr, buf, len, current->name);                              
    		free(buf);
		current = current->next;
	}
        pthread_mutex_unlock(&(sr->rt_lock));
}

/* Called after receiving a RIP response packet. You should enable triggered updates here. When the routing table changes, the router will send a RIP response immediately */
void update_route_table(struct sr_instance *sr, sr_ip_hdr_t* ip_packet, sr_rip_pkt_t* rip_packet, char* iface){
        pthread_mutex_lock(&(sr->rt_lock));		
	struct sr_rt *current;
	int i;
	int cost_to_neighbor = 1;
	int new_entry;
	for (i = 0; i < 25; i++) {
		if (rip_packet->entries[i].address != NULL) {
			int flag = 0;
			struct sr_if *cur1 = sr->if_list;
			while(cur1 != NULL){
				if((rip_packet->entries[i].mask&rip_packet->entries[i].address)==(cur1->ip&rip_packet->entries[i].mask)){
					flag = 1;
				}
				cur1 = cur1->next;
			}
			if(flag){
				/*continue;*/
			}
			else{			
			current = sr->routing_table;
			new_entry = 1; 
			while (current != NULL) {
				if((current->dest.s_addr&current->mask.s_addr)==(rip_packet->entries[i].mask&rip_packet->entries[i].address)) {
					new_entry = 0;
					if (rip_packet->entries[i].metric + cost_to_neighbor < current->metric) {
		            			current->metric = rip_packet->entries[i].metric + cost_to_neighbor;
						current->gw.s_addr = ip_packet->ip_src;
						memcpy(current->interface, iface, 32);
						time_t now;
        					time(&now);
		            			current->updated_time = now;
						send_rip_update(sr);
		        		}
				}
				current = current->next;
			}
			if (new_entry == 1) {					
					struct in_addr dest;
	    				dest.s_addr = rip_packet->entries[i].mask&rip_packet->entries[i].address;
	    				struct in_addr gw;
					gw.s_addr = ip_packet->ip_src;
	    				struct in_addr mask;
	    				mask.s_addr = rip_packet->entries[i].mask;
					sr_add_rt_entry(sr, dest, gw, mask, rip_packet->entries[i].metric + cost_to_neighbor, iface);
					new_entry = 0;
					send_rip_update(sr);
			}
			}
		}
	}
        pthread_mutex_unlock(&(sr->rt_lock));
}


