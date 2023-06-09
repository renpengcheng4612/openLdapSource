/* init.c - initialize bdb backend */
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
#include <ac/string.h>
#include <ac/unistd.h>
#include <ac/stdlib.h>
#include <ac/errno.h>
#include <sys/stat.h>
#include "back-bdb.h"
#include <lutil.h>
#include <ldap_rq.h>
#include "alock.h"

static const struct bdbi_database {
	char *file;
	char *name;
	int type;
	int flags;
} bdbi_databases[] = {
	{ "id2entry" BDB_SUFFIX, "id2entry", DB_BTREE, 0 },
	{ "dn2id" BDB_SUFFIX, "dn2id", DB_BTREE, 0 },
	{ NULL, NULL, 0, 0 }
};

typedef void * db_malloc(size_t);
typedef void * db_realloc(void *, size_t);

#define bdb_db_init	BDB_SYMBOL(db_init)
#define bdb_db_open	BDB_SYMBOL(db_open)
#define bdb_db_close	BDB_SYMBOL(db_close)

static int
bdb_db_init( BackendDB *be )
{
	struct bdb_info	*bdb;

	Debug( LDAP_DEBUG_TRACE,
		LDAP_XSTRING(bdb_db_init) ": Initializing " BDB_UCTYPE " database\n",
		0, 0, 0 );

	/* allocate backend-database-specific stuff */
	bdb = (struct bdb_info *) ch_calloc( 1, sizeof(struct bdb_info) );

	/* DBEnv parameters */
	bdb->bi_dbenv_home = ch_strdup( SLAPD_DEFAULT_DB_DIR );
	bdb->bi_dbenv_xflags = 0;
	bdb->bi_dbenv_mode = SLAPD_DEFAULT_DB_MODE;

	bdb->bi_cache.c_maxsize = DEFAULT_CACHE_SIZE;
	bdb->bi_cache.c_minfree = 1;

	bdb->bi_lock_detect = DB_LOCK_DEFAULT;
	bdb->bi_search_stack_depth = DEFAULT_SEARCH_STACK_DEPTH;
	bdb->bi_search_stack = NULL;

	ldap_pvt_thread_mutex_init( &bdb->bi_database_mutex );
	ldap_pvt_thread_mutex_init( &bdb->bi_lastid_mutex );
#ifdef BDB_HIER
	ldap_pvt_thread_mutex_init( &bdb->bi_modrdns_mutex );
#endif
	ldap_pvt_thread_mutex_init( &bdb->bi_cache.lru_head_mutex );
	ldap_pvt_thread_mutex_init( &bdb->bi_cache.lru_tail_mutex );
	ldap_pvt_thread_mutex_init( &bdb->bi_cache.c_dntree.bei_kids_mutex );
	ldap_pvt_thread_rdwr_init ( &bdb->bi_cache.c_rwlock );
	ldap_pvt_thread_rdwr_init( &bdb->bi_idl_tree_rwlock );
	ldap_pvt_thread_mutex_init( &bdb->bi_idl_tree_lrulock );

	be->be_private = bdb;
	be->be_cf_ocs = be->bd_info->bi_cf_ocs;

	return 0;
}

static int
bdb_db_close( BackendDB *be );

static int
bdb_db_open( BackendDB *be )
{
	int rc, i;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	struct stat stat1, stat2;
	u_int32_t flags;
	char path[MAXPATHLEN];
	char *dbhome;
	int do_recover = 0, do_alock_recover = 0, open_env = 1;
	int alockt, quick = 0;

	if ( be->be_suffix == NULL ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": need suffix\n",
			0, 0, 0 );
		return -1;
	}

	Debug( LDAP_DEBUG_ARGS,
		LDAP_XSTRING(bdb_db_open) ": %s\n",
		be->be_suffix[0].bv_val, 0, 0 );

#ifndef BDB_MULTIPLE_SUFFIXES
	if ( be->be_suffix[1].bv_val ) {
	Debug( LDAP_DEBUG_ANY,
		LDAP_XSTRING(bdb_db_open) ": only one suffix allowed\n", 0, 0, 0 );
		return -1;
	}
#endif

	/* Check existence of dbenv_home. Any error means trouble */
	rc = stat( bdb->bi_dbenv_home, &stat1 );
	if( rc !=0 ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": Cannot access database directory %s (%d)\n",
			bdb->bi_dbenv_home, errno, 0 );
			return -1;
	}

	/* Perform database use arbitration/recovery logic */
	alockt = (slapMode & SLAP_TOOL_READONLY) ? ALOCK_LOCKED : ALOCK_UNIQUE;
	if ( slapMode & SLAP_TOOL_QUICK ) {
		alockt |= ALOCK_NOSAVE;
		quick = 1;
	}

	rc = alock_open( &bdb->bi_alock_info, 
				"slapd", 
				bdb->bi_dbenv_home, alockt );

	/* alockt is TRUE if the existing environment was created in Quick mode */
	alockt = (rc & ALOCK_NOSAVE) ? 1 : 0;
	rc &= ~ALOCK_NOSAVE;

	if( rc == ALOCK_RECOVER ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": unclean shutdown detected;"
			" attempting recovery.\n", 
			0, 0, 0 );
		do_alock_recover = 1;
		do_recover = DB_RECOVER;
	} else if( rc == ALOCK_BUSY ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": database already in use\n", 
			0, 0, 0 );
		return -1;
	} else if( rc != ALOCK_CLEAN ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": alock package is unstable\n", 
			0, 0, 0 );
		return -1;
	}

	/*
	 * The DB_CONFIG file may have changed. If so, recover the
	 * database so that new settings are put into effect. Also
	 * note the possible absence of DB_CONFIG in the log.
	 */
	if( stat( bdb->bi_db_config_path, &stat1 ) == 0 ) {
		if ( !do_recover ) {
			char *ptr = lutil_strcopy(path, bdb->bi_dbenv_home);
			*ptr++ = LDAP_DIRSEP[0];
			strcpy( ptr, "__db.001" );
			if( stat( path, &stat2 ) == 0 ) {
				if( stat2.st_mtime < stat1.st_mtime ) {
					Debug( LDAP_DEBUG_ANY,
						LDAP_XSTRING(bdb_db_open) ": DB_CONFIG for suffix %s has changed.\n"
						"Performing database recovery to activate new settings.\n",
						be->be_suffix[0].bv_val, 0, 0 );
					do_recover = DB_RECOVER;
				}
			}
		}
	}
	else {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": Warning - No DB_CONFIG file found "
			"in directory %s: (%d)\n"
			"Expect poor performance for suffix %s.\n",
			bdb->bi_dbenv_home, errno, be->be_suffix[0].bv_val );
	}

	/* Always let slapcat run, regardless of environment state.
	 * This can be used to cause a cache flush after an unclean
	 * shutdown.
	 */
	if ( do_recover && ( slapMode & SLAP_TOOL_READONLY )) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": Recovery skipped in read-only mode. "
			"Run manual recovery if errors are encountered.\n",
			0, 0, 0 );
		do_recover = 0;
		quick = alockt;
	}

	/* An existing environment in Quick mode has nothing to recover. */
	if ( alockt && do_recover ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": cannot recover, database must be reinitialized.\n", 
			0, 0, 0 );
		rc = -1;
		goto fail;
	}

	rc = db_env_create( &bdb->bi_dbenv, 0 );
	if( rc != 0 ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": db_env_create failed: %s (%d)\n",
			db_strerror(rc), rc, 0 );
		goto fail;
	}

#ifdef HAVE_EBCDIC
	strcpy( path, bdb->bi_dbenv_home );
	__atoe( path );
	dbhome = path;
#else
	dbhome = bdb->bi_dbenv_home;
#endif

	/* If existing environment is clean but doesn't support
	 * currently requested modes, remove it.
	 */
	if ( !do_recover && ( alockt ^ quick )) {
shm_retry:
		rc = bdb->bi_dbenv->remove( bdb->bi_dbenv, dbhome, DB_FORCE );
		if ( rc ) {
			Debug( LDAP_DEBUG_ANY,
				LDAP_XSTRING(bdb_db_open) ": dbenv remove failed: %s (%d)\n",
				db_strerror(rc), rc, 0 );
			bdb->bi_dbenv = NULL;
			goto fail;
		}
		rc = db_env_create( &bdb->bi_dbenv, 0 );
		if( rc != 0 ) {
			Debug( LDAP_DEBUG_ANY,
				LDAP_XSTRING(bdb_db_open) ": db_env_create failed: %s (%d)\n",
				db_strerror(rc), rc, 0 );
			goto fail;
		}
	}

	bdb->bi_dbenv->set_errpfx( bdb->bi_dbenv, be->be_suffix[0].bv_val );
	bdb->bi_dbenv->set_errcall( bdb->bi_dbenv, bdb_errcall );

	bdb->bi_dbenv->set_lk_detect( bdb->bi_dbenv, bdb->bi_lock_detect );

	/* One long-lived TXN per thread, two TXNs per write op */
	bdb->bi_dbenv->set_tx_max( bdb->bi_dbenv, connection_pool_max * 3 );

	if( bdb->bi_dbenv_xflags != 0 ) {
		rc = bdb->bi_dbenv->set_flags( bdb->bi_dbenv,
			bdb->bi_dbenv_xflags, 1);
		if( rc != 0 ) {
			Debug( LDAP_DEBUG_ANY,
				LDAP_XSTRING(bdb_db_open) ": dbenv_set_flags failed: %s (%d)\n",
				db_strerror(rc), rc, 0 );
			goto fail;
		}
	}

#define	BDB_TXN_FLAGS	(DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN)

	Debug( LDAP_DEBUG_TRACE,
		LDAP_XSTRING(bdb_db_open) ": dbenv_open(%s)\n",
		bdb->bi_dbenv_home, 0, 0);

	flags = DB_INIT_MPOOL | DB_CREATE | DB_THREAD;

	if ( !quick )
		flags |= BDB_TXN_FLAGS;

	/* If a key was set, use shared memory for the BDB environment */
	if ( bdb->bi_shm_key ) {
		bdb->bi_dbenv->set_shm_key( bdb->bi_dbenv, bdb->bi_shm_key );
		flags |= DB_SYSTEM_MEM;
	}
	rc = (bdb->bi_dbenv->open)( bdb->bi_dbenv, dbhome,
			flags | do_recover, bdb->bi_dbenv_mode );

	if ( rc ) {
		/* Regular open failed, probably a missing shm environment.
		 * Start over, do a recovery.
		 */
		if ( !do_recover && bdb->bi_shm_key ) {
			bdb->bi_dbenv->close( bdb->bi_dbenv, 0 );
			rc = db_env_create( &bdb->bi_dbenv, 0 );
			if( rc == 0 ) {
				Debug( LDAP_DEBUG_ANY, LDAP_XSTRING(bdb_db_open)
					": Shared memory env open failed, assuming stale env\n",
					0, 0, 0 );
				goto shm_retry;
			}
		}
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": Database cannot be %s, err %d. "
			"Restore from backup!\n",
				do_recover ? "recovered" : "opened", rc, 0);
		goto fail;
	}

	if ( do_alock_recover && alock_recover (&bdb->bi_alock_info) != 0 ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": alock_recover failed\n",
			0, 0, 0 );
		rc = -1;
		goto fail;
	}

#ifdef SLAP_ZONE_ALLOC
	if ( bdb->bi_cache.c_maxsize ) {
		bdb->bi_cache.c_zctx = slap_zn_mem_create(
								SLAP_ZONE_INITSIZE,
								SLAP_ZONE_MAXSIZE,
								SLAP_ZONE_DELTA,
								SLAP_ZONE_SIZE);
	}
#endif

	if ( bdb->bi_idl_cache_max_size ) {
		bdb->bi_idl_tree = NULL;
		bdb->bi_idl_cache_size = 0;
	}

	flags = DB_THREAD | bdb->bi_db_opflags;

#ifdef DB_AUTO_COMMIT
	if ( !quick )
		flags |= DB_AUTO_COMMIT;
#endif

	bdb->bi_databases = (struct bdb_db_info **) ch_malloc(
		BDB_INDICES * sizeof(struct bdb_db_info *) );

	/* open (and create) main database */
	for( i = 0; bdbi_databases[i].name; i++ ) {
		struct bdb_db_info *db;

		db = (struct bdb_db_info *) ch_calloc(1, sizeof(struct bdb_db_info));

		rc = db_create( &db->bdi_db, bdb->bi_dbenv, 0 );
		if( rc != 0 ) {
			Debug( LDAP_DEBUG_ANY,
				LDAP_XSTRING(bdb_db_open) ": db_create(%s) failed: %s (%d)\n",
				bdb->bi_dbenv_home, db_strerror(rc), rc );
			goto fail;
		}

		if( i == BDB_ID2ENTRY ) {
			if ( slapMode & SLAP_TOOL_MODE )
				db->bdi_db->mpf->set_priority( db->bdi_db->mpf,
					DB_PRIORITY_VERY_LOW );

			rc = db->bdi_db->set_pagesize( db->bdi_db,
				BDB_ID2ENTRY_PAGESIZE );
			if ( slapMode & SLAP_TOOL_READMAIN ) {
				flags |= DB_RDONLY;
			} else {
				flags |= DB_CREATE;
			}
		} else {
			rc = db->bdi_db->set_flags( db->bdi_db, 
				DB_DUP | DB_DUPSORT );
#ifndef BDB_HIER
			if ( slapMode & SLAP_TOOL_READONLY ) {
				flags |= DB_RDONLY;
			} else {
				flags |= DB_CREATE;
			}
#else
			rc = db->bdi_db->set_dup_compare( db->bdi_db,
				bdb_dup_compare );
			if ( slapMode & (SLAP_TOOL_READONLY|SLAP_TOOL_READMAIN) ) {
				flags |= DB_RDONLY;
			} else {
				flags |= DB_CREATE;
			}
#endif
			rc = db->bdi_db->set_pagesize( db->bdi_db,
				BDB_PAGESIZE );
		}

#ifdef HAVE_EBCDIC
		strcpy( path, bdbi_databases[i].file );
		__atoe( path );
		rc = DB_OPEN( db->bdi_db,
			path,
		/*	bdbi_databases[i].name, */ NULL,
			bdbi_databases[i].type,
			bdbi_databases[i].flags | flags,
			bdb->bi_dbenv_mode );
#else
		rc = DB_OPEN( db->bdi_db,
			bdbi_databases[i].file,
		/*	bdbi_databases[i].name, */ NULL,
			bdbi_databases[i].type,
			bdbi_databases[i].flags | flags,
			bdb->bi_dbenv_mode );
#endif

		if ( rc != 0 ) {
			char	buf[SLAP_TEXT_BUFLEN];

			snprintf( buf, sizeof(buf), "%s/%s", 
				bdb->bi_dbenv_home, bdbi_databases[i].file );
			Debug( LDAP_DEBUG_ANY,
				LDAP_XSTRING(bdb_db_open) ": db_open(%s) failed: %s (%d)\n",
				buf, db_strerror(rc), rc );
			db->bdi_db->close( db->bdi_db, 0 );
			goto fail;
		}

		flags &= ~(DB_CREATE | DB_RDONLY);
		db->bdi_name = bdbi_databases[i].name;
		bdb->bi_databases[i] = db;
	}

	bdb->bi_databases[i] = NULL;
	bdb->bi_ndatabases = i;

	/* get nextid */
	rc = bdb_last_id( be, NULL );
	if( rc != 0 ) {
		Debug( LDAP_DEBUG_ANY,
			LDAP_XSTRING(bdb_db_open) ": last_id(%s) failed: %s (%d)\n",
			bdb->bi_dbenv_home, db_strerror(rc), rc );
		goto fail;
	}

	if ( !quick ) {
		XLOCK_ID(bdb->bi_dbenv, &bdb->bi_cache.c_locker);
	}

	bdb->bi_flags |= BDB_IS_OPEN;

	return 0;

fail:
	bdb_db_close( be );
	return rc;
}

static int
bdb_db_close( BackendDB *be )
{
	int rc;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	struct bdb_db_info *db;
	bdb_idl_cache_entry_t *entry, *next_entry;

	bdb->bi_flags &= ~BDB_IS_OPEN;

	ber_bvarray_free( bdb->bi_db_config );
	bdb->bi_db_config = NULL;

	while( bdb->bi_databases && bdb->bi_ndatabases-- ) {
		db = bdb->bi_databases[bdb->bi_ndatabases];
		rc = db->bdi_db->close( db->bdi_db, 0 );
		/* Lower numbered names are not strdup'd */
		if( bdb->bi_ndatabases >= BDB_NDB )
			free( db->bdi_name );
		free( db );
	}
	free( bdb->bi_databases );
	bdb->bi_databases = NULL;

	bdb_cache_release_all (&bdb->bi_cache);

	if ( bdb->bi_idl_cache_max_size ) {
		avl_free( bdb->bi_idl_tree, NULL );
		bdb->bi_idl_tree = NULL;
		entry = bdb->bi_idl_lru_head;
		while ( entry != NULL ) {
			next_entry = entry->idl_lru_next;
			if ( entry->idl )
				free( entry->idl );
			free( entry->kstr.bv_val );
			free( entry );
			entry = next_entry;
		}
		bdb->bi_idl_lru_head = bdb->bi_idl_lru_tail = NULL;
	}

	/* close db environment */
	if( bdb->bi_dbenv ) {
		/* Free cache locker if we enabled locking */
		if ( !( slapMode & SLAP_TOOL_QUICK )) {
			XLOCK_ID_FREE(bdb->bi_dbenv, bdb->bi_cache.c_locker);
			bdb->bi_cache.c_locker = 0;
		}
#ifdef BDB_REUSE_LOCKERS
		bdb_locker_flush( bdb->bi_dbenv );
#endif
		/* force a checkpoint, but not if we were ReadOnly,
		 * and not in Quick mode since there are no transactions there.
		 */
		if ( !( slapMode & ( SLAP_TOOL_QUICK|SLAP_TOOL_READONLY ))) {
			rc = TXN_CHECKPOINT( bdb->bi_dbenv, 0, 0, DB_FORCE );
			if( rc != 0 ) {
				Debug( LDAP_DEBUG_ANY,
					"bdb_db_close: txn_checkpoint failed: %s (%d)\n",
					db_strerror(rc), rc, 0 );
			}
		}

		rc = bdb->bi_dbenv->close( bdb->bi_dbenv, 0 );
		bdb->bi_dbenv = NULL;
		if( rc != 0 ) {
			Debug( LDAP_DEBUG_ANY,
				"bdb_db_close: close failed: %s (%d)\n",
				db_strerror(rc), rc, 0 );
			return rc;
		}
	}

	rc = alock_close( &bdb->bi_alock_info );
	if( rc != 0 ) {
		Debug( LDAP_DEBUG_ANY,
			"bdb_db_close: alock_close failed\n", 0, 0, 0 );
		return -1;
	}

	return 0;
}

static int
bdb_db_destroy( BackendDB *be )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;

	if( bdb->bi_dbenv_home ) ch_free( bdb->bi_dbenv_home );
	if( bdb->bi_db_config_path ) ch_free( bdb->bi_db_config_path );

	bdb_attr_index_destroy( bdb );

	ldap_pvt_thread_rdwr_destroy ( &bdb->bi_cache.c_rwlock );
	ldap_pvt_thread_mutex_destroy( &bdb->bi_cache.lru_head_mutex );
	ldap_pvt_thread_mutex_destroy( &bdb->bi_cache.lru_tail_mutex );
	ldap_pvt_thread_mutex_destroy( &bdb->bi_cache.c_dntree.bei_kids_mutex );
#ifdef BDB_HIER
	ldap_pvt_thread_mutex_destroy( &bdb->bi_modrdns_mutex );
#endif
	ldap_pvt_thread_mutex_destroy( &bdb->bi_lastid_mutex );
	ldap_pvt_thread_mutex_destroy( &bdb->bi_database_mutex );
	ldap_pvt_thread_rdwr_destroy( &bdb->bi_idl_tree_rwlock );
	ldap_pvt_thread_mutex_destroy( &bdb->bi_idl_tree_lrulock );

	ch_free( bdb );
	be->be_private = NULL;

	return 0;
}

int
bdb_back_initialize(
	BackendInfo	*bi )
{
	int rc;

	static char *controls[] = {
		LDAP_CONTROL_ASSERT,
		LDAP_CONTROL_MANAGEDSAIT,
		LDAP_CONTROL_NOOP,
		LDAP_CONTROL_PAGEDRESULTS,
		LDAP_CONTROL_PRE_READ,
		LDAP_CONTROL_POST_READ,
		LDAP_CONTROL_SUBENTRIES,
		LDAP_CONTROL_X_PERMISSIVE_MODIFY,
		NULL
	};

	/* initialize the underlying database system */
	Debug( LDAP_DEBUG_TRACE,
		LDAP_XSTRING(bdb_back_initialize) ": initialize " 
		BDB_UCTYPE " backend\n", 0, 0, 0 );

	bi->bi_flags |=
		SLAP_BFLAG_INCREMENT |
		SLAP_BFLAG_SUBENTRIES |
		SLAP_BFLAG_ALIASES |
		SLAP_BFLAG_REFERRALS;

	bi->bi_controls = controls;

	{	/* version check */
		int major, minor, patch, ver;
		char *version = db_version( &major, &minor, &patch );
#ifdef HAVE_EBCDIC
		char v2[1024];

		/* All our stdio does an ASCII to EBCDIC conversion on
		 * the output. Strings from the BDB library are already
		 * in EBCDIC; we have to go back and forth...
		 */
		strcpy( v2, version );
		__etoa( v2 );
		version = v2;
#endif

		ver = (major << 24) | (minor << 16) | patch;
		if( ver != DB_VERSION_FULL ) {
			/* fail if a versions don't match */
			Debug( LDAP_DEBUG_ANY,
				LDAP_XSTRING(bdb_back_initialize) ": "
				"BDB library version mismatch:"
				" expected " DB_VERSION_STRING ","
				" got %s\n", version, 0, 0 );
			return -1;
		}

		Debug( LDAP_DEBUG_TRACE, LDAP_XSTRING(bdb_back_initialize)
			": %s\n", version, 0, 0 );
	}

	db_env_set_func_free( ber_memfree );
	db_env_set_func_malloc( (db_malloc *)ber_memalloc );
	db_env_set_func_realloc( (db_realloc *)ber_memrealloc );
#ifndef NO_THREAD
	/* This is a no-op on a NO_THREAD build. Leave the default
	 * alone so that BDB will sleep on interprocess conflicts.
	 */
	db_env_set_func_yield( ldap_pvt_thread_yield );
#endif

	bi->bi_open = 0;
	bi->bi_close = 0;
	bi->bi_config = 0;
	bi->bi_destroy = 0;

	bi->bi_db_init = bdb_db_init;
	bi->bi_db_config = config_generic_wrapper;
	bi->bi_db_open = bdb_db_open;
	bi->bi_db_close = bdb_db_close;
	bi->bi_db_destroy = bdb_db_destroy;

	bi->bi_op_add = bdb_add;
	bi->bi_op_bind = bdb_bind;
	bi->bi_op_compare = bdb_compare;
	bi->bi_op_delete = bdb_delete;
	bi->bi_op_modify = bdb_modify;
	bi->bi_op_modrdn = bdb_modrdn;
	bi->bi_op_search = bdb_search;

	bi->bi_op_unbind = 0;

	bi->bi_extended = bdb_extended;

	bi->bi_chk_referrals = bdb_referrals;
	bi->bi_operational = bdb_operational;
	bi->bi_has_subordinates = bdb_hasSubordinates;
	bi->bi_entry_release_rw = bdb_entry_release;
	bi->bi_entry_get_rw = bdb_entry_get;

	/*
	 * hooks for slap tools
	 */
	bi->bi_tool_entry_open = bdb_tool_entry_open;
	bi->bi_tool_entry_close = bdb_tool_entry_close;
	bi->bi_tool_entry_first = bdb_tool_entry_next;
	bi->bi_tool_entry_next = bdb_tool_entry_next;
	bi->bi_tool_entry_get = bdb_tool_entry_get;
	bi->bi_tool_entry_put = bdb_tool_entry_put;
	bi->bi_tool_entry_reindex = bdb_tool_entry_reindex;
	bi->bi_tool_sync = 0;
	bi->bi_tool_dn2id_get = bdb_tool_dn2id_get;
	bi->bi_tool_id2entry_get = bdb_tool_id2entry_get;
	bi->bi_tool_entry_modify = bdb_tool_entry_modify;

	bi->bi_connection_init = 0;
	bi->bi_connection_destroy = 0;

	rc = bdb_back_init_cf( bi );

	return rc;
}

#if	(SLAPD_BDB == SLAPD_MOD_DYNAMIC && !defined(BDB_HIER)) || \
	(SLAPD_HDB == SLAPD_MOD_DYNAMIC && defined(BDB_HIER))

/* conditionally define the init_module() function */
#ifdef BDB_HIER
SLAP_BACKEND_INIT_MODULE( hdb )
#else /* !BDB_HIER */
SLAP_BACKEND_INIT_MODULE( bdb )
#endif /* !BDB_HIER */

#endif /* SLAPD_[BH]DB == SLAPD_MOD_DYNAMIC */

