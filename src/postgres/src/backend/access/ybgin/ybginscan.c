/*--------------------------------------------------------------------------
 *
 * ybginscan.c
 *	  routines to manage scans of Yugabyte inverted index relations
 *
 * Copyright (c) YugaByte, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * IDENTIFICATION
 *			src/backend/access/ybgin/ybginscan.c
 *--------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/yb_scan.h"
#include "access/ybgin.h"
#include "access/ybgin_private.h"
#include "commands/tablegroup.h"
#include "miscadmin.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/rel.h"

#include "pg_yb_utils.h"
#include "yb/yql/pggate/ybc_pggate.h"

/*
 * Parts copied from ginscan.c ginbeginscan.  Do the same thing except
 * - palloc size of YbginScanOpaqueData
 * - name memory contexts Ybgin
 */
IndexScanDesc
ybginbeginscan(Relation rel, int nkeys, int norderbys)
{
	IndexScanDesc scan;
	GinScanOpaque so;

	/* no order by operators allowed */
	Assert(norderbys == 0);

	scan = RelationGetIndexScan(rel, nkeys, norderbys);

	/* allocate private workspace */
	so = (GinScanOpaque) palloc(sizeof(YbginScanOpaqueData));
	so->keys = NULL;
	so->nkeys = 0;
	so->tempCtx = AllocSetContextCreate(GetCurrentMemoryContext(),
										"Ybgin scan temporary context",
										ALLOCSET_DEFAULT_SIZES);
	so->keyCtx = AllocSetContextCreate(GetCurrentMemoryContext(),
									   "Ybgin scan key context",
									   ALLOCSET_DEFAULT_SIZES);
	initGinState(&so->ginstate, scan->indexRelation);

	scan->opaque = so;

	return scan;
}

void
ybginrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys,
			ScanKey orderbys, int norderbys)
{
	YbginScanOpaque	  ybso = (YbginScanOpaque) scan->opaque;
	YbTableProperties yb_table_properties_relation =
		YbGetTableProperties(scan->heapRelation);
	YbTableProperties yb_table_properties_index =
		YbGetTableProperties(scan->indexRelation);
	bool querying_colocated_table = false;
	bool is_colocated = yb_table_properties_relation->is_colocated;
	bool is_colocated_tables_with_tablespace_enabled =
		*YBCGetGFlags()->ysql_enable_colocated_tables_with_tablespaces;

	/* Initialize non-yb gin scan opaque fields. */
	ginrescan(scan, scankey, nscankeys, orderbys, norderbys);

	if (!is_colocated_tables_with_tablespace_enabled)
	{
		querying_colocated_table = is_colocated;
	}
	else
	{
		querying_colocated_table =
			is_colocated && yb_table_properties_index->tablegroup_oid ==
								yb_table_properties_relation->tablegroup_oid;
	}

	/* Initialize ybgin scan opaque handle. */
	YBCPgPrepareParameters prepare_params = {
		.index_relfilenode_oid = YbGetRelfileNodeId(scan->indexRelation),
		.index_only_scan = scan->xs_want_itup,
		.querying_colocated_table = querying_colocated_table,
	};
	HandleYBStatus(YBCPgNewSelect(YBCGetDatabaseOid(scan->heapRelation),
								  YbGetRelfileNodeId(scan->heapRelation),
								  &prepare_params,
								  YBCIsRegionLocal(scan->heapRelation),
								  &ybso->handle));

	/* Add any pushdown expression to the main table scan. */
	if (scan->yb_rel_pushdown != NULL)
	{
		YbDmlAppendQuals(scan->yb_rel_pushdown->quals,
						 true /* is_primary */,
						 ybso->handle);
		YbDmlAppendColumnRefs(scan->yb_rel_pushdown->colrefs,
							  true /* is_primary */,
							  ybso->handle);
	}

	/* Add any pushdown expression to the index relation scan. */
	if (scan->yb_idx_pushdown != NULL)
	{
		YbDmlAppendQuals(scan->yb_idx_pushdown->quals,
						 false /* is_primary */,
						 ybso->handle);
		YbDmlAppendColumnRefs(scan->yb_idx_pushdown->colrefs,
							  false /* is_primary */,
							  ybso->handle);
	}

	/* Initialize ybgin scan opaque is_exec_done. */
	ybso->is_exec_done = false;
}

void
ybginendscan(IndexScanDesc scan)
{
	/*
	 * This frees the scan opaque as if it were a GinScanOpaque rather than a
	 * YbginScanOpaque, but that's fine since the extra field HANDLE doesn't
	 * need special handling.
	 */
	ginendscan(scan);
}
