/* attr.c - backend routines for dealing with attributes */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2000-2008 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include "portable.h"

#include <stdio.h>

#include <ac/socket.h>
#include <ac/string.h>

#include "slap.h"
#include "back-bdb.h"
#include "lutil.h"

/* Find the ad, return -1 if not found,
 * set point for insertion if ins is non-NULL
 */
int
bdb_attr_slot( struct bdb_info *bdb, AttributeDescription *ad, unsigned *ins )
{
	unsigned base = 0, cursor = 0;
	unsigned n = bdb->bi_nattrs;
	int val = 0;
	
	while ( 0 < n ) {
		int pivot = n >> 1;
		cursor = base + pivot;

		val = SLAP_PTRCMP( ad, bdb->bi_attrs[cursor]->ai_desc );
		if ( val < 0 ) {
			n = pivot;
		} else if ( val > 0 ) {
			base = cursor + 1;
			n -= pivot + 1;
		} else {
			return cursor;
		}
	}
	if ( ins ) {
		if ( val > 0 )
			++cursor;
		*ins = cursor;
	}
	return -1;
}

static int
ainfo_insert( struct bdb_info *bdb, AttrInfo *a )
{
	unsigned x;
	int i = bdb_attr_slot( bdb, a->ai_desc, &x );

	/* Is it a dup? */
	if ( i >= 0 )
		return -1;

	bdb->bi_attrs = ch_realloc( bdb->bi_attrs, ( bdb->bi_nattrs+1 ) * 
		sizeof( AttrInfo * ));
	if ( x < bdb->bi_nattrs )
		AC_MEMCPY( &bdb->bi_attrs[x+1], &bdb->bi_attrs[x],
			( bdb->bi_nattrs - x ) * sizeof( AttrInfo *));
	bdb->bi_attrs[x] = a;
	bdb->bi_nattrs++;
	return 0;
}

AttrInfo *
bdb_attr_mask(
	struct bdb_info	*bdb,
	AttributeDescription *desc )
{
	int i = bdb_attr_slot( bdb, desc, NULL );
	return i < 0 ? NULL : bdb->bi_attrs[i];
}

int
bdb_attr_index_config(
	struct bdb_info	*bdb,
	const char		*fname,
	int			lineno,
	int			argc,
	char		**argv )
{
	int rc;
	int	i;
	slap_mask_t mask;
	char **attrs;
	char **indexes = NULL;

	attrs = ldap_str2charray( argv[0], "," );

	if( attrs == NULL ) {
		fprintf( stderr, "%s: line %d: "
			"no attributes specified: %s\n",
			fname, lineno, argv[0] );
		return LDAP_PARAM_ERROR;
	}

	if ( argc > 1 ) {
		indexes = ldap_str2charray( argv[1], "," );

		if( indexes == NULL ) {
			fprintf( stderr, "%s: line %d: "
				"no indexes specified: %s\n",
				fname, lineno, argv[1] );
			return LDAP_PARAM_ERROR;
		}
	}

	if( indexes == NULL ) {
		mask = bdb->bi_defaultmask;

	} else {
		mask = 0;

		for ( i = 0; indexes[i] != NULL; i++ ) {
			slap_mask_t index;
			rc = slap_str2index( indexes[i], &index );

			if( rc != LDAP_SUCCESS ) {
				fprintf( stderr, "%s: line %d: "
					"index type \"%s\" undefined\n",
					fname, lineno, indexes[i] );
				return LDAP_PARAM_ERROR;
			}

			mask |= index;
		}
	}

	if( !mask ) {
		fprintf( stderr, "%s: line %d: "
			"no indexes selected\n",
			fname, lineno );
		return LDAP_PARAM_ERROR;
	}

	for ( i = 0; attrs[i] != NULL; i++ ) {
		AttrInfo	*a;
		AttributeDescription *ad;
		const char *text;
#ifdef LDAP_COMP_MATCH
		ComponentReference* cr = NULL;
		AttrInfo *a_cr = NULL;
#endif

		if( strcasecmp( attrs[i], "default" ) == 0 ) {
			bdb->bi_defaultmask |= mask;
			continue;
		}

#ifdef LDAP_COMP_MATCH
		if ( is_component_reference( attrs[i] ) ) {
			rc = extract_component_reference( attrs[i], &cr );
			if ( rc != LDAP_SUCCESS ) {
				fprintf( stderr, "%s: line %d: "
					"index component reference\"%s\" undefined\n",
					fname, lineno, attrs[i] );
				return rc;
			}
			cr->cr_indexmask = mask;
			/*
			 * After extracting a component reference
			 * only the name of a attribute will be remaining
			 */
		} else {
			cr = NULL;
		}
#endif
		a = (AttrInfo *) ch_malloc( sizeof(AttrInfo) );

#ifdef LDAP_COMP_MATCH
		a->ai_cr = NULL;
#endif
		ad = NULL;
		rc = slap_str2ad( attrs[i], &ad, &text );

		if( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s: line %d: "
				"index attribute \"%s\" undefined\n",
				fname, lineno, attrs[i] );
			return rc;
		}

		if( slap_ad_is_binary( ad ) ) {
			fprintf( stderr, "%s: line %d: "
				"index of attribute \"%s\" disallowed\n",
				fname, lineno, attrs[i] );
			return LDAP_UNWILLING_TO_PERFORM;
		}

		if( IS_SLAP_INDEX( mask, SLAP_INDEX_APPROX ) && !(
			ad->ad_type->sat_approx
				&& ad->ad_type->sat_approx->smr_indexer
				&& ad->ad_type->sat_approx->smr_filter ) )
		{
			fprintf( stderr, "%s: line %d: "
				"approx index of attribute \"%s\" disallowed\n",
				fname, lineno, attrs[i] );
			return LDAP_INAPPROPRIATE_MATCHING;
		}

		if( IS_SLAP_INDEX( mask, SLAP_INDEX_EQUALITY ) && !(
			ad->ad_type->sat_equality
				&& ad->ad_type->sat_equality->smr_indexer
				&& ad->ad_type->sat_equality->smr_filter ) )
		{
			fprintf( stderr, "%s: line %d: "
				"equality index of attribute \"%s\" disallowed\n",
				fname, lineno, attrs[i] );
			return LDAP_INAPPROPRIATE_MATCHING;
		}

		if( IS_SLAP_INDEX( mask, SLAP_INDEX_SUBSTR ) && !(
			ad->ad_type->sat_substr
				&& ad->ad_type->sat_substr->smr_indexer
				&& ad->ad_type->sat_substr->smr_filter ) )
		{
			fprintf( stderr, "%s: line %d: "
				"substr index of attribute \"%s\" disallowed\n",
				fname, lineno, attrs[i] );
			return LDAP_INAPPROPRIATE_MATCHING;
		}

		Debug( LDAP_DEBUG_CONFIG, "index %s 0x%04lx\n",
			ad->ad_cname.bv_val, mask, 0 ); 

		a->ai_desc = ad;

		if ( bdb->bi_flags & BDB_IS_OPEN ) {
			a->ai_indexmask = 0;
			a->ai_newmask = mask;
		} else {
			a->ai_indexmask = mask;
			a->ai_newmask = 0;
		}

#ifdef LDAP_COMP_MATCH
		if ( cr ) {
			a_cr = bdb_attr_mask( bdb, ad );
			if ( a_cr ) {
				/*
				 * AttrInfo is already in AVL
				 * just add the extracted component reference
				 * in the AttrInfo
				 */
				rc = insert_component_reference( cr, &a_cr->ai_cr );
				if ( rc != LDAP_SUCCESS) {
					fprintf( stderr, " error during inserting component reference in %s ", attrs[i]);
					return LDAP_PARAM_ERROR;
				}
				continue;
			} else {
				rc = insert_component_reference( cr, &a->ai_cr );
				if ( rc != LDAP_SUCCESS) {
					fprintf( stderr, " error during inserting component reference in %s ", attrs[i]);
					return LDAP_PARAM_ERROR;
				}
			}
		}
#endif
		rc = ainfo_insert( bdb, a );
		if( rc ) {
			if ( bdb->bi_flags & BDB_IS_OPEN ) {
				AttrInfo *b = bdb_attr_mask( bdb, ad );
				/* If we were editing this attr, reset it */
				b->ai_indexmask &= ~BDB_INDEX_DELETING;
				/* If this is leftover from a previous add, commit it */
				if ( b->ai_newmask )
					b->ai_indexmask = b->ai_newmask;
				b->ai_newmask = a->ai_newmask;
				ch_free( a );
				continue;
			}
			fprintf( stderr, "%s: line %d: duplicate index definition "
				"for attr \"%s\"" SLAPD_CONF_UNKNOWN_IGNORED ".\n",
				fname, lineno, attrs[i] );

			return LDAP_PARAM_ERROR;
		}
	}

	ldap_charray_free( attrs );
	if ( indexes != NULL ) ldap_charray_free( indexes );

	return LDAP_SUCCESS;
}

static int
bdb_attr_index_unparser( void *v1, void *v2 )
{
	AttrInfo *ai = v1;
	BerVarray *bva = v2;
	struct berval bv;
	char *ptr;

	slap_index2bvlen( ai->ai_indexmask, &bv );
	if ( bv.bv_len ) {
		bv.bv_len += ai->ai_desc->ad_cname.bv_len + 1;
		ptr = ch_malloc( bv.bv_len+1 );
		bv.bv_val = lutil_strcopy( ptr, ai->ai_desc->ad_cname.bv_val );
		*bv.bv_val++ = ' ';
		slap_index2bv( ai->ai_indexmask, &bv );
		bv.bv_val = ptr;
		ber_bvarray_add( bva, &bv );
	}
	return 0;
}

static AttributeDescription addef = { NULL, NULL, BER_BVC("default") };
static AttrInfo aidef = { &addef };

void
bdb_attr_index_unparse( struct bdb_info *bdb, BerVarray *bva )
{
	int i;

	if ( bdb->bi_defaultmask ) {
		aidef.ai_indexmask = bdb->bi_defaultmask;
		bdb_attr_index_unparser( &aidef, bva );
	}
	for ( i=0; i<bdb->bi_nattrs; i++ )
		bdb_attr_index_unparser( bdb->bi_attrs[i], bva );
}

void
bdb_attr_info_free( AttrInfo *ai )
{
#ifdef LDAP_COMP_MATCH
	free( ai->ai_cr );
#endif
	free( ai );
}

void
bdb_attr_index_destroy( struct bdb_info *bdb )
{
	int i;

	for ( i=0; i<bdb->bi_nattrs; i++ ) 
		bdb_attr_info_free( bdb->bi_attrs[i] );

	free( bdb->bi_attrs );
}

void bdb_attr_index_free( struct bdb_info *bdb, AttributeDescription *ad )
{
	int i;

	i = bdb_attr_slot( bdb, ad, NULL );
	if ( i >= 0 ) {
		bdb_attr_info_free( bdb->bi_attrs[i] );
		bdb->bi_nattrs--;
		for (; i<bdb->bi_nattrs; i++)
			bdb->bi_attrs[i] = bdb->bi_attrs[i+1];
	}
}

void bdb_attr_flush( struct bdb_info *bdb )
{
	int i;

	for ( i=0; i<bdb->bi_nattrs; i++ ) {
		if ( bdb->bi_attrs[i]->ai_indexmask & BDB_INDEX_DELETING ) {
			int j;
			bdb_attr_info_free( bdb->bi_attrs[i] );
			bdb->bi_nattrs--;
			for (j=i; j<bdb->bi_nattrs; j++)
				bdb->bi_attrs[j] = bdb->bi_attrs[j+1];
			i--;
		}
	}
}
