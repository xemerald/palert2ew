/*
 * dl_chain_list.h
 *
 * Header file for double-link chain list.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * March, 2019
 *
 */

#pragma once

/**/
typedef struct dl_node {
	void           *data;
	struct dl_node *prev;
	struct dl_node *next;
} DL_NODE;

#define DL_NODE_GET_DATA(NODE)  ((NODE)->data)
#define DL_NODE_GET_NEXT(NODE)  ((NODE)->next)
#define DL_NODE_GET_PREV(NODE)  ((NODE)->prev)

/* Export functions' prototypes */
DL_NODE *dl_node_append( DL_NODE **, const void * );
DL_NODE *dl_node_insert( DL_NODE *, const void * );
DL_NODE *dl_node_push( DL_NODE **, const void * );
DL_NODE *dl_node_pop( DL_NODE ** );
DL_NODE *dl_node_delete( DL_NODE *, void (*)( void * ) );
void    *dl_node_data_extract( DL_NODE * );
void     dl_list_destroy( DL_NODE **, void (*)( void * ) );