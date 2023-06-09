/* config.c - bdb backend configuration file routine */
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
#include <ac/ctype.h>
#include <ac/string.h>

#include "back-bdb.h"

#include "config.h"

#include "lutil.h"
#include "ldap_rq.h"

#ifdef DB_DIRTY_READ
#	define	SLAP_BDB_ALLOW_DIRTY_READ
#endif

#define bdb_cf_gen			BDB_SYMBOL(cf_gen)
#define	bdb_cf_cleanup		BDB_SYMBOL(cf_cleanup)
#define bdb_checkpoint		BDB_SYMBOL(checkpoint)
#define bdb_online_index	BDB_SYMBOL(online_index)

static ConfigDriver bdb_cf_gen;

enum {
	BDB_CHKPT = 1,
	BDB_CONFIG,
	BDB_DIRECTORY,
	BDB_NOSYNC,
	BDB_DIRTYR,
	BDB_INDEX,
	BDB_LOCKD,
	BDB_SSTACK
};

static ConfigTable bdbcfg[] = {
	{ "directory", "dir", 2, 2, 0, ARG_STRING|ARG_MAGIC|BDB_DIRECTORY,
		bdb_cf_gen, "( OLcfgDbAt:0.1 NAME 'olcDbDirectory' "
			"DESC 'Directory for database content' "
			"EQUALITY caseIgnoreMatch "
			"SYNTAX OMsDirectoryString SINGLE-VALUE )", NULL, NULL },
	{ "cachefree", "size", 2, 2, 0, ARG_INT|ARG_OFFSET,
		(void *)offsetof(struct bdb_info, bi_cache.c_minfree),
		"( OLcfgDbAt:1.11 NAME 'olcDbCacheFree' "
			"DESC 'Number of extra entries to free when max is reached' "
			"SYNTAX OMsInteger SINGLE-VALUE )", NULL, NULL },
	{ "cachesize", "size", 2, 2, 0, ARG_INT|ARG_OFFSET,
		(void *)offsetof(struct bdb_info, bi_cache.c_maxsize),
		"( OLcfgDbAt:1.1 NAME 'olcDbCacheSize' "
			"DESC 'Entry cache size in entries' "
			"SYNTAX OMsInteger SINGLE-VALUE )", NULL, NULL },
	{ "checkpoint", "kbyte> <min", 3, 3, 0, ARG_MAGIC|BDB_CHKPT,
		bdb_cf_gen, "( OLcfgDbAt:1.2 NAME 'olcDbCheckpoint' "
			"DESC 'Database checkpoint interval in kbytes and minutes' "
			"SYNTAX OMsDirectoryString SINGLE-VALUE )",NULL, NULL },
	{ "dbconfig", "DB_CONFIG setting", 1, 0, 0, ARG_MAGIC|BDB_CONFIG,
		bdb_cf_gen, "( OLcfgDbAt:1.3 NAME 'olcDbConfig' "
			"DESC 'BerkeleyDB DB_CONFIG configuration directives' "
			"SYNTAX OMsDirectoryString X-ORDERED 'VALUES' )", NULL, NULL },
	{ "dbnosync", NULL, 1, 2, 0, ARG_ON_OFF|ARG_MAGIC|BDB_NOSYNC,
		bdb_cf_gen, "( OLcfgDbAt:1.4 NAME 'olcDbNoSync' "
			"DESC 'Disable synchronous database writes' "
			"SYNTAX OMsBoolean SINGLE-VALUE )", NULL, NULL },
	{ "dirtyread", NULL, 1, 2, 0,
#ifdef SLAP_BDB_ALLOW_DIRTY_READ
		ARG_ON_OFF|ARG_MAGIC|BDB_DIRTYR, bdb_cf_gen,
#else
		ARG_IGNORED, NULL,
#endif
		"( OLcfgDbAt:1.5 NAME 'olcDbDirtyRead' "
		"DESC 'Allow reads of uncommitted data' "
		"SYNTAX OMsBoolean SINGLE-VALUE )", NULL, NULL },
	{ "idlcachesize", "size", 2, 2, 0, ARG_INT|ARG_OFFSET,
		(void *)offsetof(struct bdb_info,bi_idl_cache_max_size),
		"( OLcfgDbAt:1.6 NAME 'olcDbIDLcacheSize' "
		"DESC 'IDL cache size in IDLs' "
		"SYNTAX OMsInteger SINGLE-VALUE )", NULL, NULL },
	{ "index", "attr> <[pres,eq,approx,sub]", 2, 3, 0, ARG_MAGIC|BDB_INDEX,
		bdb_cf_gen, "( OLcfgDbAt:0.2 NAME 'olcDbIndex' "
		"DESC 'Attribute index parameters' "
		"EQUALITY caseIgnoreMatch "
		"SYNTAX OMsDirectoryString )", NULL, NULL },
	{ "linearindex", NULL, 1, 2, 0, ARG_ON_OFF|ARG_OFFSET,
		(void *)offsetof(struct bdb_info, bi_linear_index), 
		"( OLcfgDbAt:1.7 NAME 'olcDbLinearIndex' "
		"DESC 'Index attributes one at a time' "
		"SYNTAX OMsBoolean SINGLE-VALUE )", NULL, NULL },
	{ "lockdetect", "policy", 2, 2, 0, ARG_MAGIC|BDB_LOCKD,
		bdb_cf_gen, "( OLcfgDbAt:1.8 NAME 'olcDbLockDetect' "
		"DESC 'Deadlock detection algorithm' "
		"SYNTAX OMsDirectoryString SINGLE-VALUE )", NULL, NULL },
	{ "mode", "mode", 2, 2, 0, ARG_INT|ARG_OFFSET,
		(void *)offsetof(struct bdb_info, bi_dbenv_mode),
		"( OLcfgDbAt:0.3 NAME 'olcDbMode' "
		"DESC 'Unix permissions of database files' "
		"SYNTAX OMsInteger SINGLE-VALUE )", NULL, NULL },
	{ "searchstack", "depth", 2, 2, 0, ARG_INT|ARG_MAGIC|BDB_SSTACK,
		bdb_cf_gen, "( OLcfgDbAt:1.9 NAME 'olcDbSearchStack' "
		"DESC 'Depth of search stack in IDLs' "
		"SYNTAX OMsInteger SINGLE-VALUE )", NULL, NULL },
	{ "shm_key", "key", 2, 2, 0, ARG_INT|ARG_OFFSET,
		(void *)offsetof(struct bdb_info, bi_shm_key), 
		"( OLcfgDbAt:1.10 NAME 'olcDbShmKey' "
		"DESC 'Key for shared memory region' "
		"SYNTAX OMsInteger SINGLE-VALUE )", NULL, NULL },
	{ NULL, NULL, 0, 0, 0, ARG_IGNORED,
		NULL, NULL, NULL, NULL }
};

static ConfigOCs bdbocs[] = {
	{
#ifdef BDB_HIER
		"( OLcfgDbOc:1.2 "
		"NAME 'olcHdbConfig' "
		"DESC 'HDB backend configuration' "
#else
		"( OLcfgDbOc:1.1 "
		"NAME 'olcBdbConfig' "
		"DESC 'BDB backend configuration' "
#endif
		"SUP olcDatabaseConfig "
		"MUST olcDbDirectory "
		"MAY ( olcDbCacheSize $ olcDbCheckpoint $ olcDbConfig $ "
		"olcDbNoSync $ olcDbDirtyRead $ olcDbIDLcacheSize $ "
		"olcDbIndex $ olcDbLinearIndex $ olcDbLockDetect $ "
		"olcDbMode $ olcDbSearchStack $ olcDbShmKey $ "
		" olcDbCacheFree ) )",
		 	Cft_Database, bdbcfg },
	{ NULL, 0, NULL }
};

static slap_verbmasks bdb_lockd[] = {
	{ BER_BVC("default"), DB_LOCK_DEFAULT },
	{ BER_BVC("oldest"), DB_LOCK_OLDEST },
	{ BER_BVC("random"), DB_LOCK_RANDOM },
	{ BER_BVC("youngest"), DB_LOCK_YOUNGEST },
	{ BER_BVC("fewest"), DB_LOCK_MINLOCKS },
	{ BER_BVNULL, 0 }
};

/* perform periodic checkpoints */
static void *
bdb_checkpoint( void *ctx, void *arg )
{
	struct re_s *rtask = arg;
	struct bdb_info *bdb = rtask->arg;
	
	TXN_CHECKPOINT( bdb->bi_dbenv, bdb->bi_txn_cp_kbyte,
		bdb->bi_txn_cp_min, 0 );
	ldap_pvt_thread_mutex_lock( &slapd_rq.rq_mutex );
	ldap_pvt_runqueue_stoptask( &slapd_rq, rtask );
	ldap_pvt_thread_mutex_unlock( &slapd_rq.rq_mutex );
	return NULL;
}

/* reindex entries on the fly */
static void *
bdb_online_index( void *ctx, void *arg )
{
	struct re_s *rtask = arg;
	BackendDB *be = rtask->arg;
	struct bdb_info *bdb = be->be_private;

	Connection conn = {0};
	OperationBuffer opbuf;
	Operation *op = (Operation *) &opbuf;

	DBC *curs;
	DBT key, data;
	DB_TXN *txn;
	DB_LOCK lock;
	u_int32_t locker;
	ID id, nid;
	EntryInfo *ei;
	int rc, getnext = 1;
	int i;

	connection_fake_init( &conn, op, ctx );

	op->o_bd = be;

	DBTzero( &key );
	DBTzero( &data );
	
	id = 1;
	key.data = &nid;
	key.size = key.ulen = sizeof(ID);
	key.flags = DB_DBT_USERMEM;

	data.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
	data.dlen = data.ulen = 0;

	while ( 1 ) {
		if ( slapd_shutdown )
			break;

		rc = TXN_BEGIN( bdb->bi_dbenv, NULL, &txn, bdb->bi_db_opflags );
		if ( rc ) 
			break;
		locker = TXN_ID( txn );
		if ( getnext ) {
			getnext = 0;
			BDB_ID2DISK( id, &nid );
			rc = bdb->bi_id2entry->bdi_db->cursor(
				bdb->bi_id2entry->bdi_db, txn, &curs, bdb->bi_db_opflags );
			if ( rc ) {
				TXN_ABORT( txn );
				break;
			}
			rc = curs->c_get( curs, &key, &data, DB_SET_RANGE );
			curs->c_close( curs );
			if ( rc ) {
				TXN_ABORT( txn );
				if ( rc == DB_NOTFOUND )
					rc = 0;
				if ( rc == DB_LOCK_DEADLOCK ) {
					ldap_pvt_thread_yield();
					continue;
				}
				break;
			}
			BDB_DISK2ID( &nid, &id );
		}

		ei = NULL;
		rc = bdb_cache_find_id( op, txn, id, &ei, 0, locker, &lock );
		if ( rc ) {
			TXN_ABORT( txn );
			if ( rc == DB_LOCK_DEADLOCK ) {
				ldap_pvt_thread_yield();
				continue;
			}
			if ( rc == DB_NOTFOUND ) {
				id++;
				getnext = 1;
				continue;
			}
			break;
		}
		if ( ei->bei_e ) {
			rc = bdb_index_entry( op, txn, BDB_INDEX_UPDATE_OP, ei->bei_e );
			if ( rc == DB_LOCK_DEADLOCK ) {
				TXN_ABORT( txn );
				ldap_pvt_thread_yield();
				continue;
			}
			if ( rc == 0 ) {
				rc = TXN_COMMIT( txn, 0 );
				txn = NULL;
			}
			if ( rc )
				break;
		}
		id++;
		getnext = 1;
	}

	for ( i = 0; i < bdb->bi_nattrs; i++ ) {
		if ( bdb->bi_attrs[ i ]->ai_indexmask & BDB_INDEX_DELETING
			|| bdb->bi_attrs[ i ]->ai_newmask == 0 )
		{
			continue;
		}
		bdb->bi_attrs[ i ]->ai_indexmask = bdb->bi_attrs[ i ]->ai_newmask;
		bdb->bi_attrs[ i ]->ai_newmask = 0;
	}

	ldap_pvt_thread_mutex_lock( &slapd_rq.rq_mutex );
	ldap_pvt_runqueue_stoptask( &slapd_rq, rtask );
	bdb->bi_index_task = NULL;
	ldap_pvt_runqueue_remove( &slapd_rq, rtask );
	ldap_pvt_thread_mutex_unlock( &slapd_rq.rq_mutex );

	return NULL;
}

/* Cleanup loose ends after Modify completes */
static int
bdb_cf_cleanup( ConfigArgs *c )
{
	struct bdb_info *bdb = c->be->be_private;
	int rc = 0;

	if ( bdb->bi_flags & BDB_UPD_CONFIG ) {
		if ( bdb->bi_db_config ) {
			int i;
			FILE *f = fopen( bdb->bi_db_config_path, "w" );
			if ( f ) {
				for (i=0; bdb->bi_db_config[i].bv_val; i++)
					fprintf( f, "%s\n", bdb->bi_db_config[i].bv_val );
				fclose( f );
			}
		} else {
			unlink( bdb->bi_db_config_path );
		}
		bdb->bi_flags ^= BDB_UPD_CONFIG;
	}

	if ( bdb->bi_flags & BDB_DEL_INDEX ) {
		bdb_attr_flush( bdb );
		bdb->bi_flags ^= BDB_DEL_INDEX;
	}
	
	if ( bdb->bi_flags & BDB_RE_OPEN ) {
		bdb->bi_flags ^= BDB_RE_OPEN;
		rc = c->be->bd_info->bi_db_close( c->be );
		if ( rc == 0 )
			rc = c->be->bd_info->bi_db_open( c->be );
		/* If this fails, we need to restart */
		if ( rc ) {
			slapd_shutdown = 2;
			Debug( LDAP_DEBUG_ANY, LDAP_XSTRING(bdb_cf_cleanup)
				": failed to reopen database, rc=%d", rc, 0, 0 );
		}
	}
	return rc;
}

static int
bdb_cf_gen(ConfigArgs *c)
{
	struct bdb_info *bdb = c->be->be_private;
	int rc;

	if ( c->op == SLAP_CONFIG_EMIT ) {
		rc = 0;
		switch( c->type ) {
		case BDB_CHKPT:
			if (bdb->bi_txn_cp ) {
				char buf[64];
				struct berval bv;
				bv.bv_len = sprintf( buf, "%d %d", bdb->bi_txn_cp_kbyte,
					bdb->bi_txn_cp_min );
				bv.bv_val = buf;
				value_add_one( &c->rvalue_vals, &bv );
			} else{
				rc = 1;
			}
			break;

		case BDB_DIRECTORY:
			if ( bdb->bi_dbenv_home ) {
				c->value_string = ch_strdup( bdb->bi_dbenv_home );
			} else {
				rc = 1;
			}
			break;

		case BDB_CONFIG:
			if ( !( bdb->bi_flags & BDB_IS_OPEN )
				&& !bdb->bi_db_config ) {
				char	buf[SLAP_TEXT_BUFLEN];
				FILE *f = fopen( bdb->bi_db_config_path, "r" );
				struct berval bv;

				if ( f ) {
					bdb->bi_flags |= BDB_HAS_CONFIG;
					while ( fgets( buf, sizeof(buf), f )) {
						ber_str2bv( buf, 0, 1, &bv );
						if ( bv.bv_len > 0 && bv.bv_val[bv.bv_len-1] == '\n' ) {
							bv.bv_len--;
							bv.bv_val[bv.bv_len] = '\0';
						}
						/* shouldn't need this, but ... */
						if ( bv.bv_len > 0 && bv.bv_val[bv.bv_len-1] == '\r' ) {
							bv.bv_len--;
							bv.bv_val[bv.bv_len] = '\0';
						}
						ber_bvarray_add( &bdb->bi_db_config, &bv );
					}
					fclose( f );
				}
			}
			if ( bdb->bi_db_config ) {
				int i;
				struct berval bv;

				bv.bv_val = c->log;
				for (i=0; !BER_BVISNULL(&bdb->bi_db_config[i]); i++) {
					bv.bv_len = sprintf( bv.bv_val, "{%d}%s", i,
						bdb->bi_db_config[i].bv_val );
					value_add_one( &c->rvalue_vals, &bv );
				}
			}
			if ( !c->rvalue_vals ) rc = 1;
			break;

		case BDB_NOSYNC:
			if ( bdb->bi_dbenv_xflags & DB_TXN_NOSYNC )
				c->value_int = 1;
			break;
			
		case BDB_INDEX:
			bdb_attr_index_unparse( bdb, &c->rvalue_vals );
			if ( !c->rvalue_vals ) rc = 1;
			break;

		case BDB_LOCKD:
			rc = 1;
			if ( bdb->bi_lock_detect != DB_LOCK_DEFAULT ) {
				int i;
				for (i=0; !BER_BVISNULL(&bdb_lockd[i].word); i++) {
					if ( bdb->bi_lock_detect == bdb_lockd[i].mask ) {
						value_add_one( &c->rvalue_vals, &bdb_lockd[i].word );
						rc = 0;
						break;
					}
				}
			}
			break;

		case BDB_SSTACK:
			c->value_int = bdb->bi_search_stack_depth;
			break;
		}
		return rc;
	} else if ( c->op == LDAP_MOD_DELETE ) {
		rc = 0;
		switch( c->type ) {
		/* single-valued no-ops */
		case BDB_LOCKD:
		case BDB_SSTACK:
			break;

		case BDB_CHKPT:
			if ( bdb->bi_txn_cp_task ) {
				struct re_s *re = bdb->bi_txn_cp_task;
				bdb->bi_txn_cp_task = NULL;
				if ( ldap_pvt_runqueue_isrunning( &slapd_rq, re ))
					ldap_pvt_runqueue_stoptask( &slapd_rq, re );
				ldap_pvt_runqueue_remove( &slapd_rq, re );
			}
			bdb->bi_txn_cp = 0;
			break;
		case BDB_CONFIG:
			if ( c->valx < 0 ) {
				ber_bvarray_free( bdb->bi_db_config );
				bdb->bi_db_config = NULL;
			} else {
				int i = c->valx;
				ch_free( bdb->bi_db_config[i].bv_val );
				for (; bdb->bi_db_config[i].bv_val; i++)
					bdb->bi_db_config[i] = bdb->bi_db_config[i+1];
			}
			bdb->bi_flags |= BDB_UPD_CONFIG;
			c->cleanup = bdb_cf_cleanup;
			break;
		case BDB_DIRECTORY:
			bdb->bi_flags |= BDB_RE_OPEN;
			bdb->bi_flags ^= BDB_HAS_CONFIG;
			ch_free( bdb->bi_dbenv_home );
			bdb->bi_dbenv_home = NULL;
			ch_free( bdb->bi_db_config_path );
			bdb->bi_db_config_path = NULL;
			c->cleanup = bdb_cf_cleanup;
			ldap_pvt_thread_pool_purgekey( bdb->bi_dbenv );
			break;
		case BDB_NOSYNC:
			bdb->bi_dbenv->set_flags( bdb->bi_dbenv, DB_TXN_NOSYNC, 0 );
			break;
		case BDB_INDEX:
			if ( c->valx == -1 ) {
				int i;

				/* delete all (FIXME) */
				for ( i = 0; i < bdb->bi_nattrs; i++ ) {
					bdb->bi_attrs[i]->ai_indexmask |= BDB_INDEX_DELETING;
				}
				bdb->bi_flags |= BDB_DEL_INDEX;
				c->cleanup = bdb_cf_cleanup;

			} else {
				struct berval bv, def = BER_BVC("default");
				char *ptr;

				for (ptr = c->line; !isspace( *ptr ); ptr++);

				bv.bv_val = c->line;
				bv.bv_len = ptr - bv.bv_val;
				if ( bvmatch( &bv, &def )) {
					bdb->bi_defaultmask = 0;

				} else {
					int i;
					char **attrs;
					char sep;

					sep = bv.bv_val[ bv.bv_len ];
					bv.bv_val[ bv.bv_len ] = '\0';
					attrs = ldap_str2charray( bv.bv_val, "," );

					for ( i = 0; attrs[ i ]; i++ ) {
						AttributeDescription *ad = NULL;
						const char *text;
						AttrInfo *ai;

						slap_str2ad( attrs[ i ], &ad, &text );
						/* if we got here... */
						assert( ad != NULL );

						ai = bdb_attr_mask( bdb, ad );
						/* if we got here... */
						assert( ai != NULL );

						ai->ai_indexmask |= BDB_INDEX_DELETING;
						bdb->bi_flags |= BDB_DEL_INDEX;
						c->cleanup = bdb_cf_cleanup;
					}

					bv.bv_val[ bv.bv_len ] = sep;
					ldap_charray_free( attrs );
				}
			}
			break;
		}
		return rc;
	}

	switch( c->type ) {
	case BDB_CHKPT: {
		long	l;
		bdb->bi_txn_cp = 1;
		if ( lutil_atolx( &l, c->argv[1], 0 ) != 0 ) {
			fprintf( stderr, "%s: "
				"invalid kbyte \"%s\" in \"checkpoint\".\n",
				c->log, c->argv[1] );
			return 1;
		}
		bdb->bi_txn_cp_kbyte = l;
		if ( lutil_atolx( &l, c->argv[2], 0 ) != 0 ) {
			fprintf( stderr, "%s: "
				"invalid minutes \"%s\" in \"checkpoint\".\n",
				c->log, c->argv[2] );
			return 1;
		}
		bdb->bi_txn_cp_min = l;
		/* If we're in server mode and time-based checkpointing is enabled,
		 * submit a task to perform periodic checkpoints.
		 */
		if ((slapMode & SLAP_SERVER_MODE) && bdb->bi_txn_cp_min ) {
			struct re_s *re = bdb->bi_txn_cp_task;
			if ( re ) {
				re->interval.tv_sec = bdb->bi_txn_cp_min * 60;
			} else {
				if ( c->be->be_suffix == NULL || BER_BVISNULL( &c->be->be_suffix[0] ) ) {
					fprintf( stderr, "%s: "
						"\"checkpoint\" must occur after \"suffix\".\n",
						c->log );
					return 1;
				}
				bdb->bi_txn_cp_task = ldap_pvt_runqueue_insert( &slapd_rq,
					bdb->bi_txn_cp_min * 60, bdb_checkpoint, bdb,
					LDAP_XSTRING(bdb_checkpoint), c->be->be_suffix[0].bv_val );
			}
		}
		} break;

	case BDB_CONFIG: {
		char *ptr = c->line;
		struct berval bv;

		if ( c->op == SLAP_CONFIG_ADD ) {
			ptr += STRLENOF("dbconfig");
			while (!isspace(*ptr)) ptr++;
			while (isspace(*ptr)) ptr++;
		}

		if ( bdb->bi_flags & BDB_IS_OPEN ) {
			bdb->bi_flags |= BDB_UPD_CONFIG;
			c->cleanup = bdb_cf_cleanup;
		} else {
		/* If we're just starting up...
		 */
			FILE *f;
			/* If a DB_CONFIG file exists, or we don't know the path
			 * to the DB_CONFIG file, ignore these directives
			 */
			if (( bdb->bi_flags & BDB_HAS_CONFIG ) || !bdb->bi_db_config_path )
				break;
			f = fopen( bdb->bi_db_config_path, "a" );
			if ( f ) {
				/* FIXME: EBCDIC probably needs special handling */
				fprintf( f, "%s\n", ptr );
				fclose( f );
			}
		}
		ber_str2bv( ptr, 0, 1, &bv );
		ber_bvarray_add( &bdb->bi_db_config, &bv );
		}
		break;

	case BDB_DIRECTORY: {
		FILE *f;
		char *ptr;

		if ( bdb->bi_dbenv_home )
			ch_free( bdb->bi_dbenv_home );
		bdb->bi_dbenv_home = c->value_string;

		/* See if a DB_CONFIG file already exists here */
		if ( bdb->bi_db_config_path )
			ch_free( bdb->bi_db_config_path );
		bdb->bi_db_config_path = ch_malloc( strlen( bdb->bi_dbenv_home ) +
			STRLENOF(LDAP_DIRSEP) + STRLENOF("DB_CONFIG") + 1 );
		ptr = lutil_strcopy( bdb->bi_db_config_path, bdb->bi_dbenv_home );
		*ptr++ = LDAP_DIRSEP[0];
		strcpy( ptr, "DB_CONFIG" );

		f = fopen( bdb->bi_db_config_path, "r" );
		if ( f ) {
			bdb->bi_flags |= BDB_HAS_CONFIG;
			fclose(f);
		}
		}
		break;

	case BDB_NOSYNC:
		if ( c->value_int )
			bdb->bi_dbenv_xflags |= DB_TXN_NOSYNC;
		else
			bdb->bi_dbenv_xflags &= ~DB_TXN_NOSYNC;
		if ( bdb->bi_flags & BDB_IS_OPEN ) {
			bdb->bi_dbenv->set_flags( bdb->bi_dbenv, DB_TXN_NOSYNC,
				c->value_int );
		}
		break;

	case BDB_INDEX:
		rc = bdb_attr_index_config( bdb, c->fname, c->lineno,
			c->argc - 1, &c->argv[1] );

		if( rc != LDAP_SUCCESS ) return 1;
		if (( bdb->bi_flags & BDB_IS_OPEN ) && !bdb->bi_index_task ) {
			/* Start the task as soon as we finish here. Set a long
			 * interval (10 hours) so that it only gets scheduled once.
			 */
			if ( c->be->be_suffix == NULL || BER_BVISNULL( &c->be->be_suffix[0] ) ) {
				fprintf( stderr, "%s: "
					"\"index\" must occur after \"suffix\".\n",
					c->log );
				return 1;
			}
			bdb->bi_index_task = ldap_pvt_runqueue_insert( &slapd_rq, 36000,
				bdb_online_index, c->be,
				LDAP_XSTRING(bdb_online_index), c->be->be_suffix[0].bv_val );
		}
		break;

	case BDB_LOCKD:
		rc = verb_to_mask( c->argv[1], bdb_lockd );
		if ( BER_BVISNULL(&bdb_lockd[rc].word) ) {
			fprintf( stderr, "%s: "
				"bad policy (%s) in \"lockDetect <policy>\" line\n",
				c->log, c->argv[1] );
			return 1;
		}
		bdb->bi_lock_detect = rc;
		break;

	case BDB_SSTACK:
		if ( c->value_int < MINIMUM_SEARCH_STACK_DEPTH ) {
			fprintf( stderr,
		"%s: depth %d too small, using %d\n",
			c->log, c->value_int, MINIMUM_SEARCH_STACK_DEPTH );
			c->value_int = MINIMUM_SEARCH_STACK_DEPTH;
		}
		bdb->bi_search_stack_depth = c->value_int;
		break;
	}
	return 0;
}

int bdb_back_init_cf( BackendInfo *bi )
{
	int rc;
	bi->bi_cf_ocs = bdbocs;

	rc = config_register_schema( bdbcfg, bdbocs );
	if ( rc ) return rc;
	return 0;
}
