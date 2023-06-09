/* extended.c - ldap backend extended routines */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2003-2008 The OpenLDAP Foundation.
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

#include "slap.h"
#include "back-ldap.h"
#include "lber_pvt.h"

typedef int (ldap_back_exop_f)( Operation *op, SlapReply *rs, ldapconn_t **lc );

static ldap_back_exop_f ldap_back_exop_passwd;
static ldap_back_exop_f ldap_back_exop_generic;

static struct exop {
	struct berval		oid;
	ldap_back_exop_f	*extended;
} exop_table[] = {
	{ BER_BVC(LDAP_EXOP_MODIFY_PASSWD),	ldap_back_exop_passwd },
	{ BER_BVNULL, NULL }
};

static int
ldap_back_extended_one( Operation *op, SlapReply *rs, ldap_back_exop_f exop )
{
	ldapinfo_t	*li = (ldapinfo_t *) op->o_bd->be_private;

	ldapconn_t	*lc = NULL;
	LDAPControl	**oldctrls = NULL;
	int		rc;

	/* FIXME: this needs to be called here, so it is
	 * called twice; maybe we could avoid the 
	 * ldap_back_dobind() call inside each extended()
	 * call ... */
	if ( !ldap_back_dobind( &lc, op, rs, LDAP_BACK_SENDERR ) ) {
		return -1;
	}

	oldctrls = op->o_ctrls;
	if ( ldap_back_proxy_authz_ctrl( &lc->lc_bound_ndn,
		li->li_version, &li->li_idassert, op, rs, &op->o_ctrls ) )
	{
		op->o_ctrls = oldctrls;
		send_ldap_extended( op, rs );
		rs->sr_text = NULL;
		/* otherwise frontend resends result */
		rc = rs->sr_err = SLAPD_ABANDON;
		goto done;
	}

	rc = exop( op, rs, &lc );

	if ( op->o_ctrls && op->o_ctrls != oldctrls ) {
		free( op->o_ctrls[ 0 ] );
		free( op->o_ctrls );
	}
	op->o_ctrls = oldctrls;

done:;
	if ( lc != NULL ) {
		ldap_back_release_conn( li, lc );
	}
			
	return rc;
}

int
ldap_back_extended(
		Operation	*op,
		SlapReply	*rs )
{
	int	i;

	for ( i = 0; exop_table[i].extended != NULL; i++ ) {
		if ( bvmatch( &exop_table[i].oid, &op->oq_extended.rs_reqoid ) )
		{
			return ldap_back_extended_one( op, rs, exop_table[i].extended );
		}
	}

	/* if we get here, the exop is known; the best that we can do
	 * is pass it thru as is */
	/* FIXME: maybe a list of OIDs to pass thru would be safer */
	return ldap_back_extended_one( op, rs, ldap_back_exop_generic );
}

static int
ldap_back_exop_passwd(
	Operation	*op,
	SlapReply	*rs,
	ldapconn_t	**lcp )
{
	ldapinfo_t	*li = (ldapinfo_t *) op->o_bd->be_private;

	ldapconn_t	*lc = *lcp;
	req_pwdexop_s	*qpw = &op->oq_pwdexop;
	LDAPMessage	*res;
	ber_int_t	msgid;
	int		rc, isproxy, freedn = 0;
	int		do_retry = 1;
	char		*text = NULL;
	struct berval	dn = op->o_req_dn,
			ndn = op->o_req_ndn;

	assert( lc != NULL );

	if ( BER_BVISNULL( &ndn ) && op->ore_reqdata != NULL ) {
		/* NOTE: most of this code is mutuated
		 * from slap_passwd_parse(); we can't call
		 * that function since now the request data
		 * has been destroyed by NULL-terminating
		 * the bervals.  Luckily enough, we only need
		 * the first berval... */

		ber_tag_t tag;
		ber_len_t len = -1;
		BerElementBuffer berbuf;
		BerElement *ber = (BerElement *)&berbuf;

		struct berval	tmpid = BER_BVNULL;

		if ( op->ore_reqdata->bv_len == 0 ) {
			return LDAP_PROTOCOL_ERROR;
		}

		/* ber_init2 uses reqdata directly, doesn't allocate new buffers */
		ber_init2( ber, op->ore_reqdata, 0 );

		tag = ber_scanf( ber, "{" /*}*/ );

		if ( tag == LBER_ERROR ) {
			return LDAP_PROTOCOL_ERROR;
		}

		tag = ber_peek_tag( ber, &len );
		if ( tag == LDAP_TAG_EXOP_MODIFY_PASSWD_ID ) {
			tag = ber_scanf( ber, "m", &tmpid );

			if ( tag == LBER_ERROR ) {
				return LDAP_PROTOCOL_ERROR;
			}
		}

		if ( !BER_BVISEMPTY( &tmpid ) ) {
			rs->sr_err = dnPrettyNormal( NULL, &tmpid, &dn,
				&ndn, op->o_tmpmemctx );
			if ( rs->sr_err != LDAP_SUCCESS ) {
				/* should have been successfully parsed earlier! */
				return rs->sr_err;
			}
			freedn = 1;

		} else {
			dn = op->o_dn;
			ndn = op->o_ndn;
		}
	}

	isproxy = ber_bvcmp( &ndn, &op->o_ndn );

	Debug( LDAP_DEBUG_ARGS, "==> ldap_back_exop_passwd(\"%s\")%s\n",
		dn.bv_val, isproxy ? " (proxy)" : "", 0 );

retry:
	rc = ldap_passwd( lc->lc_ld, isproxy ? &dn : NULL,
		qpw->rs_old.bv_val ? &qpw->rs_old : NULL,
		qpw->rs_new.bv_val ? &qpw->rs_new : NULL,
		op->o_ctrls, NULL, &msgid );

	if ( rc == LDAP_SUCCESS ) {
		if ( ldap_result( lc->lc_ld, msgid, LDAP_MSG_ALL, NULL, &res ) == -1 ) {
			ldap_get_option( lc->lc_ld, LDAP_OPT_ERROR_NUMBER, &rc );
			rs->sr_err = rc;

		} else {
			/* only touch when activity actually took place... */
			if ( li->li_idle_timeout && lc ) {
				lc->lc_time = op->o_time;
			}

			/* sigh. parse twice, because parse_passwd
			 * doesn't give us the err / match / msg info.
			 */
			rc = ldap_parse_result( lc->lc_ld, res, &rs->sr_err,
					(char **)&rs->sr_matched,
					&text,
					NULL, NULL, 0 );
#ifndef LDAP_NULL_IS_NULL
			if ( rs->sr_matched && rs->sr_matched[ 0 ] == '\0' ) {
				free( (char *)rs->sr_matched );
				rs->sr_matched = NULL;
			}
			if ( rs->sr_text && rs->sr_text[ 0 ] == '\0' ) {
				free( (char *)rs->sr_text );
				rs->sr_text = NULL;
			}
#endif /* LDAP_NULL_IS_NULL */

			if ( rc == LDAP_SUCCESS ) {
				if ( rs->sr_err == LDAP_SUCCESS ) {
					struct berval	newpw;

					/* this never happens because 
					 * the frontend	is generating 
					 * the new password, so when
					 * the passwd exop is proxied,
					 * it never delegates password
					 * generation to the remote server
					 */
					rc = ldap_parse_passwd( lc->lc_ld, res,
							&newpw );
					if ( rc == LDAP_SUCCESS &&
							!BER_BVISNULL( &newpw ) )
					{
						rs->sr_type = REP_EXTENDED;
						rs->sr_rspdata = slap_passwd_return( &newpw );
						free( newpw.bv_val );
					}

				} else {
					rc = rs->sr_err;
				}
			}
			ldap_msgfree( res );
		}
	}

	if ( rc != LDAP_SUCCESS ) {
		rs->sr_err = slap_map_api2result( rs );
		if ( rs->sr_err == LDAP_UNAVAILABLE && do_retry ) {
			do_retry = 0;
			if ( ldap_back_retry( &lc, op, rs, LDAP_BACK_SENDERR ) ) {
				goto retry;
			}
		}

		if ( LDAP_BACK_QUARANTINE( li ) ) {
			ldap_back_quarantine( op, rs );
		}

		if ( text ) rs->sr_text = text;
		send_ldap_extended( op, rs );
		/* otherwise frontend resends result */
		rc = rs->sr_err = SLAPD_ABANDON;

	} else if ( LDAP_BACK_QUARANTINE( li ) ) {
		ldap_back_quarantine( op, rs );
	}

	if ( freedn ) {
		op->o_tmpfree( dn.bv_val, op->o_tmpmemctx );
		op->o_tmpfree( ndn.bv_val, op->o_tmpmemctx );
	}

	/* these have to be freed anyway... */
	if ( rs->sr_matched ) {
		free( (char *)rs->sr_matched );
		rs->sr_matched = NULL;
	}

	if ( text ) {
		free( text );
		rs->sr_text = NULL;
	}

	/* in case, cleanup handler */
	if ( lc == NULL ) {
		*lcp = NULL;
	}

	return rc;
}

static int
ldap_back_exop_generic(
	Operation	*op,
	SlapReply	*rs,
	ldapconn_t	**lcp )
{
	ldapinfo_t	*li = (ldapinfo_t *) op->o_bd->be_private;

	ldapconn_t	*lc = *lcp;
	LDAPMessage	*res;
	ber_int_t	msgid;
	int		rc;
	int		do_retry = 1;
	char		*text = NULL;

	assert( lc != NULL );

	Debug( LDAP_DEBUG_ARGS, "==> ldap_back_exop_generic(%s, \"%s\")\n",
		op->ore_reqoid.bv_val, op->o_req_dn.bv_val, 0 );

retry:
	rc = ldap_extended_operation( lc->lc_ld,
		op->ore_reqoid.bv_val, op->ore_reqdata,
		op->o_ctrls, NULL, &msgid );

	if ( rc == LDAP_SUCCESS ) {
		if ( ldap_result( lc->lc_ld, msgid, LDAP_MSG_ALL, NULL, &res ) == -1 ) {
			ldap_get_option( lc->lc_ld, LDAP_OPT_ERROR_NUMBER, &rc );
			rs->sr_err = rc;

		} else {
			/* only touch when activity actually took place... */
			if ( li->li_idle_timeout && lc ) {
				lc->lc_time = op->o_time;
			}

			/* sigh. parse twice, because parse_passwd
			 * doesn't give us the err / match / msg info.
			 */
			rc = ldap_parse_result( lc->lc_ld, res, &rs->sr_err,
					(char **)&rs->sr_matched,
					&text,
					NULL, NULL, 0 );
#ifndef LDAP_NULL_IS_NULL
			if ( rs->sr_matched && rs->sr_matched[ 0 ] == '\0' ) {
				free( (char *)rs->sr_matched );
				rs->sr_matched = NULL;
			}
			if ( rs->sr_text && rs->sr_text[ 0 ] == '\0' ) {
				free( (char *)rs->sr_text );
				rs->sr_text = NULL;
			}
#endif /* LDAP_NULL_IS_NULL */
			if ( rc == LDAP_SUCCESS ) {
				if ( rs->sr_err == LDAP_SUCCESS ) {
					rc = ldap_parse_extended_result( lc->lc_ld, res,
							(char **)&rs->sr_rspoid, &rs->sr_rspdata, 0 );
					if ( rc == LDAP_SUCCESS ) {
						rs->sr_type = REP_EXTENDED;
					}

				} else {
					rc = rs->sr_err;
				}
			}
			ldap_msgfree( res );
		}
	}

	if ( rc != LDAP_SUCCESS ) {
		rs->sr_err = slap_map_api2result( rs );
		if ( rs->sr_err == LDAP_UNAVAILABLE && do_retry ) {
			do_retry = 0;
			if ( ldap_back_retry( &lc, op, rs, LDAP_BACK_SENDERR ) ) {
				goto retry;
			}
		}

		if ( LDAP_BACK_QUARANTINE( li ) ) {
			ldap_back_quarantine( op, rs );
		}

		if ( text ) rs->sr_text = text;
		send_ldap_extended( op, rs );
		/* otherwise frontend resends result */
		rc = rs->sr_err = SLAPD_ABANDON;

	} else if ( LDAP_BACK_QUARANTINE( li ) ) {
		ldap_back_quarantine( op, rs );
	}

	/* these have to be freed anyway... */
	if ( rs->sr_matched ) {
		free( (char *)rs->sr_matched );
		rs->sr_matched = NULL;
	}

	if ( text ) {
		free( text );
		rs->sr_text = NULL;
	}

	/* in case, cleanup handler */
	if ( lc == NULL ) {
		*lcp = NULL;
	}

	return rc;
}

