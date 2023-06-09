/* init.c - initialize ldap backend */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2003-2008 The OpenLDAP Foundation.
 * Portions Copyright 1999-2003 Howard Chu.
 * Portions Copyright 2000-2003 Pierangelo Masarati.
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
/* ACKNOWLEDGEMENTS:
 * This work was initially developed by the Howard Chu for inclusion
 * in OpenLDAP Software and subsequently enhanced by Pierangelo
 * Masarati.
 */

#include "portable.h"

#include <stdio.h>

#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"
#include "back-ldap.h"

int
ldap_back_open( BackendInfo	*bi )
{
	bi->bi_controls = slap_known_controls;
	return 0;
}

int
ldap_back_initialize( BackendInfo *bi )
{
	bi->bi_open = ldap_back_open;
	bi->bi_config = 0;
	bi->bi_close = 0;
	bi->bi_destroy = 0;

	bi->bi_db_init = ldap_back_db_init;
	bi->bi_db_config = config_generic_wrapper;
	bi->bi_db_open = ldap_back_db_open;
	bi->bi_db_close = 0;
	bi->bi_db_destroy = ldap_back_db_destroy;

	bi->bi_op_bind = ldap_back_bind;
	bi->bi_op_unbind = 0;
	bi->bi_op_search = ldap_back_search;
	bi->bi_op_compare = ldap_back_compare;
	bi->bi_op_modify = ldap_back_modify;
	bi->bi_op_modrdn = ldap_back_modrdn;
	bi->bi_op_add = ldap_back_add;
	bi->bi_op_delete = ldap_back_delete;
	bi->bi_op_abandon = 0;

	bi->bi_extended = ldap_back_extended;

	bi->bi_chk_referrals = 0;
	bi->bi_entry_get_rw = ldap_back_entry_get;

	bi->bi_connection_init = 0;
	bi->bi_connection_destroy = ldap_back_conn_destroy;

	if ( chain_init() ) {
		return -1;
	}

	return ldap_back_init_cf( bi );
}

int
ldap_back_db_init( Backend *be )
{
	ldapinfo_t	*li;
	unsigned	i;

	li = (ldapinfo_t *)ch_calloc( 1, sizeof( ldapinfo_t ) );
	if ( li == NULL ) {
 		return -1;
 	}

	li->li_rebind_f = ldap_back_default_rebind;
	ldap_pvt_thread_mutex_init( &li->li_uri_mutex );

	BER_BVZERO( &li->li_acl_authcID );
	BER_BVZERO( &li->li_acl_authcDN );
	BER_BVZERO( &li->li_acl_passwd );

	li->li_acl_authmethod = LDAP_AUTH_NONE;
	BER_BVZERO( &li->li_acl_sasl_mech );
	li->li_acl.sb_tls = SB_TLS_DEFAULT;

	li->li_idassert_mode = LDAP_BACK_IDASSERT_LEGACY;

	BER_BVZERO( &li->li_idassert_authcID );
	BER_BVZERO( &li->li_idassert_authcDN );
	BER_BVZERO( &li->li_idassert_passwd );

	BER_BVZERO( &li->li_idassert_authzID );

	li->li_idassert_authmethod = LDAP_AUTH_NONE;
	BER_BVZERO( &li->li_idassert_sasl_mech );
	li->li_idassert_tls = SB_TLS_DEFAULT;

	/* by default, use proxyAuthz control on each operation */
	li->li_idassert_flags = LDAP_BACK_AUTH_PRESCRIPTIVE;

	li->li_idassert_authz = NULL;

	/* initialize flags */
	li->li_flags = LDAP_BACK_F_CHASE_REFERRALS;

	/* initialize version */
	li->li_version = LDAP_VERSION3;

	ldap_pvt_thread_mutex_init( &li->li_conninfo.lai_mutex );

	for ( i = LDAP_BACK_PCONN_FIRST; i < LDAP_BACK_PCONN_LAST; i++ ) {
		li->li_conn_priv[ i ].lic_num = 0;
		LDAP_TAILQ_INIT( &li->li_conn_priv[ i ].lic_priv );
	}
	li->li_conn_priv_max = LDAP_BACK_CONN_PRIV_DEFAULT;

	be->be_private = li;
	SLAP_DBFLAGS( be ) |= SLAP_DBFLAG_NOLASTMOD;

	be->be_cf_ocs = be->bd_info->bi_cf_ocs;

	return 0;
}

int
ldap_back_db_open( BackendDB *be )
{
	ldapinfo_t	*li = (ldapinfo_t *)be->be_private;

	Debug( LDAP_DEBUG_TRACE,
		"ldap_back_db_open: URI=%s\n",
		li->li_uri != NULL ? li->li_uri : "", 0, 0 );

	/* by default, use proxyAuthz control on each operation */
	switch ( li->li_idassert_mode ) {
	case LDAP_BACK_IDASSERT_LEGACY:
	case LDAP_BACK_IDASSERT_SELF:
		/* however, since admin connections are pooled and shared,
		 * only static authzIDs can be native */
		li->li_idassert_flags &= ~LDAP_BACK_AUTH_NATIVE_AUTHZ;
		break;

	default:
		break;
	}

	if ( LDAP_BACK_T_F_DISCOVER( li ) && !LDAP_BACK_T_F( li ) ) {
		int		rc;

		rc = slap_discover_feature( li->li_uri, li->li_version,
				slap_schema.si_ad_supportedFeatures->ad_cname.bv_val,
				LDAP_FEATURE_ABSOLUTE_FILTERS );
		if ( rc == LDAP_COMPARE_TRUE ) {
			li->li_flags |= LDAP_BACK_F_T_F;
		}
	}

	if ( LDAP_BACK_CANCEL_DISCOVER( li ) && !LDAP_BACK_CANCEL( li ) ) {
		int		rc;

		rc = slap_discover_feature( li->li_uri, li->li_version,
				slap_schema.si_ad_supportedExtension->ad_cname.bv_val,
				LDAP_EXOP_CANCEL );
		if ( rc == LDAP_COMPARE_TRUE ) {
			li->li_flags |= LDAP_BACK_F_CANCEL_EXOP;
		}
	}

	li->li_flags |= LDAP_BACK_F_ISOPEN;

	return 0;
}

void
ldap_back_conn_free( void *v_lc )
{
	ldapconn_t	*lc = v_lc;

	if ( lc->lc_ld != NULL ) {	
		ldap_unbind_ext( lc->lc_ld, NULL, NULL );
	}
	if ( !BER_BVISNULL( &lc->lc_bound_ndn ) ) {
		ch_free( lc->lc_bound_ndn.bv_val );
	}
	if ( !BER_BVISNULL( &lc->lc_cred ) ) {
		memset( lc->lc_cred.bv_val, 0, lc->lc_cred.bv_len );
		ch_free( lc->lc_cred.bv_val );
	}
	if ( !BER_BVISNULL( &lc->lc_local_ndn ) ) {
		ch_free( lc->lc_local_ndn.bv_val );
	}
	lc->lc_q.tqe_prev = NULL;
	lc->lc_q.tqe_next = NULL;
	ch_free( lc );
}

int
ldap_back_db_destroy( Backend *be )
{
	if ( be->be_private ) {
		ldapinfo_t	*li = ( ldapinfo_t * )be->be_private;
		unsigned	i;

		ldap_pvt_thread_mutex_lock( &li->li_conninfo.lai_mutex );

		if ( li->li_uri != NULL ) {
			ch_free( li->li_uri );
			li->li_uri = NULL;

			assert( li->li_bvuri != NULL );
			ber_bvarray_free( li->li_bvuri );
			li->li_bvuri = NULL;
		}
		if ( !BER_BVISNULL( &li->li_acl_authcID ) ) {
			ch_free( li->li_acl_authcID.bv_val );
			BER_BVZERO( &li->li_acl_authcID );
		}
		if ( !BER_BVISNULL( &li->li_acl_authcDN ) ) {
			ch_free( li->li_acl_authcDN.bv_val );
			BER_BVZERO( &li->li_acl_authcDN );
		}
		if ( !BER_BVISNULL( &li->li_acl_passwd ) ) {
			ch_free( li->li_acl_passwd.bv_val );
			BER_BVZERO( &li->li_acl_passwd );
		}
		if ( !BER_BVISNULL( &li->li_acl_sasl_mech ) ) {
			ch_free( li->li_acl_sasl_mech.bv_val );
			BER_BVZERO( &li->li_acl_sasl_mech );
		}
		if ( !BER_BVISNULL( &li->li_acl_sasl_realm ) ) {
			ch_free( li->li_acl_sasl_realm.bv_val );
			BER_BVZERO( &li->li_acl_sasl_realm );
		}
		if ( !BER_BVISNULL( &li->li_idassert_authcID ) ) {
			ch_free( li->li_idassert_authcID.bv_val );
			BER_BVZERO( &li->li_idassert_authcID );
		}
		if ( !BER_BVISNULL( &li->li_idassert_authcDN ) ) {
			ch_free( li->li_idassert_authcDN.bv_val );
			BER_BVZERO( &li->li_idassert_authcDN );
		}
		if ( !BER_BVISNULL( &li->li_idassert_passwd ) ) {
			ch_free( li->li_idassert_passwd.bv_val );
			BER_BVZERO( &li->li_idassert_passwd );
		}
		if ( !BER_BVISNULL( &li->li_idassert_authzID ) ) {
			ch_free( li->li_idassert_authzID.bv_val );
			BER_BVZERO( &li->li_idassert_authzID );
		}
		if ( !BER_BVISNULL( &li->li_idassert_sasl_mech ) ) {
			ch_free( li->li_idassert_sasl_mech.bv_val );
			BER_BVZERO( &li->li_idassert_sasl_mech );
		}
		if ( !BER_BVISNULL( &li->li_idassert_sasl_realm ) ) {
			ch_free( li->li_idassert_sasl_realm.bv_val );
			BER_BVZERO( &li->li_idassert_sasl_realm );
		}
		if ( li->li_idassert_authz != NULL ) {
			ber_bvarray_free( li->li_idassert_authz );
			li->li_idassert_authz = NULL;
		}
               	if ( li->li_conninfo.lai_tree ) {
			avl_free( li->li_conninfo.lai_tree, ldap_back_conn_free );
		}
		for ( i = LDAP_BACK_PCONN_FIRST; i < LDAP_BACK_PCONN_LAST; i++ ) {
			while ( !LDAP_TAILQ_EMPTY( &li->li_conn_priv[ i ].lic_priv ) ) {
				ldapconn_t	*lc = LDAP_TAILQ_FIRST( &li->li_conn_priv[ i ].lic_priv );

				LDAP_TAILQ_REMOVE( &li->li_conn_priv[ i ].lic_priv, lc, lc_q );
				ldap_back_conn_free( lc );
			}
		}
		if ( LDAP_BACK_QUARANTINE( li ) ) {
			slap_retry_info_destroy( &li->li_quarantine );
			ldap_pvt_thread_mutex_destroy( &li->li_quarantine_mutex );
		}

		ldap_pvt_thread_mutex_unlock( &li->li_conninfo.lai_mutex );
		ldap_pvt_thread_mutex_destroy( &li->li_conninfo.lai_mutex );
		ldap_pvt_thread_mutex_destroy( &li->li_uri_mutex );
	}

	ch_free( be->be_private );

	return 0;
}

#if SLAPD_LDAP == SLAPD_MOD_DYNAMIC

/* conditionally define the init_module() function */
SLAP_BACKEND_INIT_MODULE( ldap )

#endif /* SLAPD_LDAP == SLAPD_MOD_DYNAMIC */

