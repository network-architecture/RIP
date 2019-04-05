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
		if ((my_iter->mask.s_addr & my_iter->dest.s_addr) == (my_iter->mask.s_addr & my_ip)) {
		   	if ((best_match < my_iter->mask.s_addr) || !result) {
		   		result = my_iter;
		   		best_match = my_iter->mask.s_addr;
		   	}
		}
		my_iter = my_iter->next;
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

    rt_walker->next = 0;
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
        struct sr_rt *next = NULL;
        if(sr->routing_table != NULL){
            next = current->next;
        }
        if(current->updated_time > 20){
            if(next != NULL){
                sr->routing_table = next;
            }
            else{
                sr->routing_table = NULL;
            }
        }
        while(current->next != NULL){
            if(next->updated_time > 20){
                if(next->next != NULL){
                    current->next = next->next;
                }
                else{
                    current->next = NULL;
                    break;
                }
            }
            current = current->next;
            if(current->next != NULL){
                next = current->next;
            }
        }
        
        pthread_mutex_unlock(&(sr->rt_lock));
    }
    return NULL;
}

void send_rip_request(struct sr_instance *sr){
    /* Fill your code here */
}

/* Call when you receive a RIP request packet or in the sr_rip_timeout function. You should enable split horizon here to prevent count-to-infinity problem */
void send_rip_update(struct sr_instance *sr){
        pthread_mutex_lock(&(sr->rt_lock));
        /* Send a RIP response to all neighbors */
	struct sr_rt *current = sr->routing_table;
	while (current != NULL) {
		/* Populate packet fields */
		unsigned int len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_udp_hdr_t) + sizeof(sr_rip_pkt_t);
   		uint8_t* buf = (uint8_t*)malloc(len);
   
		/* Ethernet Header */
   	 	sr_ethernet_hdr_t* eth_hdr = (sr_ethernet_hdr_t*)(buf);
    		eth_hdr->ether_type = htons(ethertype_ip);
    		memset(eth_hdr->ether_shost, 0x00, 6);
    		memset(eth_hdr->ether_dhost, 0x00, 6);

		/* IP Header */
    		sr_ip_hdr_t *new_ip_hdr = (sr_ip_hdr_t*)(buf + sizeof(sr_ethernet_hdr_t));
    		new_ip_hdr->ip_hl = 5;
    		new_ip_hdr->ip_v = 4;
    		/*new_ip_hdr->ip_tos = my_ip_hdr->ip_tos;*/
    		new_ip_hdr->ip_len = htons(len - sizeof(sr_ethernet_hdr_t));
    		/*new_ip_hdr->ip_id = my_ip_hdr->ip_id;*/
    		new_ip_hdr->ip_off = 0;
    		new_ip_hdr->ip_ttl = 64;
    		new_ip_hdr->ip_p = ip_protocol_udp;
    		new_ip_hdr->ip_sum = 0;
    		/*new_ip_hdr->ip_dst = my_ip_hdr->ip_src; */
    		struct sr_rt *my_match = sr_longest_prefix_match(sr, new_ip_hdr->ip_dst);
    		struct sr_if* interface = sr_get_interface(sr, my_match->interface);    
    		new_ip_hdr->ip_src = interface->ip;
    		new_ip_hdr->ip_sum = cksum(buf + sizeof(sr_ethernet_hdr_t), sizeof(sr_ip_hdr_t)); 
  
    	        /* UDP Header */
		sr_udp_hdr_t *udp_hdr = (sr_udp_hdr_t*)(buf + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
		udp_hdr->port_dst = 520;
		udp_hdr->port_dst = 520;
		udp_hdr-> udp_len = htons(len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));

		/* RIP Packet */
		sr_rip_pkt_t *rip_pkt = (sr_rip_pkt_t*)(buf + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_udp_hdr_t));
		rip_pkt->command = 2;
		rip_pkt->version = 2;
		struct entry *my_rip_entry = (struct entry*)(rip_pkt + 2*sizeof(uint8_t) + sizeof(uint16_t));
		
                               
    		/*sr_get_nexthop(sr, buf, len, my_match->gw.s_addr, interface);*/
    		free(buf);
		current = current->next;
	}
        pthread_mutex_unlock(&(sr->rt_lock));
}

/* Called after receiving a RIP response packet. You should enable triggered updates here. When the routing table changes, the router will send a RIP response immediately */
void update_route_table(struct sr_instance *sr, sr_ip_hdr_t* ip_packet, sr_rip_pkt_t* rip_packet, char* iface){
        pthread_mutex_lock(&(sr->rt_lock));		
	/* Get current distance to node who sent ip_packet */
	uint32_t my_ip_src = ip_packet->ip_src;
	uint32_t my_cost;
	uint32_t my_sr_dst_ip;
	struct sr_rt *current = sr->routing_table;
	struct in_addr my_sr_dest;
	struct in_addr my_sr_gw;
	struct in_addr my_sr_mask;
	uint32_t my_sr_metric;
	
	while (current != NULL) {
		my_sr_dest = current->dest;
		my_sr_dst_ip = my_sr_dest.s_addr;
    		my_sr_gw = current->gw;
    		my_sr_mask = current->mask;
		my_sr_metric = current->metric;
		if (my_sr_dst_ip == my_ip_src) {
			my_cost = my_sr_metric;
		}
		current = current->next;
	}
	/* Check if my_cost + sender's cost to dst is less than current cost */
	current = sr->routing_table;
	struct entry *current_entry;
	uint16_t my_rip_afi;
	uint16_t my_rip_tag;
	uint32_t my_rip_address;
	uint32_t my_rip_mask;
	uint32_t my_rip_next_hop;
	uint32_t my_rip_metric;
	
	while (current != NULL) {
		my_sr_dest = current->dest;
		my_sr_dst_ip = my_sr_dest.s_addr;
    		my_sr_gw = current->gw;
    		my_sr_mask = current->mask;
		my_sr_metric = current->metric;
		current_entry = (struct entry*)(rip_packet + 2*sizeof(uint8_t) + sizeof(uint16_t));
		while (current_entry != NULL) {
			my_rip_afi = current_entry->afi; /* Address Family Identifier */
      			my_rip_tag = current_entry->tag; /*Route Tag */
      			my_rip_address = current_entry->address; /* IP Address */
      			my_rip_mask = current_entry->mask; /* Subnet Mask */
     			my_rip_next_hop = current_entry->next_hop; /* Next Hop */
      			my_rip_metric = current_entry->metric; /* Metric */
			/* Check if shorter route available */
			if (my_sr_dst_ip == my_rip_address) {
				if (my_sr_metric > my_cost + my_rip_metric) {
					current->metric = my_rip_metric;					
					send_rip_update(sr);
				}
			}		
			current_entry = current_entry + sizeof(struct entry);
		}
		current = current->next;
	}
        pthread_mutex_unlock(&(sr->rt_lock));
}
