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
        while(current->next != NULL){
            if(current->updated_time > 20){
                current->metric = 16;
            }
            current = current->next;
        }
        ////ALTERNATE METHOD
        // struct sr_rt *current = NULL;
        // if(sr->routing_table != NULL){
        //     current = sr->routing_table;
        // }
        // struct sr_rt *next = NULL;
        // if(sr->routing_table != NULL){
        //     next = current->next;
        // }
        // if(current->updated_time > 20){
        //     if(next != NULL){
        //         sr->routing_table = next;
        //     }
        //     else{
        //         sr->routing_table = NULL;
        //     }
        // }
        // while(current->next != NULL){
        //     if(next->updated_time > 20){
        //         if(next->next != NULL){
        //             current->next = next->next;
        //         }
        //         else{
        //             current->next = NULL;
        //             break;
        //         }
        //     }
        //     current = current->next;
        //     if(current->next != NULL){
        //         next = current->next;
        //     }
        // }
        
        pthread_mutex_unlock(&(sr->rt_lock));
    }
    return NULL;
}

void send_rip_request(struct sr_instance *sr){
    /* Fill your code here */
    uint8_t ffff[6] = {255,255,255,255,255,255};
    uint8_t tstf[6] = {0,0,0,0,0,0};
    len = 0;
    struct sr_rt *current = NULL;
    if(sr->routing_table != NULL){
        current = sr->routing_table;
    }
    while(current->next != NULL){
        len = len+1;
        current = current->next;
    }
    uint8_t *buf = malloc(sizeof(sr_rip_pkt_t)+sizeof(sr_ip_hdr_t)+sizeof(sr_udp_hdr_t));
    sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *)buf;
    sr_udp_hdr_t *udp_header = (sr_udp_hdr_t *)(buf+sizeof(sr_ip_hdr_t));
    sr_rip_pkt_t *rip_packet = (sr_rip_pkt_t *)(buf+sizeof(sr_ip_hdr_t)+sizeof(sr_udp_hdr_t))
    // POPULATE IP HEADER
    uint32_t chsum = cksum(ip_hdr, ip_hdr_size);
    ip_hdr->ip_sum = chsum;
    memcpy(ip_hdr->ip_dst, ffff, 6);
    ip_hdr->ip_src = NULL; //FIX THIS LATER
    ip_hdr->ip_ttl = 255;
    ip_hdr->ip_p = ip_protocol_udp;
    ip_hdr->ip_len = sizeof(sr_ip_hdr_t);
    // POPULATE UDP HEADER
    udp_header->port_src = 520;
    udp_header->port_dst = 520;
    udp_header->udp_len = sizeof(sr_rip_pkt_t)+sizeof(sr_ip_hdr_t)+sizeof(sr_udp_hdr_t); //CHECK
    udp_header->udp_sum = cksum(udp_header, sizeof(udp_header));
    //POPULATE RIP PACKETS
    int cur = 0;
    rip_packet->command = 1;
    rip_packet->version = 2;
    rip_packet->unused = 0;
    if(sr->routing_table != NULL){
        current = sr->routing_table;
    }
    while(cur<len){
        rip_packet->entries[cur]->afi = 2;
        rip_packet->entries[cur]->tag = 13;
        rip_packet->entries[cur]->address = current->dest;
        rip_packet->entries[cur]->mask = current->mask;
        rip_packet->entries[cur]->next_hop = current->gw; //ENSURE THAT GW IS CORRECT
        rip_packet->entries[cur]->metric = current->metric;
        cur = cur+1;
        if(current->next != NULL){
            current = current->next;
        }
    }
}

void send_rip_update(struct sr_instance *sr){
    pthread_mutex_lock(&(sr->rt_lock));
    /* Fill your code here */

    pthread_mutex_unlock(&(sr->rt_lock));
}

void update_route_table(struct sr_instance *sr, sr_ip_hdr_t* ip_packet ,sr_rip_pkt_t* rip_packet, char* iface){
    pthread_mutex_lock(&(sr->rt_lock));
    /* Fill your code here */
    
    pthread_mutex_unlock(&(sr->rt_lock));
}